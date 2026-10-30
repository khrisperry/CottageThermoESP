# Super Mini furnace controller — V1.0.X

Both applications use the repository `VERSION` file. See the [production build and upgrade guide](../docs/PRODUCTION.md) and [V1.0.0 release notes](../docs/releases/V1.0.0.md).

Hardware: HW-747 ESP32-S3 Super Mini, 4 MB flash, servo signal GPIO5, AS5600 SDA6/SCL7. Servo power uses the separate 5 V supply. Keep the tested pin profile when upgrading.

The controller webpage offers persistent Live, Service/Calibration, and attended Test/Bench Go. Go Live saves intent immediately; startup waits for valid calibration, a stable recovered encoder position and fresh primary room temperature. The Calibration & Live checks table shows saved pulses/angles/temperatures and the current command error. Capture replaces the selected marking's saved pulse and angle.

Live intent, target and calibration survive normal USB flashing. A missing encoder pauses PWM; verified reattachment can resume. Movement verification/PWM faults select Service and retain their exit diagnosis. The controller application identity and NVS layout are unchanged. Controller firmware updates use USB.

[Historical controller development notes](../docs/history/controller-development.md).
