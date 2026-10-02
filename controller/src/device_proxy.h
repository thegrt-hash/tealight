#pragma once
#include <Arduino.h>

struct ProxyResult {
  bool    ok         = false; // reached the device and got an HTTP response
  int     httpStatus = 0;     // 0 if unreachable; 404 if fingerprint unknown; else the device's own status
  String  body;               // raw JSON body when ok
};

// Proxies one HTTP call to the tealight identified by `fingerprint`. Tries
// the registry's cached IP first; on failure, does an on-demand mDNS
// resolve by fingerprint and retries once before giving up — a stale IP
// never fails silently, it always surfaces as an explicit result.
ProxyResult deviceProxyRequest(const String& fingerprint, const char* method, const String& path, const String& jsonBody = "");
