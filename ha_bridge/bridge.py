"""Allowlisted HA adapter for CottageThermoESP; no furnace-control endpoints.

Run on a local PC or alongside the ship tracker in Proxmox. HA credentials and
camera decoding stay on the host. The same compact API serves the future LCD.
"""
from __future__ import annotations
import argparse
from collections import OrderedDict
import copy
from dataclasses import dataclass
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import json
import math
import os
from pathlib import Path
import secrets
import threading
import time
from urllib.parse import urlsplit

import requests

ROOT = Path(__file__).resolve().parent
ACTION_SERVICES = {
    "light": {"on": "turn_on", "off": "turn_off"},
    "switch": {"on": "turn_on", "off": "turn_off"},
    "cover": {"open": "open_cover", "stop": "stop_cover", "close": "close_cover"},
    "vacuum": {"start": "start", "stop": "stop"},
}
# HA CoverEntityFeature.OPEN/CLOSE/STOP and VacuumEntityFeature.START/STOP.
FEATURES = {("cover", "open"): 1, ("cover", "close"): 2, ("cover", "stop"): 8,
            ("vacuum", "start"): 8192, ("vacuum", "stop"): 8}
UNKNOWN = {"unknown", "unavailable", "none", ""}

class BridgeError(Exception):
    def __init__(self, status: int, message: str):
        super().__init__(message)
        self.status = status

@dataclass(frozen=True)
class Settings:
    ha_url: str
    ha_token: str
    bridge_token: str
    entities: dict
    ca_file: str | None = None
    poll_seconds: float = 5
    ttl_seconds: float = 30

    @classmethod
    def load(cls, path: Path = ROOT / "entities.json"):
        data = json.loads(path.read_text(encoding="utf-8"))
        url = os.environ.get("COTTAGE_HA_URL", data["ha_url"]).rstrip("/")
        parsed = urlsplit(url)
        # Credentials always travel to a verified HTTPS origin; redirects are rejected.
        if parsed.scheme != "https" or not parsed.hostname or parsed.username or parsed.password or parsed.query or parsed.fragment or parsed.path not in ("", "/"):
            raise ValueError("Home Assistant URL must be an HTTPS origin without credentials/path")
        expected = {"light": "light", "switch": "input_boolean", "cover": "cover", "vacuum": "vacuum",
                    "laundry": "sensor", "temperature": "sensor", "camera": "camera"}
        for spec in data["entities"].values():
            entity = spec["entity_id"]
            if spec["kind"] not in expected or not entity.startswith(expected[spec["kind"]]+".") or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789_." for c in entity):
                raise ValueError("Invalid entity mapping")
        key = os.environ.get("COTTAGE_BRIDGE_TOKEN") or secrets.token_hex(16)
        if len(key) < 32 or any(c.isspace() for c in key):
            raise ValueError("Bridge token must contain at least 32 non-whitespace characters")
        return cls(url, os.environ.get("HA_TOKEN", ""), key, data["entities"], os.environ.get("COTTAGE_CA_FILE"))

class HAClient:
    def __init__(self, settings: Settings):
        self.settings = settings
        self.local = threading.local()

    def request(self, path: str, payload=None, binary=False):
        if not self.settings.ha_token:
            raise BridgeError(503, "Home Assistant token is not configured")
        session = getattr(self.local, "session", None)
        if session is None:
            session = self.local.session = requests.Session()
        try:
            # No automatic mutation retries; an ambiguous timeout must be checked by observing state.
            response = session.request("POST" if payload is not None else "GET", self.settings.ha_url+path,
                json=payload, headers={"Authorization": "Bearer "+self.settings.ha_token},
                verify=self.settings.ca_file or True, timeout=(3, 5), allow_redirects=False, stream=True)
            with response:
                if response.status_code in (401, 403):
                    raise BridgeError(502, "Home Assistant rejected its access token or permissions")
                if response.status_code == 404:
                    raise BridgeError(404, "Entity or action not found in Home Assistant")
                if not 200 <= response.status_code < 300:
                    raise BridgeError(502, "Home Assistant returned HTTP "+str(response.status_code))
                content = bytearray()
                for chunk in response.iter_content(16384):
                    content.extend(chunk)
                    if len(content) > 8*1024*1024:
                        raise BridgeError(502, "Home Assistant response exceeded the size limit")
                if binary:
                    return bytes(content)
                try:
                    return json.loads(content)
                except (ValueError, UnicodeError):
                    raise BridgeError(502, "Home Assistant returned invalid JSON") from None
        except requests.RequestException:
            # Never echo request headers, URLs containing credentials, token or upstream body.
            raise BridgeError(503, "Home Assistant connection failed" + ("; command outcome may be unknown" if payload is not None else "")) from None

