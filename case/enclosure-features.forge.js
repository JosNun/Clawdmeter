// enclosure-features.forge.js
// Single source of truth for the clawdmeter enclosure's MATING FEATURES.
// Both the full enclosure (enclosure.forge.js) and the fit-test coupons
// (test-coupons.forge.js) build from these, so a coupon prints the exact same
// geometry as the real part — tune a fit here and both update.
//
// Each feature is built in a simple LOCAL frame (documented per function) so the
// enclosure can place it (onFacet / translate) and a flat coupon can embed it.

const WALL = 2.5;               // nominal wall thickness the panel features assume
const OLED_BACK_OFFSET = 2.6;   // OLED glass-top (flush to inner wall) -> PCB back face
const BOSS_R = 3.6;             // heat-set boss outer radius

// --- OLED viewing bezel: one sloped funnel cutter (no step).
//     Local: +Z = outward, z=0 at the OUTER wall face. Narrow at the glass, flares out. ---
function oledBezelCut(wall = WALL) {
  return roundedRect(25, 9, 1.5)
    .extrude(wall + 1.0, { scaleTop: [1.2, 1.5] })
    .translate(0, 0, -(wall + 0.5));
}

// --- OLED retention: TUCK + RETAINER BAR (installs with NO lateral sliding, since the
//     assembled case has no room for it). Tuck the +Y (upper) edge under the fixed lip,
//     drop the -Y (lower) edge against the wall, then screw the retainer bar over it.
//     Local frame matches oledBezelCut (z=0 = outer face). All contact points sit at
//     X = ±13, clear of the diode row across the PCB back center. Uses M2 self-tap screws.
const OLED_CAP = (wall) => -wall - OLED_BACK_OFFSET - 0.4;   // capture plane, 0.4mm behind PCB back

// Fixed lip on the +Y edge — two rigid tabs the upper OLED edge tucks under (no flex needed).
function oledFixedLip(wall = WALL) {
  const capZ = OLED_CAP(wall), botZ = -6.9;
  const tab = (cx) => union(
    box(6, 1.6, -2.0 - botZ).translate(cx, 6.8, botZ),     // web outboard of the +Y edge, up into the wall
    box(6, 1.8, capZ - botZ).translate(cx, 5.1, botZ),     // lip: ~1.8mm overhang, top at the capture plane
  );
  return union(tab(-13), tab(13));
}

// Two screw posts on the -Y edge side (outboard of the PCB) for the retainer bar. Ø1.7 M2 pilot.
function oledRetainerPosts(wall = WALL) {
  const capZ = OLED_CAP(wall);
  const post = (cx) => difference(
    cylinder(-2.5 - capZ, 2.2).translate(cx, -8, capZ),    // from the capture plane up to the inner wall
    cylinder(3.5, 0.85).translate(cx, -8, capZ - 0.2),     // M2 self-tap pilot, open to the cavity
  );
  return union(post(-14), post(14));
}

// Separate retainer bar: screws down to the posts; two fingers capture the -Y edge, middle
// relieved so it clears the diode row. Print as its own small part.
function oledRetainerBar(wall = WALL) {
  const capZ = OLED_CAP(wall), thk = 1.6, botZ = capZ - thk;
  let bar = box(32, 3, thk).translate(0, -7.5, botZ);      // spine over the posts (Y -9..-6)
  for (const cx of [-13, 13]) {
    bar = union(bar, box(5, 2.5, thk).translate(cx, -5.25, botZ));   // finger onto the PCB edge (Y -6.5..-4)
  }
  for (const cx of [-14, 14]) {
    bar = difference(bar, cylinder(thk + 1, 1.15).translate(cx, -8, botZ - 0.5));  // Ø2.3 M2 clearance
  }
  return bar;
}

// --- Encoder bushing clearance hole (Ø7.2 for the Ø7 EC11 bushing; ~0.2mm clearance).
//     Local: axis +Z, centered; long enough to pass through any panel. ---
function encoderHoleCut(through = 24) {
  return cylinder(through, 3.6).translate(0, 0, -through / 2);
}

