#pragma once
#include <stdint.h>

// ============================================================================
// Tealight ESP-NOW control protocol — SHARED between device/ and controller/.
//
// These are two separate PlatformIO projects with no common include path, so
// this file is DUPLICATED VERBATIM in device/src/ and controller/src/. If you
// edit one, edit the other too — there is no negotiation beyond the version
// byte below, so a mismatch just silently breaks the fleet.
//
// One fixed-size packet carries every message (~44 bytes, well under the 250 B
// ESP-NOW payload cap). Commands flow controller -> device; reports/announces
// flow device -> controller.
// ============================================================================

constexpr uint8_t TL_MAGIC     = 0x7A; // first byte of every packet
constexpr uint8_t TL_PROTO_VER = 1;

// Message types. 1-9 = controller -> device; 10+ = device -> controller.
enum : uint8_t {
  TL_CMD_SET_STATE = 1,   // apply the state fields named by fieldMask
  TL_CMD_IDENTIFY  = 2,   // locate-me flash
  TL_CMD_QUERY     = 3,   // "report your state now" -> device replies TL_MSG_REPORT
  TL_MSG_REPORT    = 10,  // device's current full state (reply to QUERY / SET_STATE)
  TL_MSG_ANNOUNCE  = 11,  // device announces presence (cold boot / wake / rename)
};

// Field-presence bits for TL_CMD_SET_STATE; a TL_MSG_REPORT sets them all.
enum : uint8_t {
  TL_F_MODE       = 1 << 0,
  TL_F_HUE        = 1 << 1,
  TL_F_SAT        = 1 << 2,
  TL_F_SPEED      = 1 << 3,
  TL_F_INTENSITY  = 1 << 4,
  TL_F_BRIGHTNESS = 1 << 5,
  TL_F_ALL        = 0x3F,
};

// flags bits
enum : uint8_t {
  TL_FLAG_WANT_REPORT = 1 << 0, // device should reply with a TL_MSG_REPORT
};

struct __attribute__((packed)) TlMsg {
  uint8_t  magic;           // TL_MAGIC
  uint8_t  version;         // TL_PROTO_VER
  uint8_t  type;            // TL_CMD_* / TL_MSG_*
  uint8_t  flags;           // TL_FLAG_*
  uint8_t  seq;             // sender-chosen; echoed in reports for matching
  uint8_t  fieldMask;       // TL_F_* — which of the fields below are meaningful
  uint8_t  mode;            // 0..mode-count-1
  uint8_t  hue;
  uint8_t  sat;
  uint8_t  speed;
  uint8_t  intensity;
  uint8_t  brightness;
  uint8_t  mac[6];          // device STA MAC (identity) in REPORT/ANNOUNCE; 0 in cmds
  char     fingerprint[13]; // 12 hex chars + NUL, in REPORT/ANNOUNCE
  char     name[21];        // friendly name in REPORT/ANNOUNCE; "" in cmds
};