class Bridge:
    def __init__(self, settings: Settings, client=None, clock=time.monotonic):
        self.settings = settings
        self.client = client or HAClient(settings)
        self.clock = clock
        self.lock = threading.RLock()
        self.stop = threading.Event()
        self.command_lock = threading.Lock()
        self.frame_lock = threading.Lock()
        self.items = {}
        self.services = set()
        self.services_at = None
        self.last_error = "Home Assistant token is not configured" if not settings.ha_token else "Waiting for first update"
        self.commands = OrderedDict()
        self.last_frame = None
        self.frame_at = -float("inf")

    def _normalize(self, key, raw):
        spec = self.settings.entities[key]
        if not isinstance(raw, dict) or raw.get("entity_id") != spec["entity_id"] or not isinstance(raw.get("state"), str):
            raise BridgeError(502, "Unexpected Home Assistant entity response")
        state = raw["state"][:120]
        attrs = raw.get("attributes") if isinstance(raw.get("attributes"), dict) else {}
        available = state.lower() not in UNKNOWN
        actions = []
        for action, service in ACTION_SERVICES.get(spec["kind"], {}).items():
            domain = spec["entity_id"].split(".")[0]
            flag = FEATURES.get((spec["kind"], action))
            supported = attrs.get("supported_features")
            if (domain, service) in self.services and (not flag or isinstance(supported, int) and supported & flag):
                actions.append(action)
        result = {"key": key, "label": spec["label"], "kind": spec["kind"], "state": state,
                  "available": available, "actions": actions, "unit": ""}
        if spec["kind"] == "temperature":
            try:
                value = float(state)
                result["value"] = value if math.isfinite(value) else None
                result["available"] = available and math.isfinite(value)
            except ValueError:
                result["value"] = None
                result["available"] = False
            unit = attrs.get("unit_of_measurement")
            result["unit"] = unit if unit in ("°F", "°C", "F", "C") else ""
        if spec["kind"] == "cover":
            position = attrs.get("current_position")
            if isinstance(position, (int, float)) and not isinstance(position, bool) and math.isfinite(position) and 0 <= position <= 100:
                result["position"] = position
        # Deliberately omit all unknown attributes, camera tokens and inferred laundry countdowns.
        return result

    def _read(self, key):
        spec = self.settings.entities[key]
        try:
            raw = self.client.request("/api/states/"+spec["entity_id"])
            with self.lock:
                self.items[key] = (self._normalize(key, raw), self.clock(), None)
        except BridgeError as error:
            with self.lock:
                old = self.items.get(key)
                self.items[key] = (old[0] if old else {"key": key, "label": spec["label"], "kind": spec["kind"],
                    "state": "unknown", "available": False, "actions": [], "unit": ""}, old[1] if old else None, str(error))
            raise

    def poll_once(self):
        if not self.settings.ha_token:
            return
        try:
            if self.services_at is None or self.clock()-self.services_at >= 60:
                raw = self.client.request("/api/services")
                if not isinstance(raw, list):
                    raise BridgeError(502, "Invalid Home Assistant actions response")
                services = {(d["domain"], s) for d in raw if isinstance(d, dict) and isinstance(d.get("domain"), str)
                            and isinstance(d.get("services"), dict) for s in d["services"]}
                with self.lock:
                    self.services = services
                    self.services_at = self.clock()
            first_error = None
            for key in self.settings.entities:
                try:
                    self._read(key)
                except BridgeError as error:
                    first_error = first_error or str(error)
                    if error.status != 404:
                        # A global outage invalidates everything now, not only after TTL.
                        with self.lock:
                            for k, (item, when, _) in list(self.items.items()):
                                self.items[k] = (item, when, str(error))
                        break
            with self.lock:
                self.last_error = first_error
        except BridgeError as error:
            with self.lock:
                self.last_error = str(error)
                for key, (item, when, _) in list(self.items.items()):
                    self.items[key] = (item, when, str(error))

    def run(self):
        while not self.stop.is_set():
            self.poll_once()
            self.stop.wait(self.settings.poll_seconds)

    def snapshot(self):
        with self.lock:
            result = []
            for key, spec in self.settings.entities.items():
                if key in self.items:
                    item, when, error = self.items[key]
                    item = copy.deepcopy(item)
                    age = self.clock()-when if when is not None else None
                    stale = age is None or age > self.settings.ttl_seconds or error is not None
                    item.update(age_s=round(age, 1) if age is not None else None, stale=stale, error=error)
                    item["available"] = item["available"] and not stale
                    if not item["available"]:
                        item["actions"] = []
                        item["state"] = "unavailable"
                        item.pop("value", None)
                        item.pop("position", None)
                else:
                    item = {"key": key, "label": spec["label"], "kind": spec["kind"], "state": "unknown",
                            "available": False, "stale": True, "actions": [], "unit": "", "age_s": None, "error": "Waiting for update"}
                result.append(item)
            return {"version": 1, "configured": bool(self.settings.ha_token), "error": self.last_error,
                    "connected": any(i["available"] for i in result), "items": result}

    def command(self, payload):
        if not isinstance(payload, dict) or set(payload) != {"key", "action", "request_id"}:
            raise BridgeError(400, "Expected key, action and request_id")
        key, action, rid = (payload[n] for n in ("key", "action", "request_id"))
        if not all(isinstance(x, str) for x in (key, action, rid)) or not 8 <= len(rid) <= 80:
            raise BridgeError(400, "Invalid command")
        if key not in self.settings.entities or action not in ACTION_SERVICES.get(self.settings.entities[key]["kind"], {}):
            raise BridgeError(400, "Action is outside the configured controls")
        # No queued commands and no retry after a potentially executed request.
        if not self.command_lock.acquire(blocking=False):
            raise BridgeError(409, "Another command is in progress")
        try:
            with self.lock:
                if rid in self.commands:
                    prior, result = self.commands[rid]
                    if prior != (key, action):
                        raise BridgeError(409, "request_id was already used for a different command")
                    return result
                item = next(i for i in self.snapshot()["items"] if i["key"] == key)
                if not item["available"] or action not in item["actions"]:
                    raise BridgeError(409, "Entity is unavailable or action is unsupported")
                outcome = {"accepted": False, "outcome": "unknown", "request_id": rid,
                           "message": "Check current state before sending a new command"}
                self.commands[rid] = ((key, action), outcome)
                while len(self.commands) > 128:
                    self.commands.popitem(last=False)
            spec = self.settings.entities[key]
            service = ACTION_SERVICES[spec["kind"]][action]
            domain = spec["entity_id"].split(".")[0]
            try:
                self.client.request(f"/api/services/{domain}/{service}", {"entity_id": spec["entity_id"]})
            except BridgeError:
                with self.lock:
                    self.last_error = "Command outcome unknown; verify current device state"
                # Preserve outcome for duplicate request_id; never silently retry.
                return outcome
            with self.lock:
                outcome = {"accepted": True, "outcome": "service_completed", "request_id": rid}
                self.commands[rid] = ((key, action), outcome)
            # Refresh from HA; service success is not proof that the physical device reached its target.
            try:
                self._read(key)
            except BridgeError:
                pass
            return outcome
        finally:
            self.command_lock.release()

    def frame(self):
        current = next(i for i in self.snapshot()["items"] if i["kind"] == "camera")
        if not current["available"]:
            raise BridgeError(503, "Camera is unavailable")
        with self.frame_lock:
            if self.last_frame is not None and self.clock()-self.frame_at < 1:
                return self.last_frame
            entity = self.settings.entities[current["key"]]["entity_id"]
            raw = self.client.request("/api/camera_proxy/"+entity, binary=True)
            try:
                from PIL import Image, UnidentifiedImageError
                with Image.open(io.BytesIO(raw)) as image:
                    # Bound decoding before load; suppress pathological camera input.
                    if image.width*image.height > 16000000:
                        raise BridgeError(502, "Camera frame exceeds decode limit")
                    image.thumbnail((480, 320))
                    output = io.BytesIO()
                    image.convert("RGB").save(output, "JPEG", quality=65)
                    frame = output.getvalue()
                    if len(frame) > 96000:
                        raise BridgeError(502, "Scaled camera frame exceeds 96 KB")
            except ImportError:
                raise BridgeError(503, "Install bridge requirements to enable camera images") from None
            except (OSError, ValueError, UnidentifiedImageError, Image.DecompressionBombError):
                raise BridgeError(502, "Camera returned an invalid image") from None
            self.last_frame, self.frame_at = frame, self.clock()
            return frame

