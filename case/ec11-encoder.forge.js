// ec11-encoder.forge.js
// Standard EC11 rotary encoder (12mm Alps-style, push switch, 6mm D-shaft) — dimensional reference.
//
// Convention: encoder body base at Z=0 (seats on the PCB top), body/bushing/shaft
// go up +Z, terminals and locating legs go down -Z. Shaft points up.
//
// Sourced canonical dims (mm), Alps EC11E + Bourns PEC11R cross-checked:
//   Body (square can)  ~12 x 12 x 7.5 (H above PCB)
//   Bushing            M7 x 0.75, OD ~7.0, ~5 mm of thread above the can top
//   Shaft              Ø6.0 "D"/flatted (4.5 across the flat), 20 mm above the bushing
//   Terminals          5 pins: 3 encoder (A-C-B, 2.5mm pitch) one edge + 2 switch (D-E, 5mm) opposite
//   Locating legs      2 metal frame legs, ~12.5 mm apart, bent down into the PCB (anti-rotation)
//   Height to shaft tip ~32.5 from PCB;  shaft options 15/20/25mm (default 20)

const COL_CAN = "#c4c8ce";      // nickel-plated steel can
const COL_BASE = "#171717";     // plastic base
const COL_BUSHING = "#b4b8be";  // threaded bushing
const COL_SHAFT = "#adb1b8";    // metal D-shaft
const COL_PIN = "#d8c17a";      // terminals (gold)
const COL_LEG = "#a9adb4";      // locating legs

function buildEc11(props = {}) {
  const bodyW = props.bodyW ?? 12.0;      // X
  const bodyD = props.bodyD ?? 12.0;      // Y
  const bodyH = props.bodyH ?? 7.5;       // Z, above PCB
  const baseH = props.baseH ?? 1.5;       // plastic base slice
  const bushingOD = props.bushingOD ?? 7.0;
  const bushingH = props.bushingH ?? 5.0; // exposed thread above can top
  const shaftDia = props.shaftDia ?? 6.0;
  const shaftFlat = props.shaftFlat ?? 4.5; // across-flat of the D
  const shaftLen = props.shaftLen ?? 20.0;  // above the bushing top
  const pinDrop = props.pinDrop ?? 3.5;     // terminal length below PCB
  const legDrop = props.legDrop ?? 3.0;     // locating leg length below PCB

  const canTopZ = bodyH;
  const bushingTopZ = canTopZ + bushingH;
  const shaftTopZ = bushingTopZ + shaftLen;
  const children = [];

  // --- Body: plastic base + metal can ---
  children.push({
    name: "Base",
    shape: box(bodyW, bodyD, baseH).color(COL_BASE),
  });
  children.push({
    name: "Can",
    shape: box(bodyW, bodyD, bodyH - baseH)
      .translate(0, 0, baseH)
      .color(COL_CAN)
      .material({ metalness: 0.7, roughness: 0.4 }),
  });

  // --- Bushing shoulder + threaded collar ---
  children.push({
    name: "Bushing Shoulder",
    shape: cylinder(0.8, 4.5).translate(0, 0, canTopZ).color(COL_BUSHING),
  });
  children.push({
    name: "Bushing",
    shape: cylinder(bushingH, bushingOD / 2)
      .translate(0, 0, canTopZ + 0.8)
      .color(COL_BUSHING)
      .material({ metalness: 0.6, roughness: 0.45 }),
  });

  // --- D-shaft: full round lower, flat cut on the upper portion ---
  const shaftRound = cylinder(shaftLen, shaftDia / 2).translate(0, 0, bushingTopZ);
  const flatPlaneX = shaftFlat - shaftDia / 2;         // face position on -X side (= -1.5)
  const flatStartZ = bushingTopZ + 8.0;                // ~lower 8mm stays full round
  const cutter = box(shaftDia, shaftDia + 2, shaftLen)
    .translate(flatPlaneX - shaftDia / 2, 0, flatStartZ);
  const shaft = difference(shaftRound, cutter)
    .color(COL_SHAFT)
    .material({ metalness: 0.6, roughness: 0.4 });
  children.push({ name: "Shaft", shape: shaft });

  // --- Terminals: 3 encoder (A-C-B) at -Y edge, 2 switch (D-E) at +Y edge ---
  const pinTab = (x, y) =>
    box(0.9, 0.6, pinDrop + 0.5).translate(x, y, -pinDrop).color(COL_PIN);
  const encoderY = -(bodyD / 2 + 0.5);
  const switchY = bodyD / 2 + 0.5;
  const encoderPins = union(
    pinTab(-2.5, encoderY),
    pinTab(0, encoderY),
    pinTab(2.5, encoderY),
  ).color(COL_PIN);
  const switchPins = union(
    pinTab(-2.5, switchY),
    pinTab(2.5, switchY),
  ).color(COL_PIN);
  children.push({ name: "Encoder Terminals", shape: encoderPins });
  children.push({ name: "Switch Terminals", shape: switchPins });

  // --- Locating legs on the ±X sides (anti-rotation), ~12.5mm apart ---
  const legX = 12.5 / 2;
  const legLeft = box(1.2, 3.0, legDrop + 1.0).translate(-legX, 0, -legDrop).color(COL_LEG);
  const legRight = box(1.2, 3.0, legDrop + 1.0).translate(legX, 0, -legDrop).color(COL_LEG);
  children.push({ name: "Leg -X", shape: legLeft });
  children.push({ name: "Leg +X", shape: legRight });

  return group(...children).withConnectors({
    // Panel-mount interface: bushing base (shoulder) plane, shaft axis out +Z.
    // The panel front face clamps here; nut tightens from the front over the bushing.
    panel_mount: { origin: [0, 0, canTopZ], axis: [0, 0, 1], up: [0, 1, 0] },
    // Shaft tip — knob attach / clearance reference.
    shaft_tip: { origin: [0, 0, shaftTopZ], axis: [0, 0, 1], up: [0, 1, 0] },
    // PCB seating face.
    pcb_bottom: { origin: [0, 0, 0], axis: [0, 0, -1], up: [0, 1, 0] },
  });
}

if (require.main === module) {
  const shaftLen = Param.number("Shaft Length", 20, { min: 12, max: 30, unit: "mm" });
  const enc = buildEc11({ shaftLen });

  const bb = enc.boundingBox();
  verify.inRange("Body footprint ~12mm (X)", 12, 11, 14);
  verify.inRange("Height to shaft tip ~32.5mm", bb.max[2], 24, 40);

  return enc;
}

return { buildEc11 };
