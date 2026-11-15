/*
 * CottageThermostat direct-drive + adjustable mount -- fit prototype v1
 * 2026-11-15
 * Units: millimeters. Select part with: openscad -D 'part="cap"' -o cap.stl ...
 * IMPORTANT: This is a DIMENSIONALLY ADJUSTABLE mechanical prototype, NOT a
 * certified gas-furnace actuator. Do not deploy unattended. The original
 * Option C cap and S102 fit sources were not available as CAD files. These
 * nominal measured dimensions RECONSTRUCT the interface, not a 1:1 copy.
 * Print coupon first and verify with the actual knob and servo/horn.
 *
 * Model coordinate axes: X runs right, toward taped mounting pad;
 * Y vertical along panel; Z projects out from furnace panel, toward servo.
 * No AS5600, magnet, or flexible U-arm.
 */
part = "assembly"; // cap, coupling, side_base, bridge, xy_stage, servo_plate, fit_coupon, assembly
$fn = 64;

// Knob: measured approximate six-sided envelope for former Option C interface.
// Clearances must be verified against the *successful* printed Option C part.
knob_x = 18.41;
knob_y = 17.41;
knob_clearance = 0.35;      // per side, not exact legacy Option C tolerance
knob_clock_deg = 0;
knob_engagement = 13.5;
cap_roof = 4;
cap_od = 29.0;
cap_rib_count = 16;

// Direct square tenon & matching socket (short, torsionally stiff coupling)
key_width = 12.0;
key_corner_radius = 0.85;
key_height = 9.0;
key_socket_clearance = 0.25; // on each side
crossbolt_d = 3.4;          // M3 radial retention bolt through cap AND coupling
crossbolt_z = 5.0;          // measured up from bottom of coupling/key
insert_od_hole = 3.85;     // user's M3 heat-set inserts, approx OD 4, length 4
insert_depth = 4.15;

// Factory horn interface: attach printed coupler TO THE HORN with 2 screws.
// The horn spline and original servo output screw carry torque/axial retention.
// Never depend on a printed spline's friction alone.
coupler_outer_d = 32.0;
coupler_socket_depth = 9.3;
coupler_total_height = 20.0;
servo_screw_access_d = 6.2; // center hole to pass retaining screw + tool
horn_hole_pitch = 13.0;     // M2 insert centers; match the *actual* factory horn
horn_fastener_d = 2.2;      // M2 insert pilot / screw (not printed spline)
horn_insert_bore_d = 3.20;  // additional M2 heat-set inserts (not M3)
horn_insert_depth = 3.6;    // M2 screws pass through horn INTO THESE inserts

// Previously successful right-side stand-offs: approx 19.5 mm.
side_base_x = 75;
side_base_length = 80;
side_base_width = 57;
side_base_thick = 5.5;
standoff_height = 19.5;
base_post_x = [55,87];
base_post_y = [-14,14];
beam_x_start = -19;
beam_x_end = 103;
beam_width = 47;
beam_thick = 7.0;

// 3-axis translation: X by the bridge slots, Y by stage slots, Z by 3 studs.
bridge_x_travel = 17.0;     // total travel; +/- 8.5
xy_y_travel = 17.0;         // total travel; +/- 8.5
stage_x_start = -21;
stage_x_end = 44;
stage_width = 52;
stage_thick = 5.0;
stage_attach_x = [10,28];
level_points = [[-14,-18],[-14,18],[35,0]];

// Servo plate: user-verified TS90MD but dimensional drawing varies by seller.
// These are safe-to-adjust prototype dimensions, not guaranteed flange drilling.
servo_body_x = 23.4;       // clearance dimension, measured unit can differ
servo_body_y = 13.3;
servo_body_offset_x = 7.5; // puts shaft at nominal X=0
servo_mount_hole_x = [-9.5,24.5];
servo_mount_slot_len = 5.0;
servo_mount_d = 2.2;       // M2 mounting fasteners / supplied servo screws
servo_plate_thick = 5.0;

