# Prototype 01 — direct-drive bench proof of concept

**Objective**: establish whether the **TS90MD** can directly rotate an appropriately sized knob-like load through the planned angle without binding, using a coaxial removable cap and stationary fixture. This is a **bench-only experimental build**; neither the controller nor cap has been safety-validated for connection to an operating gas furnace.

## A. Prepare materials

- M5Stack **AtomS3 Lite** (ESP32-S3FN8, 8MB flash) for first bench testing; future final controller board is TBD.
- TS90MD servo; manufacturer-matching spline horn/hub; optional MG995 **only as size/torque comparison**, not automatic replacement.
- Regulated **5 V supply rated for servo stall current** (rating must be established from actual servo datasheet/measurement); do **not** drive servo from ESP32 3.3 V or exceed I/O voltage. Tie grounds together; one PWM signal from configured GPIO (**G38 / GPIO38** on AtomS3 Lite for bench firmware).
- Calipers, scale/ruler in photographs, 3D printer, optional 3D scan.
- Separate inert dummy knob/shaft fixture and torque measurement tool where possible.
- OpenSCAD (for first simple fit coupons); later a fully dimensioned multi-part assembly.

## B. Controller

See [controller/README.md](../controller/README.md). First firmware boots into a temporary OPEN, password-free SoftAP, web page on \`http://192.168.4.1\`, gives **explicit** movement commands in microseconds. PWM disabled until a user sends a command; STOP PWM button. Nothing is stored or automatically restored, and there is no closed-loop sensing yet.

**Warning**: enabling PWM can cause the servo to suddenly move to the commanded angle. Start with horn/cap REMOVED and no load, then attach inert fixture only when mechanical center and movement are verified. Do not assume the servo's printed angle or pulse endpoints are correct.

## C. Mechanical proof stages

1. **Measure** original knob (when available) using calipers, profile photos and/or scan; no guessed interference fit to a live gas appliance.
2. **Print test caps** for inner diameter 18, 19.5, 21, 22.5, 24 and 25 mm (all candidates only). See \`hardware/cap_fit_kit.scad\`; revise inner profile/tolerance/depth after testing.
3. **Check fit by hand** off-device on a dummy/replica. Do not press a tight cap onto gas-control hardware. Check no rubbing against surrounding trim.
4. **Print stationary bracket** only after surrounding fixed mounting points and thermal clearance are documented. Bracket must not ride on or be supported by rotating knob.
5. Use **matching T20 or T25 servo spline/hub**, *after verifying actual servo shaft*, directly coaxial to knob cap. Do not print spline teeth by assumption. Attach cap to hub mechanically; allow servo module to disengage while leaving manual cap in place.
6. On an isolated rig, measure available range/torque, backlash, smooth motion and mechanical loading; later professional evaluation required before proximity to functioning furnace.
7. Verify manual override: servo module detaches easily, cap stays on knob surrogate and can be turned directly.
8. Cut power at multiple positions and restore power, repeatedly; ensure no uncommanded position sweep and evaluate whether the load actually retains its previous angle without powered servo holding torque.

## D. Test matrix and acceptance evidence

| Test | Evidence to record | Stop condition |
|---|---|---|
| Unloaded PWM 1500us, then +/- small steps | angle, jitter, current, boot behavior | irregular movement/heat |
| Dummy knob at mid range | torque, smoothness, side-load | binding, stall or slip |
| Requested approximate 120° total | actual observed safe angle | any hard-stop contact |
| Power removal/restoration | before/after angle, motor twitch, retention | any unexpected displacement |
| Servo detach/reinsert | manual turning, snap alignment | requires force on furnace knob |
| Different fit cap IDs | actual ID after print, ease of removal | cracking or overly tight grip |
| Heat/placement review | measured surface temp, vent/clearance | unknown or unsafe temperatures |
| Faults: Wi-Fi loss/reboot | unchanged dummy knob physical position | movement not explicitly commanded |

Document pass/fail and measured results here. **Nothing passes real-furnace use until installation safety and compliance are reviewed.**

## E. Photo/scan acquisition at cottage

- Straight-on with ruler across knob and furnace panel; side view with calipers on outside diameter, projection and available gaps.
- Photos at **0, 1, 3, 6** if naturally permitted by ordinary manual controls, with a noninvasive angle reference; do not force stops.
- Measure total angular travel gently without applying extra torque at limits; knob may have other functions/detents.
- Separate knob from surrounding chrome/plastic region in photos; measure flat/noncircular profile, grooves, recess, heights, depth, available cap grip area.
- Photograph all possible NON-hot stationary mounting points, existing screw/fastener sites, adjacent vents, clearance to timer and panel.
- Scan with known scale; compare scan dimensions against multiple physical caliper values before CAD work.
- Record model/rating label and consult qualified installer about external actuator feasibility and hot-surface temperature.

## F. Build order

- Current deliverable: documents + basic bench web app + fit coupons.
- Next: complete real measurements / scan, print and assess coupons.
- Then: design exact keyed friction cap, detachable servo housing, and independent bracket.
- Then: encoder alignment and closed-loop position verification, then full thermostat logic.

No automatic temp learning, HA climate control, OTA, position restoration or occupancy behavior is claimed in the bench prototype firmware.
