// esp32-wroom32-devkit.forge.js
// ESP32-WROOM-32 38-pin dev board (wide DevKitC-V4 / DOIT-style, micro-USB) — dimensional reference.
//
// Convention: PCB centered on XY, PCB base at Z=0, components on top +Z,
// header pins extend down -Z. USB faces -X, WROOM module/antenna at +X end.
//
// Sourced canonical dims (mm). NOTE: the "38-pin" board comes in a WIDE variant
// (~28mm, 1.0"/25.40mm row spacing, DevKitC-V4) and a NARROW breadboard-friendly
// variant (~25.4-26mm, 0.9"/22.86mm row spacing). This models the NARROW variant to
// match the target board (~54 x 26 mm):
//   PCB           ~54.0 x 26.0 x ~1.5   (no mounting holes; measured 1.5mm thick)
//   Headers       2 x 19 @ 2.54, row-to-row 22.86 c/c (narrow), ~1.6mm inset from side edges
//   WROOM-32 can  18.0 (Y) x 25.5 (X) x 3.1  (metal shield ~18x18 + PCB antenna tail ~7mm)
//   USB           micro-B, ~5.9 x 7.5 x 2.5, protrudes ~1mm past the -X edge
//   EN + BOOT     SMD tact switches near the USB end
//   Stackup       top +3.1 (module), pins reach ~-8.5 (breadboard-oriented)
// For the WIDE DevKitC variant, build with { pcbLen: 48.26, pcbWidth: 27.94, rowSpacing: 25.40 }.

const COL_PCB = "#1c1c22";      // dark board
const COL_CAN = "#c2c6cc";      // WROOM metal shield
const COL_ANT = "#20242c";      // module PCB / antenna tail
const COL_USB = "#b9bdc4";      // micro-USB shell
const COL_BTN_BODY = "#9aa0a8"; // tact switch metal
const COL_BTN_TOP = "#1a1a1a";  // plunger
const COL_REG = "#101014";      // regulator / ICs
const COL_PIN = "#d8c17a";      // header pins (gold)
const COL_PLASTIC = "#111111";  // header spacer

const DEFAULT_PCB_THK = 1.5;    // measured board thickness (exported as PCB_THK for the enclosure/coupon)

