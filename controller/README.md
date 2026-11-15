# Super Mini furnace controller — open-loop prototype

Both applications use the root `VERSION` file. See [production build and upgrade guide](../docs/PRODUCTION.md).

Hardware: HW-747 ESP32-S3 Super Mini, 4 MB flash, servo signal GPIO5, separate 5 V servo supply. **AS5600/magnet are no longer required**, and the controller firmware does not start the encoder.

The webpage retains Service/Calibration, attended Test, 15-minute Bench Go, and persistent Live intent. Calibration still stores Off/1–6/Max PWM pulses and estimated temperatures. The obsolete angle fields remain in the saved NVS binary layout for compatibility, but firmware neither samples nor validates them.

**Live cannot automatically recover position after any reset.** Select Go Live to persist intent, then physically inspect the furnace knob, enter its actual dial setting (1–6; fractional values supported), and explicitly confirm. Live stays stopped until this confirmation and a fresh primary thermostat reading are present. The confirmation is held in RAM only; reboot, Stop, Service or fault requires it again. A communications watchdog restart will also require manual reconfirmation.

Open-loop commands can never prove actual knob movement, detect a slipped coupling, or identify binding. The status page labels commanded values as *not measured*. Test only with the furnace supervised, retain manual controls, verify mechanical stops and use independent freeze/overheat protection. **Do not deploy this prototype as unattended heat control.**

The room thermostat firmware, temperature algorithms, Saved target/Away settings, Home Assistant/MQTT, Wi-Fi, ESP-NOW and controller application identity are unchanged. Controller firmware is updated by USB.
