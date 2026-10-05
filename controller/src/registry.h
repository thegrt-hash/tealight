#pragma once
#include <Arduino.h>
#include <vector>

// What the controller remembers about one tealight. `fingerprint` (the device's
// efuse-MAC-derived id) is the stable key; `mac` is its ESP-NOW address, learned
// from the device's own reports/announces (not known at BLE-naming time).
struct DeviceRecord {
  String   fingerprint;
  String   name;
  String   macStr;                 // "aa:bb:cc:dd:ee:ff", "" until first heard
  uint8_t  mac[6]              = {0};
  bool     haveMac            = false;
  uint32_t lastSeenMs         = 0;  // millis() of last ESP-NOW message
  uint32_t provisionedAtMs    = 0;
};

void registryInit(); // loads /registry.json from LittleFS

std::vector<DeviceRecord> registryGetAll();
bool                      registryGet(const String& fingerprint, DeviceRecord& out);

// Called from the ESP-NOW recv path on every report/announce. Creates a record
// on first sight (persisted); otherwise refreshes mac/lastSeen in memory and
// persists only when the MAC first becomes known (so we don't wear the flash).
void registryNoteSeen(const String& fingerprint, const uint8_t mac[6], const String& defaultName);

// Called right after a successful BLE naming pass (MAC still unknown here).
void registryUpsertProvisioned(const String& fingerprint, const String& name);

void registryRename(const String& fingerprint, const String& name); // persisted
void registryForget(const String& fingerprint);                     // persisted