// --- ESP32 standoff post (+ optional Ø1.4 locating peg).
//     Local: base at z=0, axis +Z; post top = board underside.
//     Ø3.0 (was Ø4.4): the mounting holes sit at the same Y as the header rows, so a
//     fat post overlapped the header spacer/pin footprint and fouled the header tops
//     during insertion. Ø3.0 clears the header field (nearest pin ~2.7mm away). ---
function standoff(height, withPeg = false) {
  let s = cylinder(height, 1.5);
  if (withPeg) s = union(s, cylinder(height + 2, 0.7));
  return s;
}

// --- ESP32 snap clip (gentle): hooks a board edge from the base plate.
//     Local: board EDGE at x=0, board extends to +X, board top at z=topZ.
//     Post sits on the -X (outboard) side; lip overhangs ~1mm onto the board top. ---
function snapClip(topZ) {
  return union(
    box(2, 4, topZ + 1.2).translate(-1.2, 0, 0),          // upright post, just outboard of the edge
    box(1.6, 4, 1.0).translate(0.2, 0, topZ + 0.2),       // lip, ~1mm overhang, 0.2 above the board
  );
}

// --- ESP32 retention ledge: a wall overhang the board's opposite edge slides UNDER.
//     Local: wall face at x=0 (+X into wall), overhang toward -X; underside above z=topZ.
//     Underside at topZ+0.5 (was +0.1): with the Ø3.0 standoffs below (top at topZ-1),
//     the slide channel is now 1.5mm for a 1.0mm board — 0.5mm clearance, and print
//     support in the overhang gap actually clears. ---
function wallLedge(topZ) {
  return box(3.5, 18, 2).translate(-1.25, 0, topZ + 0.5);
}

// --- Heat-set boss post (Ø7.2). Local: base at z=0, axis +Z. ---
function bossPost(height) {
  return cylinder(height, BOSS_R);
}

// --- M3 heat-set insert bore (Ø4.1), open at the bottom for install from below.
//     Local: axis +Z, mouth at z=-0.5. Subtract from a bossPost at the same XY. ---
function insertHole() {
  return cylinder(6, 2.05).translate(0, 0, -0.5);
}

// --- Base-plate screw hole: M3 clearance (Ø3.4) + Ø6 socket-head counterbore from below.
//     Local: axis +Z, plate TOP at z=0, plate body below (z<0). Subtract from the base plate. ---
function baseScrewHole(baseThk) {
  return union(
    cylinder(baseThk + 1, 1.7).translate(0, 0, -baseThk - 0.5),
    cylinder(2.2, 3.0).translate(0, 0, -baseThk - 0.5),
  );
}

// --- USB micro-B slot: opened down to the wall base. Local: centered in wall (x=0),
//     centered on the connector Y (y=0), z open below z=0 up to the top of the slot.
//     Height 9 (was 7.5): the printed hole was ~1mm too short for the cable plug to
//     seat — top now at z=8 (was 6.5), ~3.4mm clear above the connector. ---
function usbSlotCut() {
  return box(6, 13, 9).translate(0, 0, -1);
}

if (require.main === module) {
  // Quick visual of the local-frame features, laid out in a row.
  const wall = WALL;
  const panel = box(40, 20, wall).translate(0, 0, -wall);
  const oledDemo = union(
    difference(panel, oledBezelCut(wall)),
    oledFixedLip(wall),
    oledRetainerPosts(wall),
    oledRetainerBar(wall),
  ).color("#d6dbe0");
  const encDemo = difference(box(24, 24, wall).translate(0, 0, -wall), encoderHoleCut())
    .translate(55, 0, 0).color("#d6dbe0");
  const clipDemo = union(standoff(4.6, true), snapClip(5.6).translate(-6, 0, 0))
    .translate(-45, 0, 0).color("#3b414a");
  const bossDemo = difference(bossPost(13), insertHole())
    .translate(-75, 0, 0).color("#d6dbe0");
  return group(
    { name: "OLED Panel Demo", shape: oledDemo },
    { name: "Encoder Panel Demo", shape: encDemo },
    { name: "Clip Demo", shape: clipDemo },
    { name: "Boss Demo", shape: bossDemo },
  );
}

return {
  WALL, OLED_BACK_OFFSET, BOSS_R,
  oledBezelCut, oledFixedLip, oledRetainerPosts, oledRetainerBar, encoderHoleCut,
  standoff, snapClip, wallLedge,
  bossPost, insertHole, baseScrewHole, usbSlotCut,
};
