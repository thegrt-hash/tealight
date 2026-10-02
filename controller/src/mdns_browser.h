#pragma once
#include <Arduino.h>

// Browses _tealight._tcp every MDNS_BROWSE_INTERVAL_MS from a dedicated
// low-priority task (MDNS.queryService() blocks for ~3s, so it must never
// run on the async web server's task). Every result — new or known —
// updates the registry via registryNoteSeen(), which is how a device's IP
// change gets picked up without any user action.
void mdnsBrowserInit();

// On-demand resolve used by the device proxy when a cached IP goes stale.
// Runs one query pass (serialized with the periodic browse via the same
// mutex) and returns the IP for `fingerprint`, or "" if not currently seen.
String mdnsResolveByFingerprint(const String& fingerprint);
