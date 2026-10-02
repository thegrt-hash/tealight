# Tealight

A growing fleet of color-changing LED "tealights" — plus a controller that finds, pairs, and runs all of them from one web app.

- **[`device/`](device/README.md)** — firmware for each tealight itself: a Seeed XIAO ESP32-C3 + WS2812 LED in a 3D-printed shell. Standalone (button-driven) or networked (WiFi + REST API + BLE pairing).
- **[`controller/`](controller/README.md)** — firmware for one Seeed XIAO ESP32-S3 that discovers tealights over Bluetooth, provisions them onto WiFi, and serves a web app to control the whole fleet (color, effect, speed, brightness).
- **[`enclosure/`](enclosure/)** — 3D-printable body/lid for a single tealight.
- **[`BOM.md`](BOM.md)** — parts list and pricing for building a batch of tealights.
- **[`PROTOCOL.md`](PROTOCOL.md)** — the BLE/mDNS/REST contract between `device/` and `controller/`. If you're changing either firmware's networking, pairing, or REST surface, this is the file to read and update first.

## How it fits together
```
┌─────────────┐  BLE (pairing only)   ┌──────────────┐
│  Tealight   │◄─────────────────────►│  Controller  │
│ (ESP32-C3)  │                       │  (ESP32-S3)  │
│             │   WiFi (REST + mDNS)  │              │──► Web UI (browser)
│             │◄─────────────────────►│              │
└─────────────┘                       └──────────────┘
```
Each tealight is unprovisioned (and advertising over BLE) until the controller pairs it onto your WiFi. After that, the controller tracks it by a stable hardware fingerprint — not its IP — so it keeps working even after a DHCP lease changes. See [`controller/README.md`](controller/README.md) for the discovery/pairing flow and [`device/README.md`](device/README.md) for the tealight's own REST API.

Both are separate PlatformIO projects — build/flash each from its own directory:
```bash
cd device      && pio run -t upload
cd controller  && pio run -t uploadfs && pio run -t upload
```
