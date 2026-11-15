# CottageThermostat — Direct Drive / 3-Axis Adjustable Mount v1

**Status:** PRINT/MEASURE PROTOTYPE ONLY (2026-11-15). No AS5600 sensor, encoder magnet, sensor bracket, or rotating U-arm. Not validated against the actual on-site furnace. Existing thermostat firmware stays separate; the AS5600-free controller changes are in draft PR #9.

## Starting from what already fitted at the cottage

- **Option C** of the earlier cap fit coupons fully seated on the original Williams knob with firm hand pressure. Measured approximate six-sided envelope: **18.41 × 17.41 mm**. The original Option C STL/source is **not currently available**, so `cap.stl` uses a reconstructed six-sided profile with **0.35 mm clearance per side**, a **13.5 mm engagement depth**, and a 4 mm roof. This **must be fit checked**; it is **not** an exact reproduction of the successful cap.
- **S102** was the previous proven printed servo spline hub (approximately +4% enlarged socket). This redesign deliberately does **not** use printed spline friction to carry torque: use the **stock servo horn** matched to the TS90MD spline, attached to `coupling.stl` using two M2 screws and inserts. The factory retaining screw goes through the **6.2 mm axial center passage** and threads into the servo shaft.
- Existing working **19.5 mm right-side standoff** is carried forward as the base post height; mount position and other face dimensions are adjustable prototypes, **not verified originals**.
- The **side-mounted taped pad** remains on the right/cooler furnace housing, with flat underside and no encoder magnet pockets. The previous request for base magnets is not required for this prototype.

## Printed parts

| STL | Function | Prototype envelope (mm) |
|---|---|---|
| `fit_coupon.stl` | Minimal profile fit check | Ø29 × 5.5 |
| `cap.stl` | Manual-grip knob sleeve, long male 12 mm keyed tenon | Ø31 max × 26.5 high |
| `coupling.stl` | Matching long socket, radial M3 positive lock, center screw access, factory horn interface | Ø33 max × 20 high |
| `side_base.stl` | Flat tape pad, four rigid mounting posts (19.5 mm) | 80 × 57 × 25 overall |
| `bridge.stl` | Reinforced channel bridge with X-travel slots | 122 × 47 × 17 |
| `xy_stage.stl` | Y-translation stage with three threaded adjustment studs | 65 × 52 × 5 |
| `servo_plate.stl` | Servo cradle plate, M2 ear slots + three-point depth/tilt | 65 × 52 × 5 |

**All seven exported STL meshes were checked as single closed, watertight solids.** Fit, load, thermal rating and on-furnace clearance are **not** verified.

## Adjustments

- **X (left/right on furnace):** slide the channel bridge on the four base post fasteners, nominal ±8.5 mm.
- **Y (up/down on furnace):** slide `xy_stage` on the two central stage fasteners, nominal ±8.5 mm.
- **Z (in/out toward knob):** adjust height using three M3 threaded studs between `xy_stage` and `servo_plate`. Actual usable range depends on selected stud length and nut/washer stack; avoid loading the knob shaft axially.
- **Pitch and yaw:** differentially adjust those same three studs, then lock each point with two opposed M3 nuts. Aim for truly coaxial servo and furnace knob, **not a permanently angled torque joint**.
- **Shaft clocking:** set via the factory servo horn and captured PWM calibration. Check servo actual output orientation; do not rely on a printed flexible connector.

## Estimated hardware

- One **original TS90MD factory horn with matching output spline**, and its **original center shaft screw**. Check the real servo/horn geometry; published TS90MD dimensions and screw patterns vary by variant.
- **2 × M2 heat-set inserts** (nominal ~3.2 mm holes in the coupler, 3.6 mm depth) and **2 × M2 screws** into the horn. Adjust `horn_hole_pitch` in OpenSCAD to match *actual* horn mounting-hole spacing (default 13 mm). Drill only if the horn allows an undamaged screw attachment; the horn may need a suitable metal/extended version.
- **1 × M3 × ~32–35 mm radial retention screw** through the cap tenon; **1 × M3 heat-set insert** 4 mm OD × 4 mm long in the opposite coupling wall. A 35 mm screw may require washers/spacer to prevent bottoming out.
- **4 × M3 heat-set inserts** in the base posts and **4 × M3 bridge fixing bolts**.
- **2 × M3 heat-set inserts** in the bridge and **2 × M3 short stage fixing bolts**.
- **3 × M3 heat-set inserts** in `xy_stage`, **3 × M3 ~20–25 mm threaded studs**, **6 × M3 lock nuts** and optional washers for Z/pitch/yaw leveling.
- Servo mounting screws for the actual TS90MD ear holes, compatible with 2.2 mm prototype slots.
- Suitable validated removable high-temperature mounting tape to the *approved cool, fixed* housing, not vents or service/safety panels.

