# Mechanical notes: Williams knob / TS90MD direct drive

## Reference images and uncertainty

The discussion supplied two low-resolution views of the Williams timer/thermostat section on the upper fascia: a circular timer above, a small protruding black thermostat knob below. The knob seems lobed/contoured, not guaranteed cylindrical. **Neither image has a scale. Do NOT derive physical measurements from these pixels.** The knob is lower than the timer; the actuator must avoid both the timer and the nearby top ventilation slots.

The furnace product/knob photos should be replaced or supplemented with real close-ups with calipers and a 3D scan. Store measured/verified image assets under \`hardware/reference/\` when available, but do not reuse proprietary images in the public repository without permission.

## Desired stack (axis normal to furnace panel)

\`\`\`
stationary, removable mount (attached to acceptable fixed support)
    |
TS90MD body (fixed against reaction torque)
    |
servo output shaft / matching spline horn (T20 OR T25 TBD)
    |
short direct-release keyed coupler; NO intermediate gears
    |
3D-printed removable friction sleeve / manual cap
    |
ORIGINAL Williams thermostat knob (unchanged)
\`\`\`

- Splines refer to teeth on servo output shaft; *they are not the same as a gear mesh*. A matching horn/hub can transfer torque directly, 1:1.
- Coupler is detachable axially. Remove servo and bracket without removing knob cap; turn cap manually with fingers.
- The AS5600, magnet, encoder frame and flexible U-arm are removed. Use a compact direct servo-to-cap coupling, secured to the matching factory spline horn. Provide unobstructed axial access for the original servo output retaining screw and mechanically secure the printed coupler to the horn.
- Motion ends must remain INSIDE the real knob's travel; hard stops are NOT calibration targets.
- Bracket must support both torsion reaction AND lateral alignment; the knob shaft is not a servo mounting post. Use lockable X/Y slots, adjustable axial depth and a compact tilt/rotation plate to align the servo on three axes before tightening.

## Print trials (NOT verified dimensions)

Test sleeve inner diameters: **18, 19.5, 21, 22.5, 24, 25 mm**. Actual STL geometry generated from OpenSCAD is intentionally a basic hollow knurled cup for fit trials, *without servo attachment*. It is not a press-fit guarantee. Start with generous clearance; use a caliper to verify real printed bore sizes.

Do not create a very tight snap fit on the real furnace knob until materials, forces and installation safety have been evaluated. Trial cups may use a flexible slit design later; round bore may fail if knob is noncircular.

## Measurements to obtain

| Feature | Measurement |
|---|---|
| Knob maximum and minimum OD | unknown |
| Shape/splines/grip grooves | unknown |
| Knob height/projection / usable sleeve depth | unknown |
| Gap to adjacent trim and timer | unknown |
| Mechanical travel 0–6 | ~120° estimate only |
| Back-driving torque and end-stop force | unknown |
| Servo actual output spline (T20/T25) | unknown |
| Safe stationary attachment points | unknown |
| Actual casing surface temperature during firing | unknown |
| Clearance for straight axial removal | unknown |
| Factory spline count and servo retaining-screw geometry | unverified |\n| Mount pitch/yaw/roll and three-axis adjustment clearance | unverified |

Do not attach adhesives, printed mounts or fixtures to hot surfaces, vents, access panels requiring service, or gas-safety hardware without an approved design and HVAC review.
