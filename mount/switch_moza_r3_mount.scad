// Nintendo Switch (V1, tablet only) mount for the Moza R3 wheel base.
//
// Two versions:
//
// Flush (one piece, part = "flush"): the cradle sits straight on a plate that
// screws to the R3, leaning back at a fixed angle. Print it standing up, plate
// on the bed. Simplest and sturdiest if the wheel hub doesn't hide the screen.
//
// Raised (part = "foot" + "cradle"): two printed parts joined by two M4 bolts,
// so the height clears the wheel hub and the tilt is adjustable:
//   foot   - bolts to the two M3 holes on top of the R3 (where Moza-style
//            dash / phone mounts go) and carries two hinge clevises.
//   cradle - holds the Switch. The top edge is left open so you slide the
//            Switch in from above, and so the top vents, power/volume buttons,
//            game card slot and headphone jack stay clear. The bottom ledge
//            has a gap for a USB-C cable.
//
// Render one part at a time:  openscad -D 'part="foot"' -o foot.stl this.scad
// part = "flush" | "foot" | "cradle" | "assembly" | "flush_assembly"
//        (the two *assembly parts are previews with a Switch, not for printing)
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
window_m   = 14;    // frame width left around the back vent slots
slots      = 5;     // vent slots in the back (pointed tops print without support)
slot_rib   = 6;     // rib width between slots

/* [Flush version] */
flush_tilt    = 0;   // lean back in degrees (0 = straight up; the R3 already sits tilted)
cable_clear   = 8;   // room under the Switch for a flat side-angle USB-C plug; 0 if you never charge in it
flush_plate_w = 30;  // plate depth front to back
brace_len     = 30;  // how far the riser braces reach back on the R3 (raised version only)
flush_raise   = 0;   // extra height under the Switch so the screen clears the wheel rim (60 clears William's rim)
flush_back    = 40;  // move the Switch this far back from the screw holes (the plate reaches back to it)

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
// vent slots: rectangles with 45 degree pointed tops
module back_slots() {
    sw = (W - 2 * window_m - (slots - 1) * slot_rib) / slots;
    sh = H - 2 * window_m;
    for (i = [0 : slots - 1]) translate([-W / 2 + window_m + i * (sw + slot_rib), window_m, -1])
        linear_extrude(back_t + 2)
            polygon([[0, 0], [sw, 0], [sw, sh - sw / 2], [sw / 2, sh], [0, sh - sw / 2]]);
}

module cradle(tongues = true) {
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
            if (tongues) for (s = [-1, 1]) translate([s * hinge_x, -wall - tongue_len, tongue_t / 2])
                hull() {
                    rotate([0, 90, 0]) cylinder(d = tongue_t, h = tongue_w, center = true);
                    translate([-tongue_w / 2, tongue_len - eps, -tongue_t / 2]) cube([tongue_w, eps, tongue_t]);
                }
            // tie the tongues into the ledge
            if (tongues) translate([-hinge_x - tongue_w / 2, -wall - eps, 0]) cube([2 * hinge_x + tongue_w, wall, tongue_t]);
        }
        // USB-C gap in the bottom ledge and lip
        translate([-usb_gap / 2, -wall - 1, back_t]) cube([usb_gap, wall + lip + 2, wall_h]);
        back_slots();
        // hinge bolt holes
        if (tongues) for (s = [-1, 1]) translate([s * hinge_x, -wall - tongue_len, tongue_t / 2])
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

// ---------- flush one-piece version (modelled as printed, plate on the bed) ----------
flush_a = 90 - flush_tilt;
// height of the Switch's bottom edge above the plate
flush_lift = max(wall * sin(flush_a) + 1, cable_clear) + flush_raise;
flush_y = -wall_h * sin(flush_a) / 2 + 3;  // plate centre, slightly behind the cradle's middle

module flush_cradle() {
    translate([0, 0, plate_t + flush_lift]) rotate([flush_a, 0, 0]) cradle(false);
}

module flush_ledge() {
    translate([0, 0, plate_t + flush_lift]) rotate([flush_a, 0, 0])
        translate([-cw / 2, -wall, 0]) cube([cw, wall, wall_h]);
}

plate_y0 = min(flush_y - flush_plate_w / 2, flush_back - wall_h - 3);
plate_y1 = max(flush_y + flush_plate_w / 2, flush_back + (flush_raise > 15 ? brace_len : 8));

// the Switch-holding part: cradle, plinth and riser, before moving it back
module flush_body() {
    flush_cradle();
    // solid plinth from the plate up to the bottom ledge
    hull() {
        flush_ledge();
        linear_extrude(plate_t) projection() flush_ledge();
    }
    // braces behind a tall riser so it doesn't rock under force feedback
    if (flush_raise > 15) for (s = [-1, 1])
        translate([s * (usb_gap / 2 + 4) - 4, 0, 0]) rotate([90, 0, 90]) linear_extrude(8)
            polygon([[-1, 0], [brace_len, 0], [brace_len, plate_t], [-1, plate_t + flush_lift]]);
}

module flush() {
    difference() {
        union() {
            translate([-plate_l / 2, plate_y0, 0]) cube([plate_l, plate_y1 - plate_y0, plate_t]);
            translate([0, flush_back, 0]) flush_body();
            // ribs stiffening the plate between the screws and the set-back Switch
            if (flush_back > 15) for (s = [-1, 1])
                translate([s * (usb_gap / 2 + 4) - 4, flush_y - 8, 0])
                    cube([8, flush_back - wall_h - flush_y + 8 + eps, plate_t + flush_lift]);
        }
        // R3 screw holes, counterbored from the top
        for (s = [-1, 1]) translate([s * r3_hole_spacing / 2, flush_y, 0]) {
            translate([0, 0, -1]) cylinder(d = r3_screw_d, h = plate_t + 2);
            translate([0, 0, plate_t - r3_head_h]) cylinder(d = r3_head_d, h = 200);
        }
        translate([0, flush_back, 0]) flush_cuts();
    }
}

module flush_cuts() {
        // lighten a tall riser with pointed windows front to back (no bridges)
        if (flush_raise > 15) {
            rib = 8;
            ww = (cw - 2 * rib - usb_gap - 2 * rib) / 4;  // two windows each side of the cable channel
            wh = flush_lift - 6;
            for (s = [-1, 1], i = [0, 1])
                translate([s * (usb_gap / 2 + rib + i * (ww + rib)) - (s < 0 ? ww : 0), 60, plate_t])
                    rotate([90, 0, 0]) linear_extrude(120)
                        polygon([[0, 0], [ww, 0], [ww, max(0, wh - ww / 2)], [ww / 2, wh], [0, max(0, wh - ww / 2)]]);
        }
        // USB-C cable channel under the Switch, open front to back
        if (cable_clear > 0)
            translate([-usb_gap / 2, -60, plate_t]) cube([usb_gap, 120, flush_lift + back_t * cos(flush_a) + eps]);
}

module flush_assembly() {
    color("dimgray") flush();
    translate([0, flush_back, plate_t + flush_lift]) rotate([flush_a, 0, 0])
        color("black") translate([-sw_w / 2 + 0.3, 0.3, back_t]) cube([sw_w, sw_h, sw_t]);
}

if (part == "flush") flush();
else if (part == "flush_assembly") flush_assembly();
else if (part == "foot") foot();
else if (part == "cradle") cradle();
else assembly();
