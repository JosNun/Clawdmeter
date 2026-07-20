// components.forge.js
// Combined preview of the three reference components the design is built around.
// Also demonstrates the import pattern the enclosure model will use:
//   each part file exports a builder — require it, call the builder, place it.
//
// All parts share the convention: PCB/mount plane at Z=0, components up +Z,
// leads/pins down -Z, so they sit on a common Z=0 "board plane" here.

const oledModule = require("./oled-ssd1306-091.forge.js");
const espModule = require("./esp32-wroom32-devkit.forge.js");
const ec11Module = require("./ec11-encoder.forge.js");

const oled = oledModule.buildOled().translate(0, 34, 0);
const esp32 = espModule.buildEsp32().translate(0, 0, 0);
const encoder = ec11Module.buildEc11().translate(42, 20, 0);

return group(
  { name: "OLED 0.91in", group: oled },
  { name: "ESP32 DevKit", group: esp32 },
  { name: "EC11 Encoder", group: encoder },
);
