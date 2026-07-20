// enclosure.forge.js
// clawdmeter enclosure — sloped-front desktop console (rev B).
// Continuous rounded body, tall vertical back (micro-USB), 60deg display facet at the
// front-top carrying the 0.91" OLED (right) and EC11 knob (left), removable base plate.
// Built around the real component parts.
//
// Frame: X = width (left-right, 80), Y = depth (front at -Y, back at +Y, 60),
//        Z = height (up). Components installed with headers off (wired in place).

// IMPORTANT: build this model on the OCCT backend. The rounded top/facet edges use
// 3D fillets on a boolean-cut body, which only render correctly on OCCT — Manifold
// (the default) artifacts edge finishes. In the browser editor pick OCCT from the
// backend selector; on the CLI pass `--backend occt`.

const espModule = require("./esp32-wroom32-devkit.forge.js");
const oledModule = require("./oled-ssd1306-091.forge.js");
const ec11Module = require("./ec11-encoder.forge.js");
const feat = require("./enclosure-features.forge.js");  // shared mating features (also used by the coupons)

const COL_BODY = "#d6dbe0";  // shell
const COL_BASE = "#3b414a";  // removable base plate
const COL_KNOB = "#1d1f23";  // knob cap

function rotXpt(p, deg) {
  const r = (deg * Math.PI) / 180, c = Math.cos(r), s = Math.sin(r);
  return [p[0], p[1] * c - p[2] * s, p[1] * s + p[2] * c];
}
const sub3 = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];

