# Super Mini Wi-Fi comparison — v0.6.1

The supplied v0.6.0 log reports a 20.00 dBm driver maximum. Association attempts
enter authentication, then repeatedly return to init with reason 2
(`WIFI_REASON_AUTH_EXPIRE`, authentication expired). The supplied excerpt never
gets a station IP. Attempts appear on channels 1, 6 and 11; the excerpt does not
identify the BSSID or RSSI of each failed attempt. It cannot establish whether
the nearby AP remained selected. A USB monitor disconnect/reconnect is also
present, but without enough reset context to diagnose a brownout or a crash.

v0.6.1 restores the prior 13 dBm maximum on the Super Mini only, using
`esp_wifi_set_max_tx_power(52)` after Wi-Fi startup. The driver's returned maximum
is logged and remains visible in connection diagnostics. This is an A/B test and
workaround, not proof that transmit power, USB power delivery or hardware is the
root cause. Calibration, Test, Go, radio pairing and Wi-Fi credentials are kept.

Build/flash locally with the usual `tools/build-controller-supermini.ps1` script;
do not erase flash/NVS. Confirm version **0.6.1** and
`Super Mini Wi-Fi max TX power (bench limit): 13.00 dBm`. Keep location, USB
supply/cable and AP selection unchanged for the comparison. Look for actual Wi-Fi
association and `Controller LAN address`, then verify HTTP by IP and hostname.
If it still fails, collect the reconnect sequence and check the controller's
selected AP in Omada; do not infer the root cause from the reason code alone.
