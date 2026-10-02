# Device ↔ Controller Protocol

This is the contract between [`device/`](device/README.md) (the tealight
firmware) and [`controller/`](controller/README.md) (the fleet controller).
They're separate PlatformIO projects with no shared code, so this file is
the only thing keeping them in sync — **if you change any of this on one
side, update this file and the matching side in the same change.**

The constants below live in `device/src/config.h` and
`controller/src/config.h` and **must stay identical** in both:
`BLE_SVC_UUID`, `BLE_CHR_CREDS_UUID`, `BLE_CHR_STATUS_UUID`, `BLE_CHR_INFO_UUID`,
`MDNS_SERVICE` (`"tealight"`), `MDNS_PROTO` (`"tcp"`), `DEVICE_PORT`/`API_PORT` (`80`).

## Identity: the fingerprint

Every tealight has a **fingerprint** — 12 lowercase hex characters derived
from `ESP.getEfuseMac()` (the chip's factory-burned MAC). It's the device's
permanent identity:

- independent of its current WiFi IP (which can change on any DHCP renewal),
- independent of WiFi MAC randomization,
- independent of its BLE address.

It appears in three places: the BLE `InfoRead` characteristic, the mDNS TXT
record, and as the key the controller uses for its device registry
(`controller/src/registry.h`). If you ever need two tealights to be
distinguishable, this is the field to key on — never IP, never BLE address.

## 1. BLE provisioning (before the device has WiFi)

A tealight advertises over BLE **only** while unprovisioned, or after its
button is held ≥3s through power-on (wipes stored credentials and re-enters
this mode). Once WiFi connects, advertising stops.

- **Advertised name:** `Tealight-XXXX` (last 4 hex chars of the fingerprint)
- **Service UUID:** `e2ebef84-3a8f-4682-89dd-5d57827d5eaa`

| Characteristic | UUID | Properties | Payload |
|---|---|---|---|
| `InfoRead` | `caa25f32-b6f0-46cc-8cf6-691efcb3994e` | read | `{"fingerprint","name","fw"}` |
| `CredsWrite` | `53c2f022-9349-49ef-8b56-903bfa3637b6` | write | `{"ssid","pass","name"}` |
| `StatusNotify` | `0886dd9c-de2e-4b35-8f33-01446fb6fe39` | read, notify | `{"status","fingerprint","ip","msg"}` |

`status` is one of `"idle" | "connecting" | "connected" | "failed"`.

**Pairing sequence** (controller is the BLE central, device is the peripheral):
1. Central scans, filters by the service UUID, finds the device.
2. Central connects, reads `InfoRead` to get the fingerprint/default name.
3. Central subscribes to `StatusNotify`.
4. Central writes `{"ssid","pass","name"}` to `CredsWrite`.
5. Device stores the credentials (NVS/Preferences), begins connecting, and
   publishes `StatusNotify` updates as its WiFi state changes.
6. Central waits for `status: "connected"` (with `ip` populated) or
   `"failed"`, then disconnects. The device keeps running either way — on
   `"failed"` it falls back to BLE advertising for another attempt.

A device never clears its BLE-advertised identity info itself on failure;
the controller/UI decides whether to retry.

## 2. mDNS (once the device has WiFi)

On WiFi connect, the device advertises:

- **Service:** `_tealight._tcp` on port `80`
- **Hostname:** `tealight-<fingerprint>.local` (always DNS-safe — never the
  free-form friendly name, which can contain spaces/symbols)
- **TXT records:** `fp` (fingerprint), `name` (current friendly name), `fw`
  (firmware version string)

The controller browses this every ~15s to track each fingerprint's current
IP, plus does an on-demand resolve the moment a cached IP stops answering.
This is the whole mechanism that makes IP changes invisible to the user —
nothing else keys off IP.

## 3. Device REST API (port 80, JSON, no auth)

Consumed only by the controller — tealights have no browser-facing UI of
their own.

| Route | Method | Body | Response |
|---|---|---|---|
| `/api/state` | GET | — | `{mode,modeName,hue,sat,speed,intensity,brightness}` |
| `/api/state` | POST | any subset of `{mode\|modeName,hue,sat,speed,intensity,brightness}` | full state, as GET |
| `/api/modes` | GET | — | `[{index,name}, ...]` — **never hardcode this list elsewhere**; it's how a new effect becomes controllable without touching the controller |
| `/api/info` | GET | — | `{fingerprint,name,fw,uptimeMs,ip}` |
| `/api/identify` | POST | — | `{ok:true}` — triggers a ~2s locate-me flash |

`POST /api/state` is a **partial patch**: omit any field to leave it
unchanged. `mode` is an integer index into whatever `/api/modes` currently
returns; `modeName` (string, case-insensitive) is accepted as an
alternative when `mode` is absent. `hue`/`sat`/`speed`/`intensity`/
`brightness` are all `0-255`.

## Adding a new effect

This is the one change most likely to come from the hardware-firmware side,
so it's worth spelling out exactly what it touches:

1. `device/src/effects.h` — add one `Mode` enum entry (before `COUNT`).
2. `device/src/effects.cpp` — add one `fx*()` function taking
   `(CRGB* leds, uint32_t t, const EffectParams& p)`, one case in
   `renderMode()`, one string in `modeName()`.

Nothing else needs to change — `GET /api/modes` on the device and the
mode dropdown in the controller's web UI both pick it up automatically.
Don't add a new top-level REST field for an effect-specific parameter;
reuse `hue`/`sat`/`speed`/`intensity` (see `EffectParams` in
`device/src/effects.h`) the way the existing four effects do, so the
controller's generic sliders keep working without a matching code change.
