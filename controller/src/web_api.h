#pragma once
#include <Arduino.h>

// Controller's own REST API, served to the browser (same-origin as the web
// UI in data/, so no CORS needed). Every device-facing route proxies
// through device_proxy.cpp by fingerprint — never by path-segment params,
// to avoid depending on this web server's optional regex-routing build.
//
//   GET  /api/devices                -> cached list of known devices + last-polled state
//   GET  /api/device/modes?fp=..     -> proxied GET  /api/modes
//   POST /api/device/state           -> body {"fingerprint", ...fields} -> proxied POST /api/state
//   POST /api/device/identify        -> body {"fingerprint"}            -> proxied POST /api/identify
//   POST /api/device/rename          -> body {"fingerprint","name"}     -> registry only
//   POST /api/device/forget          -> body {"fingerprint"}            -> registry only
//
//   POST /api/discovery/start        -> begin a BLE scan for unprovisioned tealights
//   GET  /api/discovery/found        -> devices found so far/after the scan
//   POST /api/discovery/provision    -> body {address,addrType,ssid,pass,name}; one at a time
//   GET  /api/discovery/status       -> progress of the in-flight provisioning run
//
// Also mounts the web UI itself (data/) as static files at "/".
void webApiInit();
