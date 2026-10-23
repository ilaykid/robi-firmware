// =====================================================================
//  ROBI - a cute desk robot enclosure for the
//  Waveshare ESP32-S3-Touch-AMOLED-1.75 (SKU 31261, version WITHOUT case)
//  + 2030 cavity speaker (30x20x6.8) + 1S LiPo (40x30x10, e.g. 4project 1200mAh)
//
//  All board positions come from Waveshare's official dimension drawing
//  (ESP32-S3-Touch-AMOLED-1.75-details-size.jpg). Units: mm.
//
//  Coordinate system (assembled):
//    X = right as you LOOK AT the robot's face
//    Y = depth, 0 = front face of the head, +Y goes backwards
//    Z = up
//
//  Select a part with -D 'part="head_front"' etc. part="assembly" shows all.
//
//  v3 (2026-10-23) - review before the first print. Changes are marked "v3:".
//   - face window 46 -> 47 mm with a full-depth 45 deg chamfer (finger reaches
//     ~97% of the screen height instead of ~95%; touch UI is now central)
//   - PWR plunger: v2 retaining cone overlapped the channel step by 0.25 mm,
//     leaving ~0.05 mm press travel. Re-derived; travel now 0.6 mm, 3 lengths.
//   - neck: v2 could not be assembled (both flanges 17 mm > 14.4 mm body hole)
//     and carried no weight. Now: collar that the head sits on, spigot glued
//     into a countersunk body top, flat on the top flange; the back head shell
//     gets a U-slot so it slides onto the neck (v2 had 0.2 mm interference).
//   - base: side screws removed (their tips pointed at the LiPo, and the tabs
//     blocked packs longer than 42 mm). Two rear hooks + the two front screws.
//     Battery tray (front curb), speaker support pillars (speaker used to drop
//     ~4 mm onto the base) and longer feet at the back against tipping on taps.
//   - board posts: post_gap 0.10 -> 0.25 so the glass is never clamped to the lip.
// =====================================================================

part = "assembly";   // assembly | head_front | head_back | body | base | neck
                     // antenna | ear_left | ear_right | pwr_button | fit_test | all_plate

$fn = 72;
eps = 0.01;

// ---------------- printer tolerances ----------------
clr      = 0.20;     // general sliding clearance
screw_clear = 2.30;  // M2 clearance hole
screw_pilot = 1.75;  // M2 self-tapping pilot hole in PLA/PETG
screw_head  = 4.40;  // M2 cap-head counterbore

// ---------------- board (Waveshare drawing) ----------------
glass_d      = 48.96;  // cover glass outer diameter
pcb_d        = 46.00;
// v3: window 46 -> 47 and chamfer 0.8 -> 1.0 (45 deg, leaves a 0.2 mm land).
// A 12 mm fingertip can now centre ~20.9 mm from the screen centre (v2: 20.2),
// i.e. ~97% of the screen height at the bottom edge. Glass still overlaps the
// lip by ~1.0 mm per side (>= 0.66 mm with the glass fully off-centre in its pocket);
// the lip is only a forward stop - the board is held by the screws/posts.
view_d       = 47.00;  // front window at the glass (active area ~44.5)
bezel_ch     = 1.0;    // 45 deg chamfer on the front window edge (< lip_t)
lip_t        = 1.20;   // face lip in front of the glass (glass front sits this deep)
pocket_d     = glass_d + 0.64;   // glass pocket
// depths measured from the glass front surface (side view of drawing)
st_pcb_back   = 6.90;
st_insert_top = 10.40;   // brass M2 standoffs H3.5 on the back of the PCB
st_usb_c      = 8.55;    // USB-C centre depth  (body ~7.3 .. 9.8)
st_btn        = 7.90;    // PWR / BOOT side-push buttons
st_mic_top    = 7.90;    // top-port MEMS mics on PCB back
st_conn_top   = 11.70;   // SPK / BAT MX1.25 connectors
st_header_top = 12.30;   // 8-pin header
// positions in FRONT view (mirrored from the back-view drawing)
inserts   = [[0, 20.5], [13.75, -14.7], [-13.75, -14.7]];
usb_r     = 23.8;              // USB-C face, left side (x<0), z = 0
pwr_ang   = 34;                // PWR button: right side, +34 deg
boot_ang  = -34;               // BOOT button: right side, -34 deg
mics      = [[-18.2, 10.8], [-18.2, -10.8]];   // re-measured on the labelled photo
conns     = [[7.2, 17.7], [-7.2, 17.7]];   // SPK , BAT
header_box = [[-10.6, -19.8], [11.0, -16.8]]; // keep-out of the 8-pin header (centred)

// ---------------- head ----------------
head_w = 66; head_h = 62; head_d = 24; head_r = 18;
wall   = 2.2;
face_t = 2.6;               // face plate thickness
back_t = 2.4;
ch     = 2.0;               // outer chamfer front & back
seam_ch = 0.6;              // decorative chamfer at the seam
post_gap = 0.25;                     // v3: 0.10 -> 0.25. Screws pull the board back onto the
                                     // posts; the glass then floats this far behind the lip,
                                     // so a thicker-than-drawn board can't be clamped on its glass
