# Clawdmeter case

A 3D-printable desktop enclosure for the ESP32-WROOM-32 + SSD1306 build — a
sloped-front console with a 60° display facet carrying the 0.91" OLED (right) and
an EC11 rotary encoder + knob (left), a tall back for the micro-USB port, and a
removable, screw-on base plate.

The model is **parametric [ForgeCAD](https://forgecad.io)** (`.forge.js` — plain
JavaScript that builds solid geometry), so you can tweak a dimension and
re-export rather than editing a mesh. These files are a vendored copy of the
standalone `clawdmeter-model` ForgeCAD project.

![assembly render](assembly.png)
<!-- render with: forgecad render 3d enclosure.forge.js --camera iso --backend occt -->

## Files

| File | What it is |
| --- | --- |
| `enclosure.forge.js` | **The enclosure.** Body + base plate + placed components. This is what you export to print. |
| `enclosure-features.forge.js` | Single source of truth for the mating features (OLED bezel/retention, USB slot, encoder hole, heat-set bosses, snap clips). Both the enclosure and the coupons build from these. |
| `test-coupons.forge.js` | Small, quick-to-print **fit-test coupons** — each reproduces one mating feature at true dimensions so you can validate a fit before committing to the full print. |
| `components.forge.js` | Reference-only preview of the three components together (not printed). |
| `esp32-wroom32-devkit.forge.js` | Dimensional reference model of the ESP32-WROOM-32 dev board (narrow 54×26 mm variant). |
| `oled-ssd1306-091.forge.js` | Dimensional reference model of the 0.91" SSD1306 OLED module. |
| `ec11-encoder.forge.js` | Dimensional reference model of the EC11 rotary encoder. |
| `forgecad.json` | ForgeCAD project manifest — marks the folder as a named project so `forgecad studio` opens it and optional hosted sync can link it. Not required for `run`/`export`/`render` (those resolve the cross-file imports on their own). |

## Prerequisites

```bash
npm install -g forgecad
```

The CLI is free for personal, non-commercial use, which covers `run`,
`studio`, `render 3d`, and `export stl`/`export 3mf` — everything below.

This git repo is the source of truth for the model; the hosted
[forgecad.io](https://forgecad.io) platform is optional. If you want the live
web editor and share links, link the folder to your own hosted project:

```bash
forgecad login
forgecad project push        # creates a new hosted project under your account
```

(`forgecad.json` here is a fresh, unlinked manifest — no account is baked in.)

> **⚠️ Build on the OCCT backend.** The rounded top/facet edges are 3D fillets on
> a boolean-cut body, which only render correctly on OCCT. On the CLI pass
> **`--backend occt`** to every command; in the editor pick OCCT from the backend
> selector. On the default (Manifold) backend the edge finishes come out wrong.

## Preview it

Open the interactive editor (parameters become sliders, 3D updates live on save):

```bash
forgecad studio case/          # from the repo root
```

Or validate / render headless without a browser window:

```bash
forgecad run enclosure.forge.js --backend occt                 # build + verify, prints a summary
forgecad render 3d enclosure.forge.js --camera iso --backend occt   # → enclosure.png
```

## Print it

STL/3MF export always outputs the **whole** returned model, so the enclosure
exposes an **`Export Part`** parameter to pick one printable part at a time.
Print these three (the components, headers, and screws are reference geometry —
not printed):

| Part | `Export Part` value | Notes |
| --- | --- | --- |
| **Body** | `Body` | The shell. 4 corner bosses for M3 heat-set inserts. |
| **Base Plate** | `Base Plate` | Snaps/screws on from below; carries the board standoffs + locating pegs. |
| **OLED Retainer** | `OLED Retainer` | Small bar that screws over the OLED's lower edge (2× M2 self-tap). |

```bash
forgecad export stl enclosure.forge.js --backend occt --param "Export Part=Body"          --output body.stl
forgecad export stl enclosure.forge.js --backend occt --param "Export Part=Base Plate"    --output base-plate.stl
forgecad export stl enclosure.forge.js --backend occt --param "Export Part=OLED Retainer" --output oled-retainer.stl
```

`Export Part=Assembly (all)` (the default) keeps the full reference view with the
components in place — handy for a render, not for printing.

Run a printability sanity check first if you like:

```bash
forgecad check print enclosure.forge.js --backend occt --param "Export Part=Body"
```

**Hardware you'll need:** 4× M3 heat-set inserts + M3 socket-head screws (base
plate → body bosses), 2× M2 self-tapping screws (OLED retainer bar). The knob is
a friction fit on the EC11's 6 mm D-shaft.

## Test fits before you commit

The full body is a long print. Before that, print a **coupon** — one feature at
true dimensions, pulled from the same `enclosure-features.forge.js`, so the fit
matches the real part. Drop in the real component, confirm the fit, tune the
feature in `enclosure-features.forge.js` (both the coupon and the enclosure
update together), then print the body.

Pick one with the `Coupon` parameter:

```bash
forgecad export stl test-coupons.forge.js --backend occt --param "Coupon=Encoder hole" --output coupon-encoder.stl
```

Available coupons: `OLED holder`, `OLED retainer bar`, `Encoder hole`,
`ESP32 retention`, `Boss body`, `Boss base plate`. Add
`--param "Show Component=true"` in the editor/renders to overlay the real part
translucently — turn it **off** before exporting an STL.

## Editing

Dimensions are parameters at the top of `buildEnclosure()` in
`enclosure.forge.js` (wall thickness, corner radius, facet tilt/length, etc.).
Mating features (fits, clearances, insert sizes) live in
`enclosure-features.forge.js` — change them there so the coupons stay truthful.
See the repo-root README and [forgecad.io](https://forgecad.io) docs for the
modeling API.