class Handler(BaseHTTPRequestHandler):
    server_version = "CottageHome/0.3"
    def log_message(self, fmt, *args):
        # Do not log paths/query strings, tokens, entity data or POST bodies.
        pass

    @property
    def bridge(self):
        return self.server.bridge

    def _authorized(self):
        supplied = self.headers.get("X-Cottage-Token", "")
        return bool(supplied) and secrets.compare_digest(supplied.encode(), self.bridge.settings.bridge_token.encode())

    def _send(self, status, data, content_type="application/json"):
        if isinstance(data, dict):
            data = json.dumps(data, allow_nan=False).encode()
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' blob:; frame-ancestors 'none'; base-uri 'none'")
        self.end_headers()
        self.wfile.write(data)

    def _body(self):
        try:
            size = int(self.headers.get("Content-Length", "0"))
            if not 0 < size <= 1024:
                raise ValueError()
            def reject_pairs(pairs):
                result = {}
                for key, value in pairs:
                    if key in result:
                        raise ValueError()
                    result[key] = value
                return result
            return json.loads(self.rfile.read(size), object_pairs_hook=reject_pairs)
        except (ValueError, UnicodeError):
            raise BridgeError(400, "Invalid JSON command") from None

    def _dispatch(self):
        path = urlsplit(self.path).path
        if self.command == "GET" and path in ("/", "/home"):
            return self._send(200, (ROOT/"home.html").read_bytes(), "text/html; charset=utf-8")
        if not self._authorized():
            raise BridgeError(401, "Bridge token required")
        if self.command == "GET" and path == "/api/ha/status":
            return self._send(200, self.bridge.snapshot())
        if self.command == "POST" and path == "/api/ha/command":
            result = self.bridge.command(self._body())
            return self._send(200 if result["accepted"] else 502, result)
        if self.command == "GET" and path == "/api/ha/camera":
            return self._send(200, self.bridge.frame(), "image/jpeg")
        # Camera live view uses successive 1 fps server-scaled frames requested by the active page.
        # It stops completely when the camera page closes; no HA camera/access_token URLs are exposed.
        raise BridgeError(404, "Unknown endpoint")

    def do_GET(self):
        self.connection.settimeout(10)
        try:
            self._dispatch()
        except BridgeError as error:
            self._send(error.status, {"error": str(error)})
        except (BrokenPipeError, ConnectionResetError, TimeoutError):
            pass

    do_POST = do_GET

class Server(ThreadingHTTPServer):
    daemon_threads = True
    def __init__(self, address, bridge):
        super().__init__(address, Handler)
        self.bridge = bridge


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", type=Path, default=ROOT/"entities.json")
    parser.add_argument("--bind", default=os.environ.get("COTTAGE_BIND", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("COTTAGE_PORT", "8087")))
    args = parser.parse_args()
    settings = Settings.load(args.config)
    bridge = Bridge(settings)
    server = Server((args.bind, args.port), bridge)
    worker = threading.Thread(target=bridge.run, daemon=True)
    worker.start()
    print(f"Cottage Home panel: http://{args.bind}:{args.port}", flush=True)
    if not os.environ.get("COTTAGE_BRIDGE_TOKEN"):
        print("Temporary bridge token: "+settings.bridge_token, flush=True)
    if not settings.ha_token:
        print("HA_TOKEN is not set. Panel runs in setup state; no HA requests or device commands are sent.", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        bridge.stop.set()
        server.server_close()

if __name__ == "__main__":
    main()