gf       = lip_t + post_gap;         // glass front depth when assembled (v3: + post_gap)
y_pcb    = gf + st_pcb_back;         // 8.35
y_ins    = gf + st_insert_top;       // 11.85 (post face)
y_usb    = gf + st_usb_c;            // 10.0
y_btn    = gf + st_btn;              // 9.35
y_mic    = gf + st_mic_top;          // 9.35
split_y  = 9.6;                      // front / back shell split
back_in  = head_d - back_t;          // 21.6 inner back wall
neck_y   = 12.0;                     // neck axis depth (both head and body)

// corner screws that join the two head shells (M2 x 12)
shell_screws = [[24.5, -22.5], [-24.5, -22.5], [23.5, 23.5], [-23.5, 23.5]];
board_screw_len = 10;   // M2 x 10 into the brass standoffs (3 mm engagement)
shell_screw_len = 12;

// USB-C opening for the plug over-mould
usb_open = [14.0, 8.4];   // along Z , along Y  (fits plug over-moulds up to ~13 x 7.5)
ear_d = 19; ear_t = 3.0; ear_peg = 6.8;   // pegs sit front/back of the ear centre
ant_y = 15.0;  ant_peg_d = 4.0;
pwr_tip_r = 24.0;   // where the PWR plunger tip rests (switch actuator). ESTIMATE - not on the drawing.
pwr_travel = 0.75;  // v3: cap-to-wall gap on the axis; the cap rim meets the curved corner
                    // ~0.15 earlier, so a finger can push ~0.6 mm (switch needs ~0.25)
pwr_play   = 0.25;  // v3: outward float before the retaining cone meets the channel step
pwr_adj    = [-0.4, 0, 0.4];   // v3: printed plunger lengths (short / std / long)
// microSD slot: on the FRONT side of the PCB, right edge (as you look at the face)
sd_y = [gf + 2.6, gf + 7.0];   // depth range of the card opening (v3: follows gf; v2 3.8..8.2)
sd_z = [-6.5, 8.5];      // height range
ear_r_pegs = [[12.0, 6.5], [12.0, -6.5]];   // right ear pegs (y, z) - behind the SD opening

// ---------------- neck ----------------
neck_od = 14.0; neck_id = 10.0; neck_flat = 6.2;  // flat on the back side (anti-rotation)
neck_fl_d = 17.0; neck_fl_t = 1.8;
neck_gap  = 5.0;            // visible neck between head and body
// v3: the visible part is now a 17 mm collar: it sits in a 45 deg countersink in
// the body top and the head rests on it. Below the collar a D-shaped spigot is
// glued into the body top (the body is one piece, so no flange can go inside it).
neck_spig = 2.0;            // spigot length below the body ceiling
neck_cs   = (neck_fl_d - neck_od) / 2;   // 1.5 - collar cone / body countersink depth

// ---------------- body ----------------
body_w = 54; body_d = 27; body_h = 38; body_r = 9;
body_y0 = neck_y - body_d/2;          // body front
body_top_t = 2.4;
spk = [30.0, 6.8, 20.0];              // speaker  x , y , z
spk_z0 = 6.0;                         // speaker bottom (above body bottom rim)
bat = [46.0, 11.5, 31.0];             // v3: largest pack envelope (incl. protection PCB) that fits
base_t = 3.0; base_lip = 2.0;
body_screws = [[22.5, body_y0 + 4.2], [-22.5, body_y0 + 4.2]];   // vertical, from below (M2 x 8)
// v3: the two side screws (M2x6 into base tabs) are gone - their tips ended 1 mm from
// the battery and the tabs limited the pack to 42 mm. The back of the base is now
// held by two hooks that click into windows in the body's back wall.
hook_x = 12; hook_w = 8; hook_t = 1.6; hook_h = 8.0;   // tongues on the base, at x = +-hook_x
hook_lip = 1.0; hook_lip_h = 1.6;                     // lip that enters the back wall
bat_y0  = body_y0 + wall + spk[1] + clr + 1.4;        // battery front face (= back of speaker rails, 9.1)
bat_curb = 5.0;                                       // low curb on the base in front of the battery
base_heel = 14;            // v3: feet reach this far behind the body (stability when the screen is tapped)

// world placement
head_cz   = base_t + body_h + neck_gap + head_h/2;   // head centre height
body_z0   = base_t;                                  // body bottom

