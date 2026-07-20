// oled-ssd1306-091.forge.js
// 0.91" I2C SSD1306 OLED module (128x32, 4-pin GND/VCC/SCL/SDA) — dimensional reference.
//
// Convention: PCB centered on XY, PCB base at Z=0, components stack up +Z,
// header pins extend down -Z. The display looks up toward +Z.
//
// Sourced canonical dims (mm):
//   PCB          38.0 x 12.0 x ~1.0            (no mounting holes on this small module)
//   Glass panel  30.0 x 11.5 x ~1.4            flush to the short edge opposite the header
//   Active area  22.384 x 5.584                inside a 24.384 x 7.584 viewing window
//   Header       1x4 @ 2.54 mm, short edge     order GND / VCC / SCL / SDA
//   Bare box     ~38 x 12 x 2.7
// Active-area offset toward the header end is derived (the SSD1306 COG driver + FPC
// fold occupy the opposite end, leaving a ~5-6 mm dead band there).

const COL_PCB = "#123a6b";      // blue module PCB
const COL_GLASS = "#0b0b12";    // dark glass
const COL_WINDOW = "#181820";   // viewing window bezel
const COL_LIT = "#a9e4ff";      // lit pixels (emissive)
const COL_PLASTIC = "#141414";  // header body
const COL_PIN = "#c8c8cc";      // header pins

function buildOled(props = {}) {
  const pcbLen = props.pcbLen ?? 38.0;     // X
  const pcbWidth = props.pcbWidth ?? 12.0; // Y
  const pcbThk = props.pcbThk ?? 1.0;      // Z
  const glassLen = props.glassLen ?? 30.0;
  const glassWidth = props.glassWidth ?? 11.5;
  const glassThk = props.glassThk ?? 1.4;
  const adhesiveGap = props.adhesiveGap ?? 0.2;
  const fpcDeadBand = props.fpcDeadBand ?? 5.5; // dead glass on the FPC/driver end
  const showHeader = props.showHeader ?? true;

  const activeW = 22.384, activeH = 5.584;
  const windowW = 24.384, windowH = 7.584;
  const halfLen = pcbLen / 2; // 19

  // --- PCB ---
  const pcbBoard = box(pcbLen, pcbWidth, pcbThk).color(COL_PCB);

  // --- Glass, flush to the +X short edge (header lives at -X) ---
  const glassMaxX = halfLen;
  const glassMinX = glassMaxX - glassLen;
  const glassCenterX = (glassMinX + glassMaxX) / 2;
  const glassBaseZ = pcbThk + adhesiveGap;
  const glassTopZ = glassBaseZ + glassThk;
  const oledGlass = box(glassLen, glassWidth, glassThk)
    .translate(glassCenterX, 0, glassBaseZ)
    .color(COL_GLASS);

  // --- Viewing window + lit active area, offset toward the header (-X) ---
  const activeMaxX = glassMaxX - fpcDeadBand;
  const activeCenterX = activeMaxX - activeW / 2;
  const viewWindow = box(windowW, windowH, 0.06)
    .translate(activeCenterX, 0, glassTopZ)
    .color(COL_WINDOW);
  const litArea = box(activeW, activeH, 0.08)
    .translate(activeCenterX, 0, glassTopZ + 0.02)
    .color(COL_LIT)
    .material({ emissive: COL_LIT, emissiveIntensity: 0.7, roughness: 0.4 });

  // --- Back-side component row (diodes/passives): ~1mm proud of the PCB back,
  //     ~2.5mm wide, spanning the width, ~13mm in from the glass (non-header) end. ---
  const backRowX = glassMaxX - 13;   // ~6mm from center, 13mm in from the +X edge
  const backRow = box(2.5, 11, 1.0)
    .translate(backRowX, 0, -1.0)    // sits on the PCB back (z 0), protruding to z -1
    .color(COL_PLASTIC);

  const children = [
    { name: "PCB", shape: pcbBoard },
    { name: "Glass", shape: oledGlass },
    { name: "Viewing Window", shape: viewWindow },
    { name: "Active Area", shape: litArea },
    { name: "Back Components", shape: backRow },
  ];

  // --- 4-pin header on the -X short edge ---
  if (showHeader) {
    const pitch = 2.54, nPins = 4;
    const rowSpan = (nPins - 1) * pitch; // 7.62
    const hdrX = -halfLen + 1.6;
    const hdrPlastic = box(2.54, rowSpan + 2.54, 2.5)
      .translate(hdrX, 0, pcbThk)
      .color(COL_PLASTIC);
    const pinH = pcbThk + 5.5; // spans from -3 (below PCB) to plastic top
    const basePin = box(0.64, 0.64, pinH).translate(hdrX, -rowSpan / 2, -3);
    const hdrPins = linearPattern(basePin, nPins, 0, pitch, 0).color(COL_PIN);
    children.push({ name: "Header Body", shape: hdrPlastic });
    children.push({ name: "Header Pins", shape: hdrPins });
  }

  return group(...children).withConnectors({
    // Center of the lit display, looking out +Z — align to a bezel/window cutout.
    display: { origin: [activeCenterX, 0, glassTopZ], axis: [0, 0, 1], up: [0, 1, 0] },
    // Bottom face center of the PCB, looking down -Z — mounting/standoff reference.
    pcb_bottom: { origin: [0, 0, 0], axis: [0, 0, -1], up: [0, 1, 0] },
  });
}

if (require.main === module) {
  const showHeader = Param.bool("Show Header", true);
  const oled = buildOled({ showHeader });

  const bb = oled.boundingBox();
  verify.inRange("PCB length ~38mm", bb.max[0] - bb.min[0], 37.5, 38.5);
  verify.inRange("PCB width ~12mm", bb.max[1] - bb.min[1], 11.5, 12.5);

  return oled;
}

return { buildOled };
