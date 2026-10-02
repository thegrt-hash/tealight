#pragma once
#include <Arduino.h>
#include <vector>

// What the controller remembers about one tealight, independent of its
// current IP. `fingerprint` (the device's efuse-MAC-derived id) is the only
// stable key — `lastIp` is a cache, refreshed by the mDNS browser and by
// on-demand resolves from the device proxy.
struct DeviceRecord {
  String   fingerprint;
  String   name;
  String   lastIp;
  uint32_t lastSeenMs       = 0;
  uint32_t provisionedAtMs  = 0;
  bool     reachable        = false;
};

void registryInit(); // loads /registry.json from LittleFS (creates none if absent)

std::vector<DeviceRecord> registryGetAll();
bool                      registryGet(const String& fingerprint, DeviceRecord& out);

// Called by the mDNS browser (every browse cycle) and the device proxy
// (after an on-demand re-resolve). Creates a new record on first sight
// (persisted immediately); otherwise only refreshes the in-memory
// lastIp/lastSeenMs/reachable — not persisted every time, to avoid wearing
// the flash with a write every ~15s per device for the life of the fleet.
void registryNoteSeen(const String& fingerprint, const String& ip, const String& defaultName);

// Called right after a successful BLE provisioning pass. Always persisted.
void registryUpsertProvisioned(const String& fingerprint, const String& name, const String& ip);

void registryRename(const String& fingerprint, const String& name);        // persisted
void registryMarkUnreachable(const String& fingerprint);                   // in-memory only
void registryForget(const String& fingerprint);                            // persisted