// =====================================================================
//  helpers
// =====================================================================
module rrect(w, h, r) {           // 2D rounded rectangle centred
    r2 = min(r, w/2 - eps, h/2 - eps);
    offset(r = r2) square([w - 2*r2, h - 2*r2], center = true);
}
// extrude a 2D shape (drawn in X/Z as X/Y) along +Y from y0 to y1
module along_y(y0, y1) {
    rotate([90, 0, 0]) translate([0, 0, -y1]) linear_extrude(y1 - y0) children();
}
// chamfered rounded prism along Y. c0 = chamfer at y0, c1 = chamfer at y1
module rprism_y(w, h, r, y0, y1, c0 = 0, c1 = 0) {
    hull() {
        if (c0 > 0) along_y(y0, y0 + eps) rrect(w - 2*c0, h - 2*c0, r - c0);
        along_y(y0 + c0, y1 - c1) rrect(w, h, r);
        if (c1 > 0) along_y(y1 - eps, y1) rrect(w - 2*c1, h - 2*c1, r - c1);
    }
}
// cylinder along Y
module cyl_y(d, y0, y1, x = 0, z = 0) {
    translate([x, y1, z]) rotate([90, 0, 0]) cylinder(d = d, h = y1 - y0);
}
// cylinder along a radial direction in the X/Z plane
module radial(ang, r0, r1, d, y) {
    translate([0, y, 0]) rotate([0, -ang, 0]) translate([r0, 0, 0])
        rotate([0, 90, 0]) cylinder(d = d, h = r1 - r0);
}
// distance from centre to the outer head wall along an angle (approx, rounded rect)
function rr_ray(w, h, r, ang) =
    let(c = cos(ang), s = sin(ang),
        tx = (w/2) / max(abs(c), 1e-6), tz = (h/2) / max(abs(s), 1e-6),
        t0 = min(tx, tz), px = abs(c)*t0, pz = abs(s)*t0,
        cx = w/2 - r, cz = h/2 - r)
    (px > cx && pz > cz)
        ? let(b = cx*abs(c) + cz*abs(s), q = cx*cx + cz*cz - r*r) b + sqrt(b*b - q)
        : t0;

