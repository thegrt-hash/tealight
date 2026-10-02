# Tealight Fleet Controller

Firmware for a **Seeed XIAO ESP32-S3** that finds, pairs, and controls a
growing fleet of [tealights](../device/README.md) from one web app — no
phone app, no cloud, nothing but your LAN.

## What it does

1. **Discovers** unprovisioned tealights over Bluetooth (they advertise as
   `Tealight-XXXX` until paired).
2. **Provisions** them onto your WiFi, one at a time — connects over BLE,
   hands over your SSID/password, and waits for the tealight to confirm it
   joined.
3. **Tracks** every tealight by its hardware fingerprint, not its IP — a
   background mDNS browse re-resolves a device's current IP automatically if
   it ever changes (new DHCP lease, reboot, etc.), with an on-demand resolve
   as a fallback the moment a cached IP stops answering.
4. **Serves a web app** (open `http://<controller-ip>/` in any browser on
   the LAN) with a card per tealight: effect, color, speed, intensity,
   brightness, identify, rename, forget.

The browser only ever talks to the controller — it proxies every device
command by fingerprint, so a tealight's IP changing mid-session never breaks
the UI.

## Hardware

Just the XIAO ESP32-S3 itself — no extra parts. Flash it, power it from any
USB-C source on your network, and open its IP in a browser.

## Build & flash

Requires [PlatformIO](https://platformio.org/).

```bash
cp src/secrets.h.example src/secrets.h   # fill in your WiFi SSID/password
pio run -t uploadfs                       # upload the web UI (data/) to LittleFS
pio run -t upload                         # flash firmware over USB-C
pio device monitor                        # serial log @ 115200 — prints the controller's IP
```
`src/secrets.h` is gitignored — it's the controller's own WiFi credentials,
set at build time (there's no separate provisioning flow for the controller
itself; it's one board you flash directly).

Re-run `pio run -t uploadfs` any time you change files under `data/`.

## Adding a tealight

1. Power on a fresh tealight (or hold its button ≥3s to reset one back to
   pairing mode — see [`../device/README.md`](../device/README.md)).
2. In the web app, click **+ Add device** → **Scan for devices**.
3. Pick it from the list, enter your WiFi SSID/password and a friendly name,
   and click **Connect**.
4. Once it reports success it appears in the grid, ready to control.

## How identity survives an IP change

Every tealight has a **fingerprint** — 12 hex characters derived from its
chip's factory-burned MAC, independent of WiFi IP or BLE address. The
controller's registry (`/registry.json` on its own flash) keys everything by
fingerprint. A background task browses `_tealight._tcp` over mDNS every 15s
and refreshes each known fingerprint's IP; if a control action hits a stale
IP, the controller resolves it on the spot before giving up. You'll never
need to re-pair a tealight just because your router handed it a new address.

## REST API (consumed by the bundled web app)

| Route | Method | Body | Does |
|---|---|---|---|
| `/api/devices` | GET | — | List known devices + last-polled state |
| `/api/device/modes?fp=` | GET | — | Proxied list of that device's effects |
| `/api/device/state` | POST | `{fingerprint, ...fields}` | Proxied state patch |
| `/api/device/identify` | POST | `{fingerprint}` | Proxied locate-flash |
| `/api/device/rename` | POST | `{fingerprint, name}` | Registry only |
| `/api/device/forget` | POST | `{fingerprint}` | Registry only |
| `/api/discovery/start` | POST | — | Begin a 5s BLE scan |
| `/api/discovery/found` | GET | — | Devices found so far/after the scan |
| `/api/discovery/provision` | POST | `{address, addrType, ssid, pass, name}` | Pair one device |
| `/api/discovery/status` | GET | — | Progress of the in-flight pairing run |

## Configure

Timing (scan window, provisioning timeout, mDNS browse interval, poll
interval) and the BLE UUIDs live in [`src/config.h`](src/config.h). The BLE
UUIDs **must match** [`../device/src/config.h`](../device/src/config.h)
exactly — that's the only thing that lets the two firmwares recognize each
other.

## No hardware yet?

Everything here needs a real ESP32-S3 (NimBLE, the async web server, and
LittleFS don't have a meaningful desktop simulator) — there's no Wokwi
shortcut for the controller the way there is for a single tealight.
