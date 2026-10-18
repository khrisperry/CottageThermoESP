# CottageThermoESP controller — AtomS3 Lite servo BENCH test

**Hardware now selected for the FIRST PROTOTYPE:** M5Stack **AtomS3 Lite** (ESP32-S3FN8 / 8 MB flash). Servo PWM signal on rear header **G38 = GPIO38**. This is an experimental, **UNBUILT / UNFLASHED starter**; it has not been verified on the physical board.

**BENCH ONLY:** Test with the servo horn removed first, then with an inert dummy load. Do NOT connect this experimental servo controller or printed parts to an operating gas furnace.

## What it does

- Starts the Wi-Fi access point `CottageThermoBench` at **192.168.4.1** after you set a unique WPA2 password in `menuconfig`.
- The web page issues explicit TS90MD servo **50 Hz PWM pulse widths from 1100 to 1900 µs**. **1500 µs** is a starting point with servo horn detached, **not** a measured middle angle. Move in small steps.
- PWM remains **disabled until you click Command PWM**. No servo sweep, automatic homing, or last PWM restoration at startup.
- **Stop PWM** disables the signal. That is not a physical brake; an unpowered/uncommanded servo may still be back-driven. The very first command can cause an abrupt movement.
- Reports the configured signal GPIO and last commanded pulse (not the servo's measured angle). There is not yet any encoder.
- This first prototype has no station-mode Wi-Fi, Home Assistant, OTA, smooth motion, authentication beyond temporary bench WPA2, servo-current feedback or mechanical safety verification.

## AtomS3 Lite wiring

The official AtomS3 Lite rear header explicitly includes **G38** (GPIO38), **GND**, **5V**, **3V3** and other GPIOs. Confirm orientation using the actual board silkscreen, not wire position.

| Source | Servo lead / destination |
|---|---|
| AtomS3 Lite rear **G38 / GPIO38** | Servo **signal** wire, commonly orange/yellow (3.3 V PWM logic) |
| AtomS3 Lite rear **GND** | External servo supply **negative/GND**, shared with servo ground (brown/black) |
| Separate **regulated 5 V servo supply**, adequately rated for TS90MD stall/start current | Servo **red / +5 V** wire |
| AtomS3 Lite **USB-C** | USB power, flashing, and USB Serial/JTAG monitor |

**Do not connect servo power to GPIO38 or the board's 3.3 V pin.** Prefer a separate appropriately rated 5 V supply for initial testing rather than drawing servo motor surge current from the AtomS3 Lite's rear 5V rail or USB. Tie the supply ground to AtomS3 GND; otherwise PWM cannot have a reliable reference. Verify the actual TS90MD wire colors before connecting. Keep all power wiring insulated.

The board GPIO provides a **3.3 V logic signal**, not a 5 V servo power supply. If the servo does not reliably recognize this signal, troubleshoot electrical levels with measurements rather than driving 5 V into GPIO38.

## Windows / VSCode ESP-IDF local build

From the configured ESP-IDF PowerShell terminal:

```powershell
cd C:\Users\kperry\Documents\GitHub
git clone https://github.com/khrisperry/CottageThermoESP.git
cd .\CottageThermoESP\controller

idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p COMxx flash monitor
```

If you already cloned the repo, replace `git clone` with `git pull` inside the `CottageThermoESP` folder.

In **menuconfig → Cottage thermostat BENCH servo tester**, set a **new temporary Wi-Fi password** (at least 8 characters). The shipped placeholder deliberately prevents Wi-Fi from starting. The servo signal default is now **GPIO38**; leave that unchanged unless you intentionally rewire it. `sdkconfig.defaults` enables the USB Serial/JTAG console and configures the chip's 8 MB flash by default.

Replace `COMxx` with the COM port for the AtomS3 Lite as shown in Windows Device Manager. The board has native USB-C flashing/serial; no separate USB-UART dongle should be needed.

### If Windows cannot flash automatically

M5Stack documents a manual download-mode procedure: **hold the board's reset button approximately 2 seconds until the internal green LED illuminates, then release**. The green light turns off and it enters download mode. Try flashing again. After flash, reset/power-cycle if it remains in download mode. Consult the [M5Stack AtomS3 Lite documentation](https://docs.m5stack.com/en/core/AtomS3-Lite).

If `idf.py monitor` is blank, verify that the USB console setting `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` is present in the generated `controller/sdkconfig`; if you previously generated a conflicting configuration, use `idf.py menuconfig` to select USB Serial/JTAG console explicitly.

### First web test

1. Disconnect servo horn or coupling and verify the servo is mechanically unloaded.
2. Power AtomS3 Lite by USB, servo from separate 5V supply with common ground.
3. Connect a phone/laptop to Wi-Fi **CottageThermoBench** using the password you configured.
4. Open **http://192.168.4.1**. It is a test SoftAP and does not use your existing home network.
5. Use **Refresh status** to confirm `servo_gpio: 38` and `pwm_active: false`.
6. Enter **1500**, click **Command PWM**, then test 1490 / 1510 / 1470 / 1530 and record actual motion.
7. Click **Stop PWM**. The servo is no longer driven or holding torque. Test power loss/restoration with an isolated fixture **only**.

### Known limitations

This example purposely has no closed-loop feedback, no automatic position or travel calibration, no servo power gating, no explicit ramp, no torque/stall detection, and no mechanically verified retention. The direct-drive furnace assembly and all unattended gas-appliance controls must undergo separate review and testing; **do not install this bench build on the furnace**.

For the overall plan see [../docs/PROJECT_SPEC.md](../docs/PROJECT_SPEC.md), [../docs/PROTOTYPE_01.md](../docs/PROTOTYPE_01.md).
