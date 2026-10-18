# ESP-IDF controller — Servo BENCH web test

**Status:** initial **UNBUILT / UNFLASHED starter source** for ESP-IDF 6.x. It has not been verified on actual hardware. Do not connect to an operating furnace or other safety-critical mechanism.

## What it intentionally does

- Creates temporary WPA2 Wi-Fi access point \`CottageThermoBench\`; connect and browse to **http://192.168.4.1**.
- Manually issues PWM pulse commands at **50 Hz** within conservative **1100–1900 µs** software bounds.
- A STOP PWM button explicitly disables signal. It does **not** physically disconnect a servo, apply a brake or guarantee the shaft remains stationary.
- **No servo PWM command on boot**, no stored target, no automatic homing and no calibration endpoints. Physical servo may still twitch when external power is applied.
- Provides last commanded pulse, not measured angle; servo feedback and safe recovery are future work.

## Wiring, BENCH ONLY

| Source | Destination |
|---|---|
| ESP32-S3 default GPIO18 | Servo signal only |
| ESP32 ground | Servo power supply ground (common reference) |
| Regulated 5 V external supply | TS90MD 5 V power, within servo's actual rating |

**Do not power TS90MD from the ESP32 3.3 V pin**. Use a sufficiently rated supply to withstand stall surges; confirm voltage/current requirements for your exact unit. Hardware 10 kΩ signal pull-down may help avoid a floating signal before boot, but electrical behavior must be tested. Remove servo horn and keep the mechanism unloaded for the first power-up and command.

GPIO18 is configurable in menuconfig and is a tentative bench assignment. The actual controller board is not yet selected.

## Build/flash in VSCode ESP-IDF terminal

\`\`\`powershell
cd controller
idf.py set-target esp32s3
idf.py menuconfig
# Set "Cottage thermostat BENCH servo tester" -> new WPA2 password and GPIO
idf.py build
idf.py -p COMxx flash monitor
\`\`\`

Connect your laptop/phone to the bench Wi-Fi SSID, enter the configured password, open \`http://192.168.4.1\` and click **Refresh status**, then **Command PWM**. Start with \`1500\` µs and move in very small increments. Use **Stop PWM** before powering down or reconfiguring the servo.

**Important:** The first PWM command can make the horn jump to its absolute servo target. Removing the PWM signal does not move servo to an OFF state; the motor is no longer commanded and may be back-drivable.

## Known omissions (intentionally not production-ready)

No captive Wi-Fi provisioning, STA, OTA, HA MQTT, motor current feedback, torque sensing, angle encoder, real knob calibration, thermal limits, network authentication beyond bench AP WPA2, rate-limited motion trajectory, movement timeout, occupancy gating, or gas appliance integration. Do not treat this as a safe controller for unattended operation.

The full plan is in [../docs/PROJECT_SPEC.md](../docs/PROJECT_SPEC.md).
