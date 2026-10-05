# Device ↔ Controller Protocol

This is the contract between [`device/`](device/README.md) (the tealight
firmware) and [`controller/`](controller/README.md) (the fleet controller).
They're separate PlatformIO projects with no shared code, so this file is
the only thing keeping them in sync — **if you change any of this on one
side, update this file and the matching side in the same change.**

As of firmware **2.x**, control runs over **ESP-NOW** (connectionless 2.4 GHz),
not WiFi/HTTP. Tealights never associate to an AP — that's the whole battery
win. WiFi/REST/mDNS are gone; BLE survives only for first-run naming.

The constants below **must stay identical** in `device/src/config.h` and
`controller/src/config.h`: `BLE_SVC_UUID`, `BLE_CHR_CREDS_UUID`,
`BLE_CHR_STATUS_UUID`, `BLE_CHR_INFO_UUID`, and **`FLEET_CHANNEL`**. The ESP-NOW
packet layout in `espnow_proto.h` is duplicated verbatim in both `src/` trees
and must match byte-for-byte.

## Identity: the fingerprint

Every tealight has a **fingerprint** — 12 lowercase hex characters derived
from `ESP.getEfuseMac()` (the chip's factory-burned MAC). It's the device's
permanent identity, independent of its ESP-NOW/STA MAC and BLE address. It is
the key the controller registry (`controller/src/registry.h`) uses. The
device's **STA MAC** (the ESP-NOW address) is a separate 6-byte value the
device reports in every message; the controller stores it to unicast back.

## 0. The shared channel (read this first)

Every node — controller and all devices — must have its radio on the same
2.4 GHz channel, `FLEET_CHANNEL`. The controller is a WiFi STA on the home
network, so its radio sits on the **home AP's channel**. Therefore:

> **Lock your router's 2.4 GHz radio to a fixed channel and set
> `FLEET_CHANNEL` to that number in both configs.**

If they disagree the controller logs a warning at boot and the fleet never
hears a command. (To go router-independent, switch the controller to a SoftAP
on a hardcoded channel — a small change in `controller/src/main.cpp`.)

## 1. BLE naming (first run only)

A tealight advertises over BLE **only** while un-named, or after its button is
held ≥3 s through power-on (wipes the stored name). Once named it stops
advertising and lives purely on ESP-NOW.

- **Advertised name:** `Tealight-XXXX` (last 4 hex chars of the fingerprint)
- **Service UUID:** `e2ebef84-3a8f-4682-89dd-5d57827d5eaa`

| Characteristic | UUID | Properties | Payload |
|---|---|---|---|
| `InfoRead` | `caa25f32-b6f0-46cc-8cf6-691efcb3994e` | read | `{"fingerprint","name","fw"}` |
| `NameWrite` | `53c2f022-9349-49ef-8b56-903bfa3637b6` | write | `{"name"}` |
| `StatusNotify` | `0886dd9c-de2e-4b35-8f33-01446fb6fe39` | read, notify | `{"status","fingerprint","name"}` |

`status` is `"unnamed"` or `"named"`. (The `NameWrite` UUID is the old
`CredsWrite` UUID reused for wire-compat; it no longer carries WiFi creds.)

**Naming sequence** (controller is the BLE central):
1. Central scans, filters by the service UUID, finds the device.
2. Central connects, reads `InfoRead` for the fingerprint.
3. Central writes `{"name"}` to `NameWrite`; the device persists it, stops
   advertising, and announces itself on ESP-NOW.
4. Central records `fingerprint → name` in its registry. The MAC is filled in
   later, from the device's first ESP-NOW announce.

## 2. ESP-NOW control (everything after naming)

One fixed-size packet (`TlMsg` in `espnow_proto.h`, ~44 bytes) carries every
message. `magic = 0x7A`, `version = 1`. Commands flow controller → device
(unicast by MAC, or broadcast to `ff:ff:ff:ff:ff:ff` for the whole fleet);
reports/announces flow device → controller.

**Message types:**

| Type | Value | Direction | Meaning |
|---|---|---|---|
| `TL_CMD_SET_STATE` | 1 | ctrl → dev | apply the fields named by `fieldMask` |
| `TL_CMD_IDENTIFY` | 2 | ctrl → dev | ~2 s locate-me flash |
| `TL_CMD_QUERY` | 3 | ctrl → dev | reply with a `TL_MSG_REPORT` |
| `TL_MSG_REPORT` | 10 | dev → ctrl | full current state + identity |
| `TL_MSG_ANNOUNCE` | 11 | dev → ctrl | presence (cold boot / wake / rename) |

`fieldMask` bits (`TL_F_*`) select which of `mode/hue/sat/speed/intensity/
brightness` are meaningful — a `SET_STATE` is a **partial patch**, exactly like
the old REST `POST /api/state`. `mode` is an index into the effect list;
colour/speed fields are `0-255`. Set the `TL_FLAG_WANT_REPORT` flag to ask the
device to echo its resulting state.

Devices **announce on every wake**, so the controller's registry `lastSeenMs`
and last-known state cache refresh roughly every `IDLE_WAKE_INTERVAL_S`.

## 3. Two-tier sleep (device-side power policy)

Only **Solid** mode sleeps — the WS2812 latches its colour with no refresh.
Animated modes keep the MCU awake to push frames and never sleep.

- **Live window:** after any command or button press the device stays awake +
  listening for `LIVE_WINDOW_MS`, so repeated tweaks land instantly.
- **Idle:** it deep-sleeps, waking every `IDLE_WAKE_INTERVAL_S` to open a brief
  `RX_LISTEN_MS` listen window (and to announce). A command that arrives while
  it's asleep isn't seen until the next wake — up to `IDLE_WAKE_INTERVAL_S` of
  latency for an idle light. A button press always wakes immediately.

State survives the nap in RTC memory and is restored in `setup()`.

## Adding a new effect

1. `device/src/effects.h` — add one `Mode` enum entry (before `COUNT`).
2. `device/src/effects.cpp` — add one `fx*()` function, one `renderMode()`
   case, one `modeName()` string.
3. `controller/src/modes.h` — add the matching name in the same position.
   **This is the one list that must be kept in sync by hand** now that the
   device no longer serves `/api/modes` (it's asleep). Reuse
   `hue/sat/speed/intensity` for effect params so the generic sliders keep
   working without further controller changes.