function buildEnclosure(props = {}) {
  const W = props.width ?? 80;        // X
  const D = props.depth ?? 60;        // Y
  const wall = props.wall ?? 2.5;
  const cornerR = props.cornerR ?? 10;
  const frontLip = props.frontLip ?? 10;   // vertical front lip height
  const tilt = props.tilt ?? 60;           // display facet, degrees from horizontal
  const faceLen = props.faceLen ?? 30;     // display facet length up-slope
  const baseThk = props.baseThk ?? 2.5;
  const edgeR = props.edgeRadius ?? 3;
  const ghost = props.ghost ?? false;

  const tRad = (tilt * Math.PI) / 180;
  const run = faceLen * Math.cos(tRad);
  const rise = faceLen * Math.sin(tRad);
  const front = -D / 2, back = D / 2;
  const H = frontLip + rise;                // back/top height
  const intX = W / 2 - wall, intY = D / 2 - wall;  // interior wall faces

  const nIn = [0, Math.sin(tRad), -Math.cos(tRad)];   // inward facet normal
  const nOut = [0, -Math.sin(tRad), Math.cos(tRad)];  // outward facet normal
  const facetOffset = front * nIn[1] + frontLip * nIn[2];
  const facetCenter = [0, front + run / 2, frontLip + rise / 2];
  // place a facet-local shape (its +Z = outward normal) onto the facet at ctr
  const onFacet = (shp, ctr) => shp.rotate([1, 0, 0], tilt).translate(ctr[0], ctr[1], ctr[2]);

  // --- Outer shell: solid, facet cut, rounded top/facet edges, then hollow ---
  let outer = roundedRect(W, D, cornerR).extrude(H).trimByPlane(nIn, facetOffset);
  const roundEdges = coalesceEdges(
    selectEdges(outer, { convex: true, within: { zMin: frontLip + 0.5 } }),
  );
  if (roundEdges.length) outer = fillet(outer, edgeR, roundEdges);
  const cavity = roundedRect(W - 2 * wall, D - 2 * wall, Math.max(1, cornerR - wall))
    .extrude(H - wall + 1).translate(0, 0, -1).trimByPlane(nIn, facetOffset + wall);
  let body = difference(outer, cavity);

  // --- Face layout: knob (left) + OLED (right), evenly spaced ---
  const evenGap = (W - 15 - 38) / 3;
  const knobX = -W / 2 + evenGap + 7.5;
  const oledX = W / 2 - evenGap - 19;
  const oledFace = [oledX, facetCenter[1], facetCenter[2]];  // outer facet point
  const knobFace = [knobX, facetCenter[1], facetCenter[2]];

  const knobHole = onFacet(feat.encoderHoleCut(24), knobFace);
  const oledBezel = onFacet(feat.oledBezelCut(wall), oledFace);  // sloped viewing funnel

  // --- ESP32: flipped (pins up), USB out the RIGHT wall, sat against the right wall ---
  const espRaw = espModule.buildEsp32({ showHeaders: true, mountHoles: true })
    .rotate([0, 1, 0], 180);   // length along X, USB to +X (right), pins up, USB low
  const eb = espRaw.boundingBox();
  const espTx = intX - eb.max[0];         // USB face flush to the inner RIGHT wall (aligns cutout)
  const espTy = 6.0;                       // shifted FORWARD off the back wall (clears back bosses)
  const espTz = 1.5 - eb.min[2];          // lowest component 1.5mm above floor
  const esp = espRaw.translate(espTx, espTy, espTz);
  const pcbUnderZ = espTz - espModule.PCB_THK;  // PCB underside = standoff top (1.5mm board, flipped)
  const usbTopZ = espTz;                  // PCB top / pin side

  // USB hole in the RIGHT wall, opened to the wall base, tall enough to center the port
  const usbHole = feat.usbSlotCut().translate(intX + wall / 2, espTy, 0);

  // OLED retention: fixed lip (upper edge) + screw posts (lower edge) are part of the body;
  // the retainer bar is a separate part (see below). Board tucks in, no sliding needed.
  const oledHold = onFacet(union(feat.oledFixedLip(wall), feat.oledRetainerPosts(wall)), oledFace);

  body = difference(body, knobHole, oledBezel, usbHole);
  body = union(body, oledHold);

  // --- Corner bosses for M3 heat-set inserts, gusseted into the corners, kept inside ---
  const bossH = 13;
  const bossRad = cornerR - feat.BOSS_R - 1.0;           // radial dist from corner-arc center
  const bd = bossRad / Math.SQRT2;                       // on the corner diagonal
  const bcx = (W / 2 - cornerR) + bd, bcy = (D / 2 - cornerR) + bd;
  const bossPts = [[-bcx, -bcy], [bcx, -bcy], [-bcx, bcy], [bcx, bcy]];
  for (const [bx, by] of bossPts) body = union(body, feat.bossPost(bossH).translate(bx, by, 0));
  for (const [bx, by] of bossPts) body = difference(body, feat.insertHole().translate(bx, by, 0));

  // --- Retention ledge on the RIGHT wall: the board's right edge slides UNDER it (hold-down) ---
  body = union(body, feat.wallLedge(usbTopZ).translate(intX, espTy + 1, 0));

  let bodyShaped = body.color(COL_BODY).material({ roughness: 0.55, metalness: 0.04 });
  if (ghost) bodyShaped = bodyShaped.material({ opacity: 0.22, roughness: 0.4 });

  // --- Base plate: standoffs+pegs, snap clips, counterbored screw holes ---
  let basePlate = roundedRect(W, D, cornerR).extrude(baseThk).translate(0, 0, -baseThk);
  // standoffs with Ø1.4 locating pegs at the board's 4 corner mounting holes
  const holeLocal = [[25.6, 11.6], [25.6, -11.6], [-25.6, 11.6], [-25.6, -11.6]];
  const pegPts = holeLocal.map(([hx, hy]) => [-hx + espTx, hy + espTy]);  // after Y180: x -> -x
  let mounts = null;
  // Locating peg on the LEFT holes only, so the RIGHT edge is free to slide under the wall ledge.
  for (const [px, py] of pegPts) {
    const s = feat.standoff(pcbUnderZ, px < 0).translate(px, py, 0);
    mounts = mounts ? union(mounts, s) : s;
  }
  // 2 gentle snap clips over the LEFT short edge, in the pin-free gap between the header rows
  const boardLeftX = eb.min[0] + espTx;
  for (const cyRel of [-5, 5]) {
    mounts = union(mounts, feat.snapClip(usbTopZ).translate(boardLeftX, espTy + cyRel, 0));
  }
  basePlate = union(basePlate, mounts);
  // M3 clearance + Ø6 socket-head counterbore (screws up from below)
  for (const [bx, by] of bossPts) {
    basePlate = difference(basePlate, feat.baseScrewHole(baseThk).translate(bx, by, 0));
  }
  basePlate = basePlate.color(COL_BASE);

  // --- OLED set back flush to the INNER facet wall; header on ---
  const oledOrigin = [2.31, 0, 2.6];  // buildOled 'display' connector origin
  const oledInner = [oledFace[0] + nIn[0] * wall, oledFace[1] + nIn[1] * wall, oledFace[2] + nIn[2] * wall];
  const oT = sub3(oledInner, rotXpt(oledOrigin, tilt));
  const oled = oledModule.buildOled({ showHeader: true })
    .rotate([1, 0, 0], tilt).translate(oT[0], oT[1], oT[2]);
  const oledRetainer = onFacet(feat.oledRetainerBar(wall), oledFace)
    .color(COL_BASE).material({ roughness: 0.55 });

  // --- Encoder + knob + nut on the facet (body seated BEHIND the inner wall) ---
  // panel_mount (local z 7.5, top of body) goes to the INNER wall face, so the 12x12
  // body sits inside the case and only the Ø7 bushing pokes through the Ø8 hole.
  // Along local z after the tilt: 7.5 = inner face, 10.0 = outer face (wall = 2.5).
  const encOrigin = [0, 0, 7.5];      // buildEc11 'panel_mount' connector origin
  const knobInner = [knobFace[0] + nIn[0] * wall, knobFace[1] + nIn[1] * wall, knobFace[2] + nIn[2] * wall];
  const eT = sub3(knobInner, rotXpt(encOrigin, tilt));
  const enc = ec11Module.buildEc11({ shaftLen: 15 })
    .rotate([1, 0, 0], tilt).translate(eT[0], eT[1], eT[2]);
  const knob = cylinder(17, 7.5, 6.8).translate(0, 0, 12.5)   // bottom ~2.5mm proud, clears the nut
    .rotate([1, 0, 0], tilt).translate(eT[0], eT[1], eT[2])
    .color(COL_KNOB).material({ roughness: 0.5 });
  const encNut = cylinder(2.2, 5.77, 5.77, 6).translate(0, 0, 10.0)  // on the bushing, against the OUTER face
    .rotate([1, 0, 0], tilt).translate(eT[0], eT[1], eT[2])
    .color("#4a4d52").material({ metalness: 0.6, roughness: 0.5 });

  return group(
    { name: "Body", tags: "shell", shape: bodyShaped },
    { name: "Base Plate", tags: "shell", shape: basePlate },
    { name: "ESP32", tags: "internal", group: esp },
    { name: "OLED", tags: "internal", group: oled },
    { name: "OLED Retainer", tags: "internal", shape: oledRetainer },
    { name: "Encoder", tags: "internal", group: enc },
    { name: "Encoder Nut", tags: "internal", shape: encNut },
    { name: "Knob", shape: knob },
  ).withReferences({
    points: { oledFace, knobFace },
  });
}

if (require.main === module) {
  // STL/3MF export (CLI and UI) always outputs the WHOLE returned model — there is
  // no per-object export flag. So pick a single PRINTED part here to export it alone;
  // "Assembly (all)" keeps the full reference view. In the UI set this dropdown, in the
  // CLI pass e.g. --param "Export Part=Base Plate".
  const part = Param.choice("Export Part", "Assembly (all)",
    ["Assembly (all)", "Body", "Base Plate", "OLED Retainer"]);
  const ghost = Param.bool("Ghost Shell", false);
  const enclosure = buildEnclosure({ ghost });

  if (part !== "Assembly (all)") return enclosure.child(part);

  const bb = enclosure.boundingBox();
  verify.inRange("Width ~80mm", bb.max[0] - bb.min[0], 78, 82);
  verify.inRange("Height ~38.5mm (knob protrudes in front)", bb.max[2] - bb.min[2], 36, 42);
  return enclosure;
}

return { buildEnclosure };