// Simple 2D helpers
module rr2(w,h,r) {
    offset(r=r) square([max(0.1,w-2*r),max(0.1,h-2*r)],center=true);
}
module slot2(len,w) {
    hull() {
        translate([-(len-w)/2,0]) circle(d=w);
        translate([ (len-w)/2,0]) circle(d=w);
    }
}
module slot_y2(len,w) { rotate(90) slot2(len,w); }
module insert_down(x,y,z_top) {
    translate([x,y,z_top-insert_depth]) cylinder(d=insert_od_hole,h=insert_depth+0.1);
    translate([x,y,z_top-7]) cylinder(d=2.8,h=7.1);
}
module knob_outline() {
    // six-sided flattened profile, dimensions match measured envelope.
    // Exact Option C outline is unknown; use coupon to match fit.
    rotate(knob_clock_deg) offset(delta=knob_clearance)
    polygon(points=[[-knob_x/2,-knob_y/4],[0,-knob_y/2],
      [knob_x/2,-knob_y/4],[knob_x/2,knob_y/4],
      [0,knob_y/2],[-knob_x/2,knob_y/4]]);
}
module cap() {
    h=knob_engagement+cap_roof;
    difference() {
        union() {
            cylinder(d=cap_od,h=h);
            // External grip ribs stay outside the previously successful bore.
            for (i=[0:cap_rib_count-1])
                rotate([0,0,i*360/cap_rib_count])
                  translate([cap_od/2-0.3,-1.05,2.0])
                    cube([1.3,2.1,h-3.0]);
            // Wide base fillet reinforces key root where last design flexed.
            translate([0,0,h-0.05])
              linear_extrude(height=2.0)
                rr2(key_width+3.5,key_width+3.5,1.7);
            translate([0,0,h+1.5])
              linear_extrude(height=key_height-1.5)
                rr2(key_width,key_width,key_corner_radius);
        }
        translate([0,0,-0.1]) linear_extrude(height=knob_engagement+0.11)
            knob_outline();
        // Mechanical cross-bolt hole through male tenon (not a snap-fit).
        translate([-key_width/2-0.5,0,h+1.5+crossbolt_z])
          rotate([0,90,0]) cylinder(d=crossbolt_d,h=key_width+1.0);
    }
}
module coupling() {
    difference() {
        union() {
            cylinder(d=coupler_outer_d,h=coupler_total_height);
            // Fixed collar at the root makes plate/socket rigid.
            translate([0,0,coupler_socket_depth-0.4])
              cylinder(d=coupler_outer_d+1.0,h=2.0);
        }
        // Deep female socket on cap side. Long square key takes torque.
        translate([0,0,-0.05])
          linear_extrude(height=coupler_socket_depth+0.1)
            rr2(key_width+2*key_socket_clearance,
                key_width+2*key_socket_clearance,key_corner_radius+key_socket_clearance);
        // Servo original retaining screw passes axially UP through this hole.
        translate([0,0,coupler_socket_depth-0.02])
          cylinder(d=servo_screw_access_d,h=coupler_total_height-coupler_socket_depth+0.15);
        // Two M2 heat-set inserts are installed in the TOP face. Fasteners
        // from the factory horn into these inserts carry rotation torque;
        // the original servo retaining screw is tightened from below.
        for (sx=[-1,1]) {
          translate([sx*horn_hole_pitch/2,0,coupler_total_height-horn_insert_depth])
            cylinder(d=horn_insert_bore_d,h=horn_insert_depth+0.15);
          translate([sx*horn_hole_pitch/2,0,coupler_total_height-horn_insert_depth-2])
            cylinder(d=horn_fastener_d,h=2.1);
        }
        // One continuous bored profile avoids non-manifold overlapping
        // cylinders. M3 shaft clearance transitions to 4mm heat-set insert.
        translate([0,0,crossbolt_z]) rotate([0,90,0])
            rotate_extrude(convexity=5)
              polygon(points=[
                [0,-coupler_outer_d/2-0.3],
                [crossbolt_d/2,-coupler_outer_d/2-0.3],
                [crossbolt_d/2,coupler_outer_d/2-insert_depth-0.9],
                [insert_od_hole/2,coupler_outer_d/2-insert_depth+0.1],
                [insert_od_hole/2,coupler_outer_d/2+0.3],
                [0,coupler_outer_d/2+0.3] ]);
    }
}
module fit_coupon() {
    // A minimal knob-profile test before replacing the previously fitted cap.
    difference() {
      cylinder(d=cap_od,h=5.5);
      translate([0,0,-0.1]) linear_extrude(height=5.8) knob_outline();
    }
}
module side_base() {
    difference() {
        union() {
            translate([side_base_x,0,side_base_thick/2])
              cube([side_base_length,side_base_width,side_base_thick],center=true);
            for (x=base_post_x,y=base_post_y) {
                // Double front/rear towers, right-side mount is stiff.
                translate([x,y,side_base_thick])
                  cylinder(d=11.0,h=standoff_height);
                hull() {
                  translate([x,y,side_base_thick-0.01]) cylinder(d=17,h=0.5);
                  translate([x,y,side_base_thick+8]) cylinder(d=11,h=0.5);
                }
            }
        }
        for (x=base_post_x,y=base_post_y)
            insert_down(x,y,side_base_thick+standoff_height);
        // Tape underside intentionally SOLID; no magnet pockets or screws
        // through the cooled surface. Verify surface temperature first.
    }
}
module bridge() {
    difference(){
        union(){
          translate([(beam_x_start+beam_x_end)/2,0,beam_thick/2])
            cube([beam_x_end-beam_x_start,beam_width,beam_thick],center=true);
          // Side flanges form a rigid channel beam (unlike thin U-arm).
          for (yy=[-1,1])
            translate([(beam_x_start+beam_x_end)/2,yy*(beam_width/2-3),beam_thick+5])
              cube([beam_x_end-beam_x_start-3,6,10],center=true);
        }
        // 4 X slots matching heat-set towers on the taped base.
        for(x=base_post_x,y=base_post_y)
          translate([x,y,-0.1]) linear_extrude(height=beam_thick+0.2)
            slot2(bridge_x_travel+3.5,3.5);
        // Two M3 inserts on top to hold XY translation stage.
        for(x=stage_attach_x) insert_down(x,0,beam_thick);
    }
}
module xy_stage() {
    difference(){
        translate([(stage_x_start+stage_x_end)/2,0,stage_thick/2])
          cube([stage_x_end-stage_x_start,stage_width,stage_thick],center=true);
        // M3 bolts through ±Y slots into bridge inserts.
        for (x=stage_attach_x)
          translate([x,0,-0.1]) linear_extrude(height=stage_thick+0.2)
            slot_y2(xy_y_travel+3.5,3.5);
        // Three 4 mm OD heatset inserts receive adjustable M3 studs.
        // Nuts above/below servo plate lock height + pitch/yaw tilt.
        for (p=level_points) insert_down(p[0],p[1],stage_thick);
    }
}
module servo_plate() {
    difference() {
        translate([(stage_x_start+stage_x_end)/2,0,servo_plate_thick/2])
            cube([stage_x_end-stage_x_start,stage_width,servo_plate_thick],center=true);
        // Three point / 3-stud floating plate supports Z + 2 tilt axes.
        for(p=level_points)
            translate([p[0],p[1],-0.1]) cylinder(d=4.3,h=servo_plate_thick+0.2);
        // TS90MD body drops into this opening from the FRONT; the factory
        // servo ears rest on plate and screws pass through slots.
        translate([servo_body_offset_x,0,-0.1])
          linear_extrude(height=servo_plate_thick+0.2)
            rr2(servo_body_x,servo_body_y,0.6);
        for (x=servo_mount_hole_x)
          translate([x,0,-0.1])
            linear_extrude(height=servo_plate_thick+0.2)
              slot2(servo_mount_slot_len+servo_mount_d,servo_mount_d);
    }
}
module dummy_servo(){
    // Visualization only, does not affect STLs.
    color([0.10,0.26,0.7,0.78])
      translate([servo_body_offset_x,0,0]) cube([22.7,12.2,25.5],center=true);
}
module assembly() {
    color([0.34,0.40,0.43]) side_base();
    // Bridge sits on base towers, x/y centered at nominal position.
    translate([0,0,side_base_thick+standoff_height])
       color([0.12,0.55,0.66]) bridge();
    translate([0,0,side_base_thick+standoff_height+beam_thick])
       color([0.2,0.66,0.56]) xy_stage();
    translate([0,0,side_base_thick+standoff_height+beam_thick+stage_thick+5])
       color([0.2,0.4,0.74]) servo_plate();
    // 3 leveling studs are approximate VISUAL guides only.
    for(p=level_points)
       color([0.70,0.70,0.72])
       translate([p[0],p[1],side_base_thick+standoff_height+beam_thick+stage_thick-1])
         cylinder(d=3,h=17);
    color([0.25,0.25,0.28]) cap();
    translate([0,0,knob_engagement+cap_roof+1.5]) color([0.91,0.51,0.16]) coupling();
    translate([0,0,side_base_thick+standoff_height+beam_thick+stage_thick+5+servo_plate_thick+14]) dummy_servo();
}

if(part=="cap") cap();
else if(part=="coupling") coupling();
else if(part=="side_base") side_base();
else if(part=="bridge") bridge();
else if(part=="xy_stage") xy_stage();
else if(part=="servo_plate") servo_plate();
else if(part=="fit_coupon") fit_coupon();
else assembly();