// =====================================================================
//  HEAD
// =====================================================================
module head_outer() { rprism_y(head_w, head_h, head_r, 0, head_d, ch, ch); }
module head_inner() {
    // cavity
    along_y(face_t, back_in) rrect(head_w - 2*wall, head_h - 2*wall, head_r - wall);
}
module head_cuts() {
    // front window with bezel chamfer
    translate([0, -eps, 0]) rotate([-90, 0, 0])
        cylinder(d1 = view_d + 2*bezel_ch, d2 = view_d, h = bezel_ch + eps);
    cyl_y(view_d, -1, lip_t + eps);
    // glass pocket
    cyl_y(pocket_d, lip_t, face_t + eps);
    // cheeks (shallow round dimples to paint)
    for (sx = [-1, 1]) translate([sx*18, -eps, -25]) rotate([-90, 0, 0]) cylinder(d = 6, h = 0.6);
    // USB-C opening (left side)
    translate([-head_w/2 - 1, y_usb, 0])
        rotate([0, 90, 0]) linear_extrude(head_w/2 - usb_r + 1.2)
            rrect(usb_open[0], usb_open[1], 3.2);
    // PWR plunger channel (right, +34 deg) + counterbore for its cap
    pr = rr_ray(head_w, head_h, head_r, pwr_ang);
    radial(pwr_ang, 20, pr + 1, 3.7, y_btn);
    radial(pwr_ang, pr - 6.5, pr - 2.2, 6.0, y_btn);   // room for the retaining flange
    // BOOT pin-hole (right, -34 deg) - press with a paper clip
    radial(boot_ang, 22, rr_ray(head_w, head_h, head_r, boot_ang) + 1, 1.8, y_btn);
    // ear pegs
    for (sy = [-1, 1])
        translate([-(head_w/2 - wall/2), y_usb + sy*ear_peg, 0]) rotate([0, 90, 0])
            cylinder(d = 2.3, h = wall + 2, center = true);
    for (p = ear_r_pegs)
        translate([head_w/2 - wall/2, p[0], p[1]]) rotate([0, 90, 0])
            cylinder(d = 2.3, h = wall + 2, center = true);
    // microSD slot (right side). Open up to the split line so it prints without bridges.
    translate([23.0, sd_y[0], sd_z[0]]) cube([head_w/2 - 23.0 + 1, split_y - sd_y[0] + eps, sd_z[1] - sd_z[0]]);
    // antenna socket
    translate([0, ant_y, head_h/2 - 6]) cylinder(d = ant_peg_d + 0.25, h = 10);
    translate([0, ant_y, head_h/2 - 1.75 - clr]) cylinder(d1 = ant_peg_d + 2*clr, d2 = 7.5 + 2*clr, h = 1.75 + clr + eps);
    // neck socket + seat for the top flange
    neck_socket();
    // v3: the socket straddles the shell split (neck_y 12 vs split_y 9.6), so the back
    // shell held more than half the circle: its 13.6 mm mouth could not pass the 14 mm
    // neck. Open the back-shell half into a U-slot towards the split; the front shell's
    // arc still stops the neck moving forward.
    intersection() {
        hull() { neck_socket(); translate([0, split_y - neck_y, 0]) neck_socket(); }
        translate([-50, split_y, -head_h/2 - 5]) cube([100, 50, 20]);
    }
    // mic ports (back)
    for (m = mics) translate([m[0], 0, m[1]]) cyl_y(1.6, y_mic + 0.9, head_d + 1);
    // back vents
    for (i = [-2:2]) translate([i*5, back_in - 1, -head_h/2 + 9])
        rotate([-90, 0, 0]) linear_extrude(back_t + 2) rrect(2.0, 9, 1);
}
module neck_socket() {
    // D-hole through the head floor (anti-rotation flat at the back)
    translate([0, neck_y, -head_h/2 - 1]) linear_extrude(wall + 2) neck_2d(neck_od + 2*clr, true);
    // 45 deg seat for the neck's top flange. v3: flat at the back as well, so the
    // anti-rotation flat engages over the whole flange, not only 0.5 mm of stem.
    intersection() {
        translate([0, neck_y, -head_h/2 + wall - (neck_fl_d - neck_od + 0.4)/2 - clr])
            cylinder(d1 = neck_od - 0.4 + 2*clr, d2 = neck_fl_d + 2*clr, h = (neck_fl_d - neck_od + 0.4)/2 + clr + eps);
        translate([-20, neck_y - 20, -head_h/2 - 1]) cube([40, 20 + neck_flat + clr, wall + 2]);
    }
}
module neck_2d(d, flat) {
    intersection() {
        circle(d = d);
        if (flat) translate([-d, -d]) square([2*d, d + neck_flat + (d - neck_od)/2]);
        else circle(d = d + 1);
    }
}
// solid features hanging off the inside
module head_bosses_front() {
    for (s = shell_screws) translate([s[0], 0, s[1]])
        difference() {
            cyl_y(6.0, face_t - eps, split_y);
            cyl_y(screw_pilot, split_y - 6.5, split_y + 1);
        }
}
module head_bosses_back() {
    // shell screws
    for (s = shell_screws) translate([s[0], 0, s[1]]) cyl_y(6.0, split_y, back_in + eps);
    // board posts
    for (p = inserts) translate([p[0], 0, p[1]])
        difference() {
            cyl_y(5.0, y_ins, back_in + eps);
            header_keepout(1.0);
        }
    // mic tubes
    for (m = mics) translate([m[0], 0, m[1]]) cyl_y(4.4, y_mic + 0.9, back_in + eps);
    // PWR plunger guide tube
    pr = rr_ray(head_w, head_h, head_r, pwr_ang);
    radial(pwr_ang, pr - 7.5, pr - 0.5, 7.0, y_btn);
}
module header_keepout(extra = 0) {
    translate([header_box[0][0] - extra, gf, header_box[0][1] - extra])
        cube([header_box[1][0] - header_box[0][0] + 2*extra,
              st_header_top + 1.2,
              header_box[1][1] - header_box[0][1] + 2*extra]);
}
module head_back_holes() {
    for (s = shell_screws) translate([s[0], 0, s[1]]) {
        cyl_y(screw_clear, split_y - 1, head_d + 1);
        cyl_y(screw_head, split_y - 5.5 + shell_screw_len, head_d + 1);
    }
    for (p = inserts) translate([p[0], 0, p[1]]) {
        cyl_y(screw_clear, y_ins - 1, head_d + 1);
        // M2x10: 3.0 mm into the 3.5 mm standoff, tip stops 0.5 mm above the PCB
        cyl_y(screw_head, y_ins - 3.0 + board_screw_len, head_d + 1);
    }
}
module head_full() {
    difference() {
        union() {
            difference() { head_outer(); head_inner(); }
            head_bosses_front();
            head_bosses_back();
        }
        head_cuts();
        head_back_holes();
        // the PWR channel goes through the guide tube too
        radial(pwr_ang, 20, rr_ray(head_w, head_h, head_r, pwr_ang) + 1, 3.7, y_btn);
    }
}
module seam_groove() {
    // small V-groove around the outside of the split line
    difference() {
        along_y(split_y - seam_ch, split_y + seam_ch) rrect(head_w + 2, head_h + 2, head_r + 1);
        hull() {
            along_y(split_y - seam_ch, split_y - seam_ch + eps) rrect(head_w, head_h, head_r);
            along_y(split_y, split_y + eps) rrect(head_w - 2*seam_ch, head_h - 2*seam_ch, head_r - seam_ch);
            along_y(split_y + seam_ch - eps, split_y + seam_ch) rrect(head_w, head_h, head_r);
        }
    }
}
module head_front() {
    difference() {
        head_full();
        translate([-100, split_y, -100]) cube(200);
    }
}
module head_back() {
    difference() {
        head_full();
        translate([-100, split_y - 200, -100]) cube(200);
    }
}

// =====================================================================
//  NECK  (built standing on its bottom flange, z = 0)
// =====================================================================
// v3 neck, from the bottom: D-spigot (glued in the body top) | collar (visible, the head
// sits on it; 45 deg cone underneath sits in the body countersink) | D-stem through the
// head floor | top flange inside the head (45 deg underside, flat at the back).
neck_zc  = neck_spig + body_top_t;                   // collar bottom = body top surface
neck_len = neck_zc + neck_gap + wall + clr + neck_fl_t;
neck_z0  = base_t + body_h - neck_zc;                // world z of the spigot end
module neck() {
    difference() {
        union() {
            linear_extrude(neck_len) neck_2d(neck_od, true);
            translate([0, 0, neck_zc - neck_cs]) cylinder(d1 = neck_od, d2 = neck_fl_d, h = neck_cs + eps);
            translate([0, 0, neck_zc]) cylinder(d = neck_fl_d, h = neck_gap);
            // top flange with a 45 deg underside so it prints without support
            intersection() {
                union() {
                    translate([0, 0, neck_len - neck_fl_t - (neck_fl_d - neck_od + 0.4)/2])
                        cylinder(d1 = neck_od - 0.4, d2 = neck_fl_d, h = (neck_fl_d - neck_od + 0.4)/2 + eps);
                    translate([0, 0, neck_len - neck_fl_t]) cylinder(d = neck_fl_d, h = neck_fl_t);
                }
                translate([-20, -20, 0]) cube([40, 20 + neck_flat, neck_len + 1]);
            }
        }
        translate([0, 0, -1]) cylinder(d = neck_id, h = neck_len + 2);
        // small lead-in on the spigot
        translate([0, 0, -eps]) difference() {
            cylinder(d = neck_od + 4, h = 1);
            cylinder(d1 = neck_od - 1, d2 = neck_od + 1, h = 1);
        }
    }
}

