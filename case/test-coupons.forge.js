// test-coupons.forge.js
// Small, quick-to-print FIT-TEST coupons for the clawdmeter enclosure. Each coupon
// reproduces ONE mating feature at true dimensions (pulled from the shared module
// enclosure-features.forge.js, so they always match the real enclosure). Print one,
// drop in the real component, confirm the fit, THEN commit to the full enclosure.
//
// Pick a coupon with the "Coupon" dropdown. "Show Component" overlays the real part
// (translucent) for a visual check — turn it OFF before exporting an STL to print.
//
// Frame per coupon: the printable panel/tray outer face is at Z=0; the component
// installs from -Z (behind), exactly as it seats in the enclosure.

const feat = require("./enclosure-features.forge.js");
const oledModule = require("./oled-ssd1306-091.forge.js");
const ec11Module = require("./ec11-encoder.forge.js");
const espModule = require("./esp32-wroom32-devkit.forge.js");

const WALL = feat.WALL;      // 2.5
const BASE_THK = 2.5;
const COL_PART = "#d6dbe0";
const COL_TRAY = "#3b414a";

// ---------------------------------------------------------------------------
// OLED retention — TWO separate printed parts, each its own Coupon choice so they
// export individually:
//   "OLED holder"       = panel with the sloped bezel + fixed lip (upper edge) + 2
//                         screw posts (lower edge). Tuck the OLED's upper edge under
//                         the lip, drop the lower edge.
//   "OLED retainer bar" = the bar that screws to the posts (2x M2 self-tap), fingers
//                         capture the lower edge, middle relieved for the diode row.
// ---------------------------------------------------------------------------
function couponOledHolder(show) {
  const panel = box(46, 22, WALL).translate(0, 0, -WALL);   // outer face at z=0
  const holder = union(
    difference(panel, feat.oledBezelCut(WALL)),
    feat.oledFixedLip(WALL),
    feat.oledRetainerPosts(WALL),
  ).color(COL_PART).material({ roughness: 0.55 });

  const children = [{ name: "OLED Holder Coupon", shape: holder }];
  if (show) {
    // glass top (display origin z 2.6) flush to the panel inner face (z = -WALL)
    const oled = oledModule.buildOled({ showHeader: true })
      .translate(-2.31, 0, -WALL - 2.6);
    children.push({ name: "OLED (ref)", group: oled });
  }
  return group(...children);
}

function couponOledBar() {
  const bar = feat.oledRetainerBar(WALL).color(COL_TRAY);
  return group({ name: "OLED Retainer Bar Coupon", shape: bar });
}

// ---------------------------------------------------------------------------
// Encoder hole: a panel with the Ø8 bushing hole (test the bushing + panel nut).
// ---------------------------------------------------------------------------
function couponEncoder(show) {
  const panel = box(28, 28, WALL).translate(0, 0, -WALL);
  const part = difference(panel, feat.encoderHoleCut())
    .color(COL_PART).material({ roughness: 0.55 });

  const children = [{ name: "Encoder Coupon", shape: part }];
  if (show) {
    // panel_mount (body top, local z 7.5) against the inner face (z = -WALL)
    const enc = ec11Module.buildEc11({ shaftLen: 15 }).translate(0, 0, -WALL - 7.5);
    children.push({ name: "Encoder (ref)", group: enc });
    const nut = cylinder(2.2, 5.77, 5.77, 6).color("#4a4d52");  // on the bushing, outer face
    children.push({ name: "Nut (ref)", shape: nut });
  }
  return group(...children);
}

// ---------------------------------------------------------------------------
// ESP32 retention: the full mounting tray — 2 left pegs + 2 snap clips, and a
// right wall with the slide-under ledge + USB slot. Tests the whole slide-and-snap.
// Board-relative frame: board centered on XY, USB end at +X (matches the enclosure).
// ---------------------------------------------------------------------------
function couponEsp32(show) {
  const espRaw = espModule.buildEsp32({ showHeaders: true, mountHoles: true }).rotate([0, 1, 0], 180);
  const eb = espRaw.boundingBox();
  const espTz = 1.5 - eb.min[2];        // same seating height the enclosure computes
  const pcbUnderZ = espTz - espModule.PCB_THK;  // board underside / standoff height (1.5mm board)
  const boardTopZ = espTz;              // board top (pin side)
  const rightEdge = eb.max[0] - 1;      // PCB right edge (USB protrudes 1mm past it)
  const leftEdge = eb.min[0];           // PCB left edge

  const holeX = 25.6, holeY = 11.6;     // board mounting holes, board-relative
  let tray = box(62, 32, BASE_THK).translate(0, 0, -BASE_THK);   // base plate

  // 4 support standoffs; locating pegs on the LEFT holes only (right slides under the ledge)
  for (const sx of [-holeX, holeX]) {
    for (const sy of [-holeY, holeY]) {
      tray = union(tray, feat.standoff(pcbUnderZ, sx < 0).translate(sx, sy, 0));
    }
  }
  // 2 gentle snap clips over the LEFT edge
  for (const cy of [-5, 5]) tray = union(tray, feat.snapClip(boardTopZ).translate(leftEdge, cy, 0));

  // right wall + slide-under ledge + USB slot
  const wallInnerX = rightEdge + 1;
  tray = union(tray, box(2, 30, boardTopZ + 4).translate(wallInnerX + 1, 0, 0));
  tray = union(tray, feat.wallLedge(boardTopZ).translate(wallInnerX, 1, 0));
  tray = difference(tray, feat.usbSlotCut().translate(wallInnerX + 1, 0, 0));
  tray = tray.color(COL_TRAY);

  const children = [{ name: "ESP32 Tray Coupon", shape: tray }];
  if (show) children.push({ name: "ESP32 (ref)", group: espRaw.translate(0, 0, espTz) });
  return group(...children);
}

// ---------------------------------------------------------------------------
// Heat-set boss — TWO separate printed parts, each its own Coupon choice so they
// export individually (print both, then screw them together to test the assembly):
//   "Boss body"       = a body-side boss with the Ø4.1 heat-set insert bore.
//   "Boss base plate" = a base-plate square with the counterbored M3 clearance hole
//                       (the screw goes up through this into the insert). Coaxial —
//                       both centered on (0,0), so they stack for the test.
// ---------------------------------------------------------------------------
function couponBossBody() {
  const boss = difference(feat.bossPost(13), feat.insertHole())
    .color(COL_PART).material({ roughness: 0.55 });
  return group({ name: "Boss Body Coupon", shape: boss });
}

function couponBossBase() {
  const basePad = difference(
    box(16, 16, BASE_THK).translate(0, 0, -BASE_THK),
    feat.baseScrewHole(BASE_THK),
  ).color(COL_TRAY);
  return group({ name: "Boss Base Plate Coupon", shape: basePad });
}

if (require.main === module) {
  const which = Param.choice("Coupon", "OLED holder",
    ["OLED holder", "OLED retainer bar", "Encoder hole", "ESP32 retention",
     "Boss body", "Boss base plate"]);
  const show = Param.bool("Show Component", false);

  if (which === "OLED retainer bar") return couponOledBar();
  if (which === "Encoder hole") return couponEncoder(show);
  if (which === "ESP32 retention") return couponEsp32(show);
  if (which === "Boss body") return couponBossBody();
  if (which === "Boss base plate") return couponBossBase();
  return couponOledHolder(show);
}

return {
  couponOledHolder, couponOledBar, couponEncoder, couponEsp32,
  couponBossBody, couponBossBase,
};
