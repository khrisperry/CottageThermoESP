# User preferences

Do not add local webpage or local device API access tokens, token-entry screens,
or token-based setup gates unless the user explicitly requests them.
Device webpages must open directly and controller Wi-Fi setup must work without
copying a token from the serial monitor. Preserve required external-service
credentials (such as Home Assistant/MQTT) and encrypted ESP-NOW peer keys.