// =====================================================================
//  BODY  (z = 0 at the bottom rim, built upright; printed upside down)
// =====================================================================
module body_shape(w, d, r, z0, z1, ctop = 0) {
    hull() {
        translate([0, 0, z0]) linear_extrude(z1 - z0 - ctop) body_2d(w, d, r);
        if (ctop > 0) translate([0, 0, z1 - eps]) linear_extrude(eps) body_2d(w - 2*ctop, d - 2*ctop, r - ctop);
    }
}
module body_2d(w, d, r) { translate([0, body_y0 + body_d/2]) rrect(w, d, r); }
module cdisc(r, h = 3.0, c = 1.4) {   // disc with 45 deg chamfers top & bottom
    cylinder(h = c, r1 = r - c, r2 = r);
    translate([0, 0, c - eps]) cylinder(h = h - 2*c + 2*eps, r = r);
    translate([0, 0, h - c]) cylinder(h = c, r1 = r, r2 = r - c);
}
module arm() {   // shoulder at z = 0, hangs down and a little outwards (+x)
    scale([1, 1.3, 1]) hull() {
        translate([-1.0, 0, -3.0])  cdisc(4.4);
        translate([ 3.0, 0, -19.0]) cdisc(5.0);
    }
}
module heart_2d(s) {   // point down, centred, ~s wide
    k = s / 10;
    scale(k) {
        hull() { translate([-2.6, 1.2]) circle(d = 5.2); translate([0, -5.2]) circle(d = 0.6); }
        hull() { translate([ 2.6, 1.2]) circle(d = 5.2); translate([0, -5.2]) circle(d = 0.6); }
    }
}
module body() {
    in_w = body_w - 2*wall; in_d = body_d - 2*wall;
    yf = body_y0 + wall;            // inner front
    yb = body_y0 + body_d - wall;   // inner back
    ztop_in = body_h - body_top_t;
    spk_x = spk[0]/2 + clr;
    difference() {
        union() {
            difference() {
                body_shape(body_w, body_d, body_r, 0, body_h, 2.0);
                body_shape(in_w, in_d, body_r - wall, -1, ztop_in, 0);
            }
            // arms
            // v3: clip the arms to the outside - the shoulder discs reached 2.2 mm into the
            // cavity (harmless with a 40 mm pack, but they touch a 46 mm one)
            difference() {
                for (sx = [-1, 1]) translate([sx*(body_w/2 + 1.0), neck_y, body_h - 5]) mirror([sx < 0 ? 1 : 0, 0, 0]) arm();
                body_shape(in_w, in_d, body_r - wall, -1, ztop_in, 0);
            }
            // speaker rails (L-profile), speaker slides in from the open bottom
            for (sx = [-1, 1]) {
                translate([sx > 0 ? spk_x : -spk_x - 1.6, yf - eps, spk_z0])
                    cube([1.6, spk[1] + clr + 1.4, ztop_in - spk_z0 + eps]);
                translate([sx > 0 ? spk_x - 2.0 : -spk_x, yf + spk[1] + clr, spk_z0])
                    cube([2.0 + eps, 1.4, ztop_in - spk_z0 + eps]);
                // top stops
                translate([sx > 0 ? 11 : -15, yf - eps, spk_z0 + spk[2] + clr])
                    cube([4, spk[1] + clr, ztop_in - spk_z0 - spk[2] - clr + eps]);
            }
            // screw columns for the base
            for (s = body_screws) translate([s[0], s[1], 0]) cylinder(d = 5.6, h = ztop_in + eps);
        }
        // speaker grille: vertical slots inside a rounded window
        gz = spk_z0 + spk[2]/2;
        intersection() {
            translate([0, body_y0 - 1, gz]) rotate([-90, 0, 0]) linear_extrude(wall + 2)
                rrect(26, 16, 6);
            for (i = [-4:4]) translate([i*3.0 - 0.8, body_y0 - 1, gz - 10]) cube([1.6, wall + 2, 20]);
        }
        // little heart above the grille
        translate([0, body_y0 + 0.8, gz + 14.2]) rotate([90, 0, 0])
            linear_extrude(1.0) heart_2d(9);
        // neck hole (D, glue joint) + v3: 45 deg countersink for the neck collar
        translate([0, neck_y, ztop_in - 1]) linear_extrude(body_top_t + 2) neck_2d(neck_od + 2*clr, true);
        translate([0, neck_y, body_h - neck_cs])
            cylinder(d1 = neck_od + 2*clr, d2 = neck_fl_d + 2*clr, h = neck_cs + eps);
        // base screw pilots
        for (s = body_screws) translate([s[0], s[1], -1]) cylinder(d = screw_pilot, h = 10);
        // v3: windows in the back wall for the base hooks (replace the side screws)
        for (sx = [-1, 1]) translate([sx*hook_x - hook_w/2 - 0.3, yb - 1, hook_h - hook_lip_h - 0.3])
            cube([hook_w + 0.6, wall + 2, hook_lip_h + 0.6]);
    }
}

