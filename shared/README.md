# Shared thermostat engine

`thermo/` is an ESP-IDF component and hardware-independent C module. It owns configuration validation, source selection/freshness, calibrated interpolation, weekly schedule selection and bounded movement decisions. It never performs I/O, writes NVS, emits PWM, invents position feedback or commits a proposed command until the platform reports success.

The controller owns persistence, monotonic time, wall-clock input, HTTP and hardware. Host tests compile exactly the same engine source. See `docs/API.md` for the JSON wire contract.
