# Bill of Materials — 15× Mini Color-Changing Tealight

Prices are approximate (late 2026) and fluctuate with vendor/region/stock.
Two tiers: **Retail** (US distributors, in hand fast, easy to assemble) and
**Budget** (bulk/overseas, cheaper, longer shipping + more soldering).

## Per-unit quantities
Each tealight needs: 1× XIAO ESP32-C3, 1× WS2812 LED, 1× 500 mAh LiPo,
1× momentary button, a few wires, printed body + lid. Resistor/cap optional.

## Retail tier (convenient)

| Part | Qty for 15 | Buy as | ~Unit | ~15-unit | Link |
|------|-----------|--------|-------|----------|------|
| XIAO ESP32-C3 | 15 | singles | $5–6 | **$90** | [Seeed/PiHut](https://thepihut.com/products/seeed-xiao-esp32c3), [TME](https://www.tme.com/in/en/katalog/p,seeed-studio_1325/) |
| WS2812B LED (breakout) | 15 | singles | $1.50 | **$23** | [SparkFun](https://www.sparkfun.com/sparkfun-rgb-led-breakout-ws2812b.html) |
| LiPo 500 mAh protected (503035) | 15 | singles | $7 | **$105** | [Pi Hut](https://thepihut.com/collections/maker-store/products/500mah-3-7v-lipo-battery), [Jameco LJ503035](https://www.jameco.com/z/LJ503035-Jameco-ReliaPro-Lithium-Ion-Polymer-Battery-3-7V-500mAh-Rechargeable_2275319.html) |
| Momentary button 6×6 mm | 1×100-pack | pack | $8/pack | **$8** | [Amazon TWTADE](https://www.amazon.com/s?k=twtade+tactile+push+button) |
| Hookup wire (22 AWG set) | 1 set | set | $12 | **$12** | Amazon "22AWG hookup wire kit" |
| PLA filament (body+lid ≈30 g ea) | ~500 g | 1 kg spool | $18/kg | **$18** | Amazon / Microcenter PLA 1 kg |
| Diffuser | 15 | printed | — | **$0** | from same filament |
| Resistor + cap *(optional)* | kit | assortment | $15 | **$15** | [Walmart cap kit](https://www.walmart.com/ip/356513092) |
| **Total (w/o optional)** | | | | **≈ $256** | **≈ $17 / unit** |
| **Total (w/ resistor+cap kit)** | | | | **≈ $271** | **≈ $18 / unit** |

## Budget tier (bulk / overseas)

| Part | Qty for 15 | Buy as | ~15-unit | Notes |
|------|-----------|--------|----------|-------|
| XIAO ESP32-C3 | 15 | bulk | **~$67** | ~$4.50 ea in volume |
| WS2812B | from strip | 5 m / 300-LED strip | **~$12** | cut 15 singles, solder 3 wires each ([strip](https://shillehtek.com/products/non-waterproof-ws2812b-smd-led-strip-60-led-meter-flexible-5m-roll-5v-ip30)) |
| LiPo 500 mAh | 15 | bulk | **~$60** | ~$4 ea; ensure **protected** cells |
| Button 100-pack | 1 | pack | **~$3** | eBay bulk |
| Wire | 1 | set | **~$8** | |
| PLA | 1 kg | spool | **~$15** | budget brand |
| Resistor/cap kit *(opt)* | 1 | kit | **~$13** | |
| **Total** | | | **≈ $165–178** | **≈ $11–12 / unit** |

## Notes & gotchas
- **LiPo safety/shipping:** buy cells with a built-in protection board (PCM).
  Air shipping of lithium is restricted — domestic ground is simplest; overseas
  orders can be slow or split.
- **Resistor + cap are optional** for the single-pixel, 3V3-powered build. Add
  a ~330–470 Ω series resistor on the LED data line and a ~1000 µF cap across
  LED power only if you move to 5 V or a multi-LED ring/strip.
- **USB-C cable:** need at least one *data-capable* cable for flashing; assumed
  already on hand.
- **WS2812 choice:** breakouts ($1.50) are plug-and-play; cutting a strip is
  ~10× cheaper per LED but adds 3 solder joints per unit.
- **Filament:** one 1 kg spool covers all 15 enclosures (~450–500 g) with margin.
