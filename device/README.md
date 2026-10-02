# Mini Color-Changing LED Tealight 

A tiny battery-powered "tealight" that glows and shifts color like a candle.
Firmware for a **Seeed XIAO ESP32-C3** driving a **WS2812 / NeoPixel** RGB LED,
with a button to cycle effects.

## Effects
1. **CandleColor** — warm flame flicker with a slow color drift (default/signature look)
2. **ColorCycle** — smooth rainbow hue sweep
3. **Breathing** — brightness eases in/out over a drifting hue
4. **Solid** — static color

Color, speed, and intensity are runtime-settable (over the REST API below) —
the button only cycles which effect is active and how bright it is:

- **Short press** → next effect
- **Long press (≥0.6s)** → cycle brightness (40 / 90 / 160)
- **Hold through power-on (≥3s)** → wipe stored WiFi credentials and
  re-enter BLE pairing mode

All of this works completely standalone, with no controller present.

## Parts list

### Required
| # | Part | Notes |
|---|------|-------|
| 1 | **Seeed Studio XIAO ESP32-C3** | The MCU. Any ESP32-C3 dev board works; pin labels may differ. |
| 1 | **WS2812 / WS2812B NeoPixel** | Single RGB LED — bare 5mm/PCB module, or a small ring/strip. |
| 1 | **Momentary push button** | Tactile switch, normally-open, 2-leg. |
| — | **USB-C cable** | For flashing + dev power (data-capable, not charge-only). |
| — | **Hookup wire** | Breadboard jumpers, or thin silicone wire for a soldered build. |

### Recommended (clean build / multiple LEDs)
| # | Part | Notes |
|---|------|-------|
| 1 | **~330–470 Ω resistor** | In series on the LED DIN line — protects the first pixel. |
| 1 | **~1000 µF electrolytic cap (≥6.3V)** | Across LED VCC/GND — absorbs inrush, steadies power. |
| 1 | **Breadboard** | For prototyping before soldering. |

### For a battery-powered tealight (optional, finishing it off)
| # | Part | Notes |
|---|------|-------|
| 1 | **3.7V LiPo cell** | e.g. 302030 / 402030 (~150–250 mAh fits a tealight shell). XIAO ESP32-C3 has an onboard LiPo charger + `BAT+`/`BAT-` pads. |
| 1 | **Slide/slider switch** | Optional hard power cutoff in the battery line. |
| — | **Diffuser** | Frosted/ping-pong ball, wax shell, or printed cap to spread the glow. |

> **Power notes:** one WS2812 at full white draws ~60 mA; code caps brightness
> so it's lower in practice. A single pixel runs fine off the XIAO's 3V3 pin.
> Only step up to a dedicated 5V rail (and the cap + resistor above) once you
> add a ring/strip of several LEDs.

## Wiring

```
  XIAO ESP32-C3                 WS2812 / NeoPixel
  ┌──────────────┐
  │ D0 (GPIO2) ──┼───────────── DIN
  │ 5V / 3V3   ──┼───────────── VCC   (5V is brightest; 3V3 works for 1 pixel)
  │ GND        ──┼──────┬────── GND
  │              │      │
  │ D1 (GPIO3) ──┼───[ button ]─┘      (button other leg -> GND; uses INPUT_PULLUP)
  └──────────────┘
```

> For more than a couple of pixels, power the LED(s) from a proper 5V rail and
> add a ~300–500Ω resistor on DIN and a ~1000µF cap across VCC/GND.

## Button (what to buy & how to wire)

Any **momentary, normally-open (NO)** push button works — the firmware uses
`INPUT_PULLUP`, so one leg goes to **D1 (GPIO3)** and the other to **GND**. No
resistor needed; debounce is handled in software.

- **Prototyping:** a standard **6×6mm through-hole tactile switch** (4-leg,
  e.g. Omron B3F-1000 or any generic 6×6×5mm). Use a *diagonal* pin pair.
- **Finished enclosure:** a **panel-mount momentary push button**. The STL's
  side hole is **7 mm** (`BTN_D` in `enclosure/tealight_case.py`) — match it to
  the button's thread/bezel. A common fit is a 7–8 mm SPST-NO momentary
  (e.g. "PBS-33B" style). Buying a 12 mm one? Just bump `BTN_D` and re-render.

Short press = next effect · long press (≥0.6 s) = brightness step.

## Battery & runtime

