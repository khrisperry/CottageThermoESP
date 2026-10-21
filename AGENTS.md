# User preferences

Do not add local webpage or local device API access tokens, token-entry screens,
or token-based setup gates unless the user explicitly requests them.
Device webpages must open directly and controller Wi-Fi setup must work without
copying a token from the serial monitor. Preserve required external-service
credentials (such as Home Assistant/MQTT) and encrypted ESP-NOW peer keys.

Network services (HA, Ships, MQTT and camera) must resume when the wall panel's
Wi-Fi returns, even if the furnace controller is still unavailable or uses
ESP-NOW. Pause those services for actual Wi-Fi loss; controller transport must
not become their recovery gate. Keep Wi-Fi retries active during outages.
