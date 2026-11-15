# Super Mini furnace controller — open-loop prototype

Both applications use the root `VERSION` file. See [production build and upgrade guide](../docs/PRODUCTION.md).

Hardware: HW-747 ESP32-S3 Super Mini, 4 MB flash, servo signal GPIO5, separate 5 V servo supply. **AS5600/magnet are no longer required**, and the controller firmware does not start the encoder.

The webpage retains Service/Calibration, attended Test, 15-minute Bench Go, and persistent Live intent. Calibration still stores Off/1–6/Max PWM pulses and estimated temperatures. The obsolete angle fields remain in the saved NVS binary layout for compatibility, but firmware neither samples nor validates them.

**Live never automatically moves on boot.** After a reboot, PWM is OFF even when Live intent was saved. Choose **Stay Manual · stop servo** to persistently disable automatic movement, physically inspect and confirm the dial, or explicitly select **Resume Auto remotely** using the last settled **command** checkpoint from an earlier physically confirmed setup. Firmware writes a PENDING journal to NVS before every Live movement, then marks STABLE after four seconds; an interrupted movement blocks remote resume. A saved command is **NOT a measured knob position**. Someone could have turned the dial, or the coupling could have slipped, while the servo was unpowered. Remote rearm must be tested with furnace safety precautions, authenticated trusted network access and independent heat/freeze protection. See docs/PRODUCTION.md.

Open-loop commands can never prove actual knob movement, detect a slipped coupling, or identify binding. The status page labels commanded values as *not measured*. Test only with the furnace supervised, retain manual controls, verify mechanical stops and use independent freeze/overheat protection. **Do not deploy this prototype as unattended heat control.**

The room thermostat firmware, temperature algorithms, Saved target/Away settings, Home Assistant/MQTT, Wi-Fi, ESP-NOW and controller application identity are unchanged. Controller firmware is updated by USB.