## First print and fitting sequence

1. Print `fit_coupon.stl` alone; verify its six-sided profile relative to your **known-good Option C cap** and the real Williams knob. If it feels noticeably different, **do not force it**: edit `knob_clearance`, `knob_clock_deg`, `knob_x`, `knob_y` and reprint. The old Option C STL would allow an exact contour transfer.
2. With no furnace attachment, print `cap.stl` and `coupling.stl`. Slide square male tenon into matching female pocket. The joint should be rigid without excessive force. Verify the **cross-bolt holes line up** and M3 bolt positively prevents withdrawal; check cap removal after bolt loosening.
3. Attach the stock servo horn to the coupler using two M2 screws into its inserts. With cap OFF the coupler, drive the **original servo shaft screw upward through the center passage** into the horn/servo shaft and tighten. Confirm there is no interference with the servo's output bearing or the central screw head.
4. Print `side_base`, `bridge`, `xy_stage`, `servo_plate`; dry-assemble using screws/studs before applying tape. Verify the servo body, output-shaft offset, ear-hole pattern and wiring clearance. Edit `servo_body_x`, `servo_body_y`, `servo_body_offset_x` and `servo_mount_hole_x` if needed.
5. Confirm the taped right-side footprint and bridge clear the furnace timer, controls, bezel, vents and access panel. Adjust X/Y slots and the three Z/tilt studs until the servo output shaft and knob axis are coaxial and the direct coupler engages **without lateral side load**. Fully lock nuts/bolts before movement.
6. Supervise a slow mechanical-only servo test with **firmware in Service/Test, not Live**. Verify complete mechanical disengagement/manual knob control, no drift or loose key, no forced end stops and acceptable material temperature through real heating cycles.
7. Only consider automation after a furnace professional has assessed the actuator, mounting surface and its interaction with the gas appliance controls, and after an independent safe heat/freeze fallback is confirmed. The open-loop firmware cannot detect a slipped cap, jammed knob or mispositioned servo.

## Print settings (trial)

- Test coupon first; then ASA or other temperature-qualified filament **only after measuring actual housing temperature during operation**. PLA should not be assumed suitable next to a furnace. Printed polymer dimensions will change by filament/printer.
- Suggested prototype: 0.16–0.20 mm layer height, 5+ perimeters, 40–60% infill; orient flat faces down. Add supports only if your slicer needs them for horizontal cross holes or the servo plate.
- Do not put heat-set inserts into installed hardware; install and inspect on the bench.

## Regenerating STL from the editable source

Open `CottageThermostat_DirectDrive_v1.scad` in OpenSCAD, set `part` or run from a shell:

```bash
openscad -o cap.stl -D 'part="cap"' CottageThermostat_DirectDrive_v1.scad
openscad -o coupling.stl -D 'part="coupling"' CottageThermostat_DirectDrive_v1.scad
openscad -o side_base.stl -D 'part="side_base"' CottageThermostat_DirectDrive_v1.scad
openscad -o bridge.stl -D 'part="bridge"' CottageThermostat_DirectDrive_v1.scad
openscad -o xy_stage.stl -D 'part="xy_stage"' CottageThermostat_DirectDrive_v1.scad
openscad -o servo_plate.stl -D 'part="servo_plate"' CottageThermostat_DirectDrive_v1.scad
```

Parametric CAD source is included so successful original fit, mounting positions and material tolerances can be incorporated without rebuilding the assembly.