// =====================================================================
//  BASE plate with toes (z = 0 floor, printed as is)
// =====================================================================
module base_2d() {
    body_2d(body_w, body_d, body_r);
    // v3: each foot now runs from the toe to a heel base_heel behind the body.
    // A tap on the lower screen pushes Robi backwards; the tipping edge moves from
    // 13.5 mm to ~27 mm behind the centre of mass (about 2x the force to tip it).
    for (sx = [-1, 1]) hull() {
        translate([sx*13, body_y0 + 3]) scale([1, 1.25]) circle(d = 18);
        translate([sx*13, body_y0 + body_d + base_heel - 9]) circle(d = 18);
    }
}
module base() {
    in_w = body_w - 2*wall; in_d = body_d - 2*wall;
    difference() {
        union() {
            hull() {
                linear_extrude(eps) offset(-1.2) base_2d();
                translate([0, 0, 1.2]) linear_extrude(base_t - 1.2) base_2d();
            }
            // toe caps
            intersection() {
                for (sx = [-1, 1]) translate([sx*13, body_y0 - 3.0, base_t - eps])
                    scale([1, 1.1, 1]) cylinder(h = 3.0, d1 = 15, d2 = 10);
                translate([-50, body_y0 - 30, 0]) cube([100, 30 - 0.4, 20]);
            }
            // v3: rear hooks (replace the side-screw tabs). Fit: back edge first, base
            // tilted ~15 deg front-down, swing up until the lips click into the
            // windows, then the two front screws.
            for (sx = [-1, 1]) translate([sx*hook_x - hook_w/2, body_y0 + body_d - wall - clr - hook_t, base_t - eps]) {
                cube([hook_w, hook_t, hook_h + eps]);
                // lip: flat underside (it carries the load), 45 deg top as lead-in
                translate([0, hook_t - eps, hook_h - hook_lip_h]) hull() {
                    cube([hook_w, hook_lip, hook_lip_h - hook_lip]);
                    cube([hook_w, eps, hook_lip_h]);
                }
            }
            // v3: battery tray - low curb in front of the pack (the hooks are behind it)
            translate([-20, bat_y0 - 1.2, base_t - eps]) cube([40, 1.2, bat_curb]);
            // v3: pillars under the speaker (v2: nothing below it, it dropped ~4 mm)
            for (sx = [-1, 1]) translate([sx*9 - 1.5, body_y0 + wall + spk[1]/2 - 1.5, base_t - eps])
                cube([3, 3, spk_z0 - clr]);
            // locating lip that goes into the body
            translate([0, 0, base_t - eps]) difference() {
                linear_extrude(base_lip) body_2d(in_w - 2*clr, in_d - 2*clr, body_r - wall - clr);
                translate([0, 0, -1]) linear_extrude(base_lip + 2)
                    body_2d(in_w - 2*clr - 2.4, in_d - 2*clr - 2.4, body_r - wall - clr - 1.2);
                // v3: keep the ring's rounded back corners out of the battery footprint
                translate([-bat[0]/2 - clr, bat_y0, -1]) cube([bat[0] + 2*clr, bat[1] + 0.5, base_lip + 2]);
            }
        }
        for (s = body_screws) translate([s[0], s[1], 0]) {
            translate([0, 0, -1]) cylinder(d = screw_clear, h = 20);
            translate([0, 0, -1]) cylinder(d = screw_head, h = 2.5);
            translate([0, 0, base_t - eps]) cylinder(d = 6.2, h = base_lip + 1); // column clearance
        }
    }
}