Target cell: **3.7 V LiPo, ~500 mAh, with a built-in protection board (PCM).**
Good form factor: **503035** (5 × 30 × 35 mm) — fits the enclosure and solders
to the XIAO's `BAT+` / `BAT-` pads (onboard charger, charges over USB-C).

Runtime estimate:

| Draw source | Typical |
|---|---|
| XIAO ESP32-C3 (radio off, running effects) | ~25 mA |
| One WS2812 at capped brightness, warm colors | ~10–15 mA |
| **Average total** | **~40 mA** |

- **500 mAh ÷ ~40 mA ≈ 10–12 h** ideal → **~8–10 h real** (LiPo usable ~85%).
- Even if draw runs high (~60–70 mA), 500 mAh still clears the **6–8 h** goal.
- Want it smaller? A **~400 mAh** cell (e.g. 402535) still gives ~7–8 h.
- Recharge: default XIAO charge current ~100 mA → full in ~5–6 h (a solder pad
  bumps this higher if you want faster charging).

> Use a **protected** LiPo. Don't fully seal the cell in with no slack — leave a
> little room and secure with foam tape, not glue directly on the pouch.

> ⚠️ **Wireless changes this math.** The table above assumes the radio is off.
> This firmware keeps **WiFi connected** (BLE provisioning + REST API + mDNS),
> which pushes average draw to roughly **80–120 mA**, cutting a 500 mAh cell to
> **~4–6 h** — possibly under the 6–8 h goal. To hit 6–8 h *with* WiFi, either
> go bigger (**~1000 mAh**, e.g. 603048) or add WiFi modem/light-sleep between
> API calls. Pure-local (radio off) still gets 8–10 h on 500 mAh.

## Cost to build (15 units)

Full priced bill of materials with vendor links: **[`../BOM.md`](../BOM.md)**.

Headline (approx, late 2026):

| Tier | 15 units | Per unit |
|------|----------|----------|
| **Retail** (US distributors, easy assembly) | **≈ $256** | **~$17** |
| **Budget** (bulk / overseas, more soldering) | **≈ $165–178** | **~$11–12** |

Cost drivers are the **XIAO (~$90/15)** and **LiPo (~$105/15)** — ~75% of total;
bulk-sourcing those is where the savings are. The resistor + cap are optional for
the single-pixel 3V3 build, and the diffuser prints from the same filament.

## Networking & the fleet controller

Each tealight is **API-only** — it has no web UI of its own. Out of the box
it advertises over BLE as `Tealight-XXXX`, ready to be found and paired by
the [`../controller`](../controller/README.md) web app, which assigns it a
friendly name and WiFi credentials. From then on the tealight:

- joins that WiFi network on every boot (credentials stored in flash),
- answers a small REST API on port 80:
  - `GET /api/state` / `POST /api/state` — mode, hue, sat, speed, intensity, brightness
  - `GET /api/modes` — available effects, so a client never has to hardcode the list
  - `GET /api/info` — fingerprint, name, firmware version, uptime, IP
  - `POST /api/identify` — brief white flash, for finding the physical unit
- advertises itself over mDNS as `_tealight._tcp` with its fingerprint in a
  TXT record, so the controller can re-find it if its IP ever changes.

The **fingerprint** (12 hex chars, derived from the chip's efuse MAC) is the
tealight's permanent identity — independent of WiFi IP or BLE address. If a
tealight ever needs to move to a different network, hold its button through
power-on for ≥3s to wipe its stored credentials and it'll re-advertise over
BLE for pairing again.

## Build & flash

Requires [PlatformIO](https://platformio.org/) (`pip install platformio` or the VS Code extension).

```bash
pio run                 # compile
pio run -t upload       # flash over USB-C
pio device monitor      # serial log @ 115200
```

## Configure
Pins, LED count, brightness cap, animation timing, WiFi/BLE/identify timing,
and the BLE UUIDs all live in [`src/config.h`](src/config.h). For a
ring/strip, bump `NUM_LEDS` — effects scale automatically. If you change the
BLE UUIDs, update the matching constants in
[`../controller/src/config.h`](../controller/src/config.h) too — the two
firmwares only recognize each other by those UUIDs matching exactly.

## No hardware yet?
Drop the sketch into [Wokwi](https://wokwi.com/) (ESP32-C3 + NeoPixel) to
preview the effects before parts arrive.
