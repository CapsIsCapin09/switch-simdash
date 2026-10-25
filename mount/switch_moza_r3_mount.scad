// Nintendo Switch (V1, tablet only) mount for the Moza R3 wheel base.
//
// Two printed parts joined by two M4 bolts, so the tilt is adjustable:
//   foot   - bolts to the two M3 holes on top of the R3 (where Moza-style
//            dash / phone mounts go) and carries two hinge clevises.
//   cradle - holds the Switch. The top edge is left open so you slide the
//            Switch in from above, and so the top vents, power/volume buttons,
//            game card slot and headphone jack stay clear. The bottom ledge
//            has a gap for a USB-C cable.
//
// Render one part at a time:  openscad -D 'part="foot"' -o foot.stl this.scad
// part = "foot" | "cradle" | "assembly" (preview only, not for printing)
//
// Measurements marked CHECK are best guesses; measure yours with calipers.

part = "assembly";

/* [Moza R3 top mount] */
r3_hole_spacing = 90;   // CHECK: centre-to-centre of the two M3 holes on top of the R3
r3_screw_d      = 3.4;  // M3 clearance hole
r3_head_d       = 6.4;  // M3 socket head counterbore
r3_head_h       = 3.2;  // counterbore depth (leaves plate_t - this under the head)
plate_t         = 6;    // foot plate thickness
plate_w         = 22;   // foot plate depth (front to back)
plate_l         = r3_hole_spacing + 16;

/* [Switch V1 console] */
sw_w   = 173.0;  // CHECK: width without Joy-Cons
sw_h   = 101.9;  // CHECK: height
sw_t   = 13.9;   // CHECK: thickness
fit    = 0.6;    // total clearance added to each dimension

/* [Cradle] */
back_t     = 3;     // back plate thickness
wall       = 3;     // side / bottom wall thickness
lip        = 4;     // how far the lips reach over the front bezel
lip_t      = 2;     // lip thickness
side_len   = 65;    // how far up the sides the side walls go (top stays open)
usb_gap    = 34;    // gap in the bottom ledge for the USB-C plug
window_m   = 14;    // frame width left around the back window

/* [Hinge] */
hinge_x     = 27;   // hinge centre distance from the middle (each side)
hinge_h     = 30;   // CHECK: hinge axis height above the R3 top; raise to clear your wheel
tongue_w    = 10;   // cradle tongue width
cheek_w     = 5;    // foot cheek width
hinge_gap   = 0.3;  // side clearance between tongue and cheeks
tongue_t    = 12;   // tongue thickness (hinge axis is in the middle)
tongue_len  = 12;   // ledge bottom to hinge axis
bolt_d      = 4.4;  // M4 clearance
nut_af      = 7.2;  // M4 nut across flats + clearance
nut_h       = 3.4;  // nut trap depth
tilt        = 15;   // assembly preview only: lean back in degrees

$fn = 64;
eps = 0.01;

W = sw_w + fit;
H = sw_h + fit;
T = sw_t + fit;
cw = W + 2 * wall;          // cradle outer width
wall_h = back_t + T + lip_t; // wall height from the back face

// ---------- cradle (modelled lying on its back, screen facing +z) ----------
module cradle() {
    difference() {
        union() {
            // back plate
            translate([-cw / 2, -wall, 0]) cube([cw, H + wall, back_t]);
            // side walls with lips
            for (s = [-1, 1]) mirror([s < 0 ? 1 : 0, 0, 0]) {
                translate([W / 2, -wall, 0]) cube([wall, side_len + wall, wall_h]);
                translate([W / 2 - lip, -wall, wall_h - lip_t]) cube([lip + wall, side_len + wall, lip_t]);
            }
            // bottom ledge with lip
            translate([-cw / 2, -wall, 0]) cube([cw, wall, wall_h]);
            translate([-cw / 2, -wall, wall_h - lip_t]) cube([cw, lip + wall, lip_t]);
            // hinge tongues
            for (s = [-1, 1]) translate([s * hinge_x, -wall - tongue_len, tongue_t / 2])
                hull() {
                    rotate([0, 90, 0]) cylinder(d = tongue_t, h = tongue_w, center = true);
                    translate([-tongue_w / 2, tongue_len - eps, -tongue_t / 2]) cube([tongue_w, eps, tongue_t]);
                }
            // tie the tongues into the ledge
            translate([-hinge_x - tongue_w / 2, -wall - eps, 0]) cube([2 * hinge_x + tongue_w, wall, tongue_t]);
        }
        // USB-C gap in the bottom ledge and lip
        translate([-usb_gap / 2, -wall - 1, back_t]) cube([usb_gap, wall + lip + 2, wall_h]);
        // back window (rear vent, less plastic); stops short of the ledge
        translate([-W / 2 + window_m, window_m, -1]) cube([W - 2 * window_m, H - 2 * window_m, back_t + 2]);
        // hinge bolt holes
        for (s = [-1, 1]) translate([s * hinge_x, -wall - tongue_len, tongue_t / 2])
            rotate([0, 90, 0]) cylinder(d = bolt_d, h = tongue_w + 2, center = true);
    }
}

// ---------- foot (modelled as printed, plate on the bed) ----------
cheek_off = tongue_w / 2 + hinge_gap + cheek_w / 2;
axis_z = plate_t + hinge_h;

module cheek() {
    hull() {
        translate([-cheek_w / 2, -plate_w / 2, 0]) cube([cheek_w, plate_w, plate_t]);
        translate([0, 0, axis_z]) rotate([0, 90, 0]) cylinder(d = tongue_t, h = cheek_w, center = true);
    }
}

module foot() {
    difference() {
        union() {
            translate([-plate_l / 2, -plate_w / 2, 0]) cube([plate_l, plate_w, plate_t]);
            for (s = [-1, 1], c = [-1, 1]) translate([s * hinge_x + c * cheek_off, 0, 0]) cheek();
        }
        // R3 screw holes, counterbored from the top
        for (s = [-1, 1]) translate([s * r3_hole_spacing / 2, 0, 0]) {
            translate([0, 0, -1]) cylinder(d = r3_screw_d, h = plate_t + 2);
            translate([0, 0, plate_t - r3_head_h]) cylinder(d = r3_head_d, h = r3_head_h + 1);
        }
        for (s = [-1, 1]) translate([s * hinge_x, 0, axis_z]) {
            // bolt through both cheeks
            rotate([0, 90, 0]) cylinder(d = bolt_d, h = 2 * cheek_off + cheek_w + 2, center = true);
            // hex nut trap on the outer cheek
            translate([s * (cheek_off + cheek_w / 2 - nut_h / 2 + eps), 0, 0])
                rotate([0, 90, 0]) rotate([0, 0, 30]) cylinder(d = nut_af / cos(30), h = nut_h, center = true, $fn = 6);
        }
    }
}

module assembly() {
    color("dimgray") foot();
    // stand the cradle up on the hinge axis and lean it back
    translate([0, 0, axis_z]) rotate([90 - tilt, 0, 0])
        translate([0, wall + tongue_len, -tongue_t / 2]) {
            color("steelblue") cradle();
            // Switch for reference
            color("black") translate([-sw_w / 2 + 0.3, 0.3, back_t]) cube([sw_w, sw_h, sw_t]);
        }
}

if (part == "foot") foot();
else if (part == "cradle") cradle();
else assembly();