// =====================================================================
//  small parts
// =====================================================================
module antenna() {          // stands on its peg end, z = 0
    cylinder(d = ant_peg_d, h = 5);                          // peg
    translate([0, 0, 3.25]) cylinder(d1 = ant_peg_d, d2 = 7.5, h = 1.75 + eps);   // collar (45 deg underside)
    translate([0, 0, 5]) cylinder(d1 = 7.5, d2 = 6.5, h = 1.5);
    translate([0, 0, 6.5]) cylinder(d = 3.4, h = 9);         // stalk
    // ball, with 45 deg underside (prints without support)
    translate([0, 0, 15.15]) cylinder(d1 = 3.4, d2 = 9.3, h = 2.95);
    translate([0, 0, 20]) sphere(d = 10);
}
module ear(left) {          // lies flat, z = 0 is the side touching the head
    difference() {
        union() {
            cylinder(d = ear_d, h = ear_t - 0.8);
            translate([0, 0, ear_t - 0.8]) cylinder(d1 = ear_d, d2 = ear_d - 1.6, h = 0.8);
            if (left) for (sy = [-1, 1]) translate([0, sy*ear_peg, -2.0]) cylinder(d = 2.0, h = 2.0 + eps);
            else for (p = ear_r_pegs) translate([-p[1], p[0] - y_usb, -2.0]) cylinder(d = 2.0, h = 2.0 + eps);
        }
        // right ear: window for the microSD card (local x = -world z, local y = world y - y_usb)
        if (!left) translate([-sd_z[1], sd_y[0] - y_usb, -3]) cube([sd_z[1] - sd_z[0], sd_y[1] - sd_y[0], ear_t + 6]);
        if (left) translate([0, 0, -1]) linear_extrude(ear_t + 2) rrect(usb_open[0] + 0.4, usb_open[1] + 0.4, 3.4);

    }
}
// v3: re-derived in radial coordinates. In v2 the retaining cone was already 4.2 mm
// wide where the 3.7 mm channel starts (r = pr - 2.2), so at rest the plunger sat
// 0.25 mm further in than drawn: ~0.05 mm of press travel left / switch held down.
pwr_r     = rr_ray(head_w, head_h, head_r, pwr_ang);   // outer wall along the PWR axis
pwr_rface = pwr_r + 1.5 + pwr_travel;                  // cap face (assembled, at rest)
module pwr_button(adj = 0) {   // stands on its cap, z = 0. adj > 0 = longer plunger
    len_ = pwr_rface - (pwr_tip_r - adj);                       // cap face -> switch
    zn   = pwr_rface - (pwr_r - 2.2 - pwr_play) - 0.2;           // cone narrow end (d 3.3)
    cylinder(d1 = 5.8, d2 = 7.0, h = 0.6);
    translate([0, 0, 0.6]) cylinder(d = 7.0, h = 0.9);
    translate([0, 0, 1.5 - eps]) cylinder(d = 3.3, h = len_ - 1.5);
    translate([0, 0, zn]) cylinder(d1 = 3.3, d2 = 5.4, h = 1.05 + eps);   // 45 deg, prints upright
    translate([0, 0, zn + 1.05]) cylinder(d = 5.4, h = 1.0);              // retaining flange
}
module place_pwr() {
    translate([0, y_btn, 0]) rotate([0, -pwr_ang, 0])
        translate([pwr_rface, 0, 0]) rotate([0, -90, 0]) children();
}
module fit_test() {         // face lip + glass pocket only
    intersection() {
        difference() {
            rprism_y(60, 60, 14, 0, face_t + 1.0, ch, 0);
            head_cuts();
            cyl_y(pocket_d, face_t - eps, 10);
        }
        cyl_y(58, -1, 10);
    }
}

// =====================================================================
//  mock-ups for checking (not printed)
// =====================================================================
module mock_board() {
    color("black") cyl_y(glass_d, gf, gf + 0.6);
    color("dimgray") cyl_y(pcb_d, gf + 0.6, gf + st_pcb_back);
    color("gold") for (p = inserts) translate([p[0], 0, p[1]]) cyl_y(3.5, y_pcb, y_ins);
    color("silver") translate([-usb_r, y_usb - 1.6, -4.5]) cube([7, 3.2, 9]);
    color("white") for (c = conns) translate([c[0] - 2.5, y_pcb, c[1] - 2]) cube([5, st_conn_top - st_pcb_back, 4]);
    color("dimgray") translate([header_box[0][0] + 0.2, y_pcb, header_box[0][1] + 0.2])
        cube([header_box[1][0] - header_box[0][0] - 0.4, st_header_top - st_pcb_back, 2.5]);
}
module mock_speaker() { color("darkslategray")
    translate([-spk[0]/2, body_y0 + wall, body_z0 + spk_z0]) cube(spk); }
// v3: largest pack envelope that fits (bat = 46 x 11.5 x 31 incl. protection PCB),
// standing on the base floor between the curb and the hooks
module mock_battery() { color("orange")
    translate([-bat[0]/2, bat_y0 + 0.2, base_t + 0.1]) cube(bat); }
// v3: rough cable bundle check - two MX1.25 plugs side by side going up the neck bore
module mock_plugs() { color("red")
    translate([-4.6, neck_y - 1.7, neck_z0 - 2]) cube([9.2, 3.4, neck_len + 6]); }

// =====================================================================
//  placement
// =====================================================================
module place_head() { translate([0, 0, head_cz]) children(); }

module assembly(explode = 0) {
    place_head() {
        color("white") translate([0, -explode, 0]) head_front();
        color("whitesmoke") translate([0, explode, 0]) head_back();
        color("deepskyblue") translate([-head_w/2 - explode, y_usb, 0]) rotate([0, -90, 0]) ear(true);
        color("deepskyblue") translate([head_w/2 + explode, y_usb, 0]) rotate([0, 90, 0]) ear(false);
        color("tomato") translate([0, ant_y, head_h/2 - 5 + explode]) antenna();
        color("tomato") place_pwr() pwr_button();
        mock_board();
    }
    color("deepskyblue") translate([0, neck_y, neck_z0]) neck();
    color("white") translate([0, 0, body_z0 + explode/2]) body();
    color("deepskyblue") translate([0, 0, -explode/2]) base();
    mock_speaker(); mock_battery();
}

