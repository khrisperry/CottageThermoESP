/*
 * CottageThermoESP — KNOB DIAMETER FIT COUPONS ONLY
 *
 * Intended to be printed CLOSED BASE DOWN, OPENING UP on a 3D printer.
 * These six test cups have nominal INNER bore diameters in millimeters;
 * they are NOT measured knob geometry, NOT servo couplings, and NOT
 * approved to touch an operating/hot gas furnace.
 *
 * To preview: openscad hardware/cap_fit_kit.scad
 * To export STL: openscad -o cap_fit_kit.stl hardware/cap_fit_kit.scad
 *
 * Change diameters[] after measuring. Print a single version if desired
 * using item=-1 (all), or item=0..5 for individual variants.
 */

diameters = [18, 19.5, 21, 22.5, 24, 25];
item = -1;                   // -1 = full six-size test kit
wall_mm = 2.2;
base_mm = 2.6;               // closed base (lays flat on print bed)
depth_mm = 12;               // sleeve depth above base
rib_height_mm = 0.8;         // external grip ribs only
rib_width_mm = 1.5;
rib_count = 16;
label_height_mm = 0.65;
spacing_mm = 38;
$fn = 96;

module fit_cup(nominal_inner_mm) {
    od = nominal_inner_mm + 2*wall_mm;
    total_height = base_mm + depth_mm;
    union() {
        difference() {
            union() {
                cylinder(d=od, h=total_height);
                // External grip ribs; none intrude into the fit bore.
                for (k = [0:rib_count-1]) {
                    rotate([0,0,360*k/rib_count])
                        translate([od/2-0.25,-rib_width_mm/2,base_mm+1])
                            cube([rib_height_mm+0.25,rib_width_mm,depth_mm-2]);
                }
            }
            // A completely OPEN TOP and flat CLOSED bottom print cleanly.
            translate([0,0,base_mm])
                cylinder(d=nominal_inner_mm, h=depth_mm+0.1);
        }
        // Tiny integrated flat tag for nominal bore label; overlaps cup.
        translate([-9,-od/2-5,0])
            cube([18,9,base_mm]);
        translate([0,-od/2-0.7,base_mm])
            linear_extrude(height=label_height_mm)
                text(str(nominal_inner_mm), size=4.0,
                     halign="center", valign="center");
    }
}

if (item == -1) {
    for (i=[0:len(diameters)-1])
        translate([i*spacing_mm,0,0]) fit_cup(diameters[i]);
} else {
    fit_cup(diameters[item]);
}