function buildEsp32(props = {}) {
  const pcbLen = props.pcbLen ?? 54.0;   // X
  const pcbWidth = props.pcbWidth ?? 26.0; // Y
  const pcbThk = props.pcbThk ?? DEFAULT_PCB_THK; // Z
  const rowSpacing = props.rowSpacing ?? 22.86; // header center-to-center across Y (narrow 0.9")
  const pitch = 2.54;
  const pinsPerRow = 19;
  const showHeaders = props.showHeaders ?? true;
  const mountHoles = props.mountHoles ?? false; // ~Ø1.6 corner holes, centers 1.4mm in from edges

  const halfLen = pcbLen / 2;
  const children = [];

  // --- PCB (optional Ø1.6 corner mounting holes, 1.4mm in from the edges) ---
  let pcbBoard = box(pcbLen, pcbWidth, pcbThk);
  if (mountHoles) {
    const hx = pcbLen / 2 - 1.4, hy = pcbWidth / 2 - 1.4;
    for (const [mx, my] of [[hx, hy], [hx, -hy], [-hx, hy], [-hx, -hy]]) {
      pcbBoard = difference(pcbBoard, cylinder(pcbThk + 2, 0.8).translate(mx, my, -1));
    }
  }
  children.push({ name: "PCB", shape: pcbBoard.color(COL_PCB) });

  // --- WROOM-32 module at the +X end (shield can + antenna tail) ---
  const moduleEndX = halfLen - 2.0;       // ~2mm keep-out to board edge
  const canLen = 18.0, canWid = 18.0, canH = 3.1;
  const antLen = 7.0, antH = 0.8;
  const antCenterX = moduleEndX - antLen / 2;
  const canCenterX = (moduleEndX - antLen) - canLen / 2;
  children.push({
    name: "WROOM Antenna",
    shape: box(antLen, canWid, antH).translate(antCenterX, 0, pcbThk).color(COL_ANT),
  });
  children.push({
    name: "WROOM Shield",
    shape: box(canLen, canWid, canH)
      .translate(canCenterX, 0, pcbThk)
      .color(COL_CAN)
      .material({ metalness: 0.8, roughness: 0.35 }),
  });

  // --- micro-USB at the -X end ---
  const usbW = 7.5, usbD = 5.9, usbH = 2.5, usbProtrude = 1.0;
  const usbOuterX = -halfLen - usbProtrude;
  const usbCenterX = usbOuterX + usbD / 2;
  children.push({
    name: "USB micro-B",
    shape: box(usbD, usbW, usbH)
      .translate(usbCenterX, 0, pcbThk)
      .color(COL_USB)
      .material({ metalness: 0.7, roughness: 0.4 }),
  });

  // --- EN + BOOT tact switches near the USB end ---
  const btnX = -halfLen + 6.0;
  const btnY = 9.0;
  function tactSwitch(x, y) {
    const body = box(3.9, 3.5, 1.5).translate(x, y, pcbThk).color(COL_BTN_BODY);
    const plunger = cylinder(0.7, 1.1).translate(x, y, pcbThk + 1.5).color(COL_BTN_TOP);
    return group({ name: "Body", shape: body }, { name: "Plunger", shape: plunger });
  }
  children.push({ name: "EN Button", shape: tactSwitch(btnX, btnY) });
  children.push({ name: "BOOT Button", shape: tactSwitch(btnX, -btnY) });

  // --- AMS1117 regulator + a couple of ICs (envelope detail) ---
  children.push({
    name: "Regulator",
    shape: box(6.5, 3.5, 1.6).translate(-halfLen + 14, 10.5, pcbThk).color(COL_REG),
  });
  children.push({
    name: "USB-UART IC",
    shape: box(4.0, 4.0, 1.0).translate(-halfLen + 14, -10.5, pcbThk).color(COL_REG),
  });

  // --- 2 x 19 header rows, pins pointing down (-Z) ---
  if (showHeaders) {
    const fieldLen = (pinsPerRow - 1) * pitch; // 45.72
    const firstX = -fieldLen / 2;
    const spacerZ = -2.5; // plastic under the board (breadboard style)
    const pinTopZ = pcbThk;
    const pinBotZ = -8.5;
    const pinH = pinTopZ - pinBotZ;
    for (const rowY of [rowSpacing / 2, -rowSpacing / 2]) {
      const side = rowY > 0 ? "Row +Y" : "Row -Y";
      const spacer = box(fieldLen + pitch, 2.54, 2.5)
        .translate(0, rowY, spacerZ)
        .color(COL_PLASTIC);
      const basePin = box(0.64, 0.64, pinH).translate(firstX, rowY, pinBotZ);
      const rowPins = linearPattern(basePin, pinsPerRow, pitch, 0, 0).color(COL_PIN);
      children.push({ name: `Header ${side} Spacer`, shape: spacer });
      children.push({ name: `Header ${side} Pins`, shape: rowPins });
    }
  }

  return group(...children).withConnectors({
    // USB opening, looking out -X — align to an enclosure access hole.
    usb: { origin: [usbOuterX, 0, pcbThk + usbH / 2], axis: [-1, 0, 0], up: [0, 0, 1] },
    // PCB bottom center, looking down -Z — mounting/standoff reference.
    pcb_bottom: { origin: [0, 0, 0], axis: [0, 0, -1], up: [0, 1, 0] },
    // Top of the WROOM shield, looking up +Z.
    module_top: { origin: [canCenterX, 0, pcbThk + canH], axis: [0, 0, 1], up: [1, 0, 0] },
  });
}

if (require.main === module) {
  const showHeaders = Param.bool("Show Headers", true);
  const esp = buildEsp32({ showHeaders });

  const pcbBB = box(54.0, 26.0, 1.0).boundingBox();
  verify.inRange("PCB length ~54mm", pcbBB.max[0] - pcbBB.min[0], 47, 56);
  verify.inRange("PCB width ~26mm", pcbBB.max[1] - pcbBB.min[1], 24, 29);

  return esp;
}

return { buildEsp32, PCB_THK: DEFAULT_PCB_THK };