// =====================================================================
//  print orientations
// =====================================================================
module print_head_front() { rotate([90, 0, 0]) head_front(); }                    // face on the bed
module print_head_back()  { translate([0, 0, head_d]) rotate([-90, 0, 0]) head_back(); } // back on the bed
module print_body()       { translate([0, 0, body_h]) rotate([180, 0, 0]) body(); } // shoulders on the bed
module print_fit_test()   { rotate([90, 0, 0]) fit_test(); }

if (part == "assembly")   assembly(0);
// assembled positions, used by the clash checker
if (part == "A_head_front") place_head() head_front();
if (part == "A_head_back")  place_head() head_back();
if (part == "A_body")       translate([0, 0, body_z0]) body();
if (part == "A_base")       base();
if (part == "A_neck")       translate([0, neck_y, neck_z0]) neck();
if (part == "A_antenna")    place_head() translate([0, ant_y, head_h/2 - 5]) antenna();
if (part == "A_pwr")        place_head() place_pwr() pwr_button();
if (part == "A_pwr_long")   place_head() place_pwr() pwr_button(pwr_adj[2]);
if (part == "M_plugs")      mock_plugs();
if (part == "A_ear_l")      place_head() translate([-head_w/2, y_usb, 0]) rotate([0, -90, 0]) ear(true);
if (part == "A_ear_r")      place_head() translate([head_w/2, y_usb, 0]) rotate([0, 90, 0]) ear(false);
if (part == "M_board")      place_head() mock_board();
if (part == "M_speaker")    mock_speaker();
if (part == "M_battery")    mock_battery();
if (part == "M_sdcard")     place_head() translate([23.2, gf + 4.4, -5.0]) cube([30, 0.8, 11.0]);  // microSD sliding in
if (part == "M_usbplug")    place_head() translate([-usb_r - 30, y_usb - 3.25, -6.0]) cube([30 - 0.3, 6.5, 12.0]);  // typical plug over-mould 12 x 6.5
// if (part == "M_cables")     place_head() for (c = conns) translate([c[0] - 1.5, y_pcb + 4, c[1] - 1.5]) cube([3, 6, 3]); // plug + cable bend room
if (part == "exploded")   assembly(18);
if (part == "head_front") print_head_front();
if (part == "head_back")  print_head_back();
if (part == "body")       print_body();
if (part == "base")       base();
if (part == "neck")       neck();
if (part == "antenna")    antenna();
if (part == "ear_left")   translate([0, 0, ear_t]) rotate([180, 0, 0]) ear(true);
if (part == "ear_right")  translate([0, 0, ear_t]) rotate([180, 0, 0]) ear(false);
if (part == "pwr_button") pwr_button();                       // standard length
if (part == "pwr_button_short") pwr_button(pwr_adj[0]);       // v3: -0.4 mm (if PWR stays pressed)
if (part == "pwr_button_long")  pwr_button(pwr_adj[2]);       // v3: +0.4 mm (if pressing does nothing)
if (part == "fit_test")   print_fit_test();

if (part == "section") difference() { assembly(0); translate([0, -100, -10]) cube([200, 300, 300]); }
if (part == "section_y") difference() { assembly(0); translate([-150, y_btn, -10]) cube([300, 300, 300]); }

// preview only: a face drawn on the screen (what Xiaozhi shows)
module mock_face() {
    color("white") for (sx = [-1, 1]) translate([sx*9, gf - 0.05, 4]) rotate([90, 0, 0])
        linear_extrude(0.1) hull() { translate([0, 3.5]) circle(d = 7); translate([0, -3.5]) circle(d = 7); }
    color("white") translate([0, gf - 0.05, -7]) rotate([90, 0, 0]) linear_extrude(0.1)
        difference() { circle(d = 12); circle(d = 9); translate([-7, 0]) square([14, 7]); }
    color("hotpink") for (sx = [-1, 1]) translate([sx*18, 0.55, -25]) rotate([90, 0, 0]) cylinder(d = 5.6, h = 0.1);
}
if (part == "hero") { assembly(0); place_head() mock_face(); }
if (part == "hero_exploded") { assembly(20); place_head() translate([0, -20, 0]) mock_face(); }

// preview: size comparison with a 330 ml can (66 x 115 mm)
if (part == "size") {
    assembly(0); place_head() mock_face();
    color("silver") translate([90, 12, 0]) cylinder(d = 66, h = 115);
    color("crimson") translate([90, 12, 15]) cylinder(d = 66.4, h = 80);
}
if (part == "right_side") { assembly(0); }
// v3 review renders
if (part == "sec_side") { difference() { assembly(0); translate([0, -100, -10]) cube([200, 300, 300]); } mock_plugs(); }
if (part == "sec_head") difference() { assembly(0); translate([-150, y_pcb + 1.5, -10]) cube([300, 300, 300]); }
if (part == "sec_body") difference() { assembly(0); translate([-150, -150, base_t + 22]) cube([300, 300, 300]); }
if (part == "sec_bezel") difference() {   // zoom on the lower window edge, cut at x = 0
    place_head() { head_front(); head_back(); mock_board(); }
    translate([0, -100, -10]) cube([200, 300, 300]);
}
