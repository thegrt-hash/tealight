#include <LittleFS.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "registry.h"
#include "config.h"

static std::vector<DeviceRecord> gRecords;
static SemaphoreHandle_t         gMutex = nullptr;

struct Lock {
  Lock() { xSemaphoreTake(gMutex, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(gMutex); }
};

static DeviceRecord* findMutable(const String& fp) {
  for (auto& r : gRecords) {
    if (r.fingerprint == fp) return &r;
  }
  return nullptr;
}

// Assumes the caller already holds gMutex.
static void saveLocked() {
  JsonDocument doc;
  JsonArray    arr = doc.to<JsonArray>();
  for (auto& r : gRecords) {
    JsonObject o         = arr.add<JsonObject>();
    o["fingerprint"]     = r.fingerprint;
    o["name"]            = r.name;
    o["lastIp"]          = r.lastIp;
    o["lastSeenMs"]      = r.lastSeenMs;
    o["provisionedAtMs"] = r.provisionedAtMs;
  }
  File f = LittleFS.open(REGISTRY_PATH, "w");
  if (!f) return;
  serializeJson(doc, f);
  f.close();
}

void registryInit() {
  gMutex = xSemaphoreCreateMutex();
  gRecords.clear();

  if (!LittleFS.exists(REGISTRY_PATH)) return;
  File f = LittleFS.open(REGISTRY_PATH, "r");
  if (!f) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return;

  for (JsonObject o : doc.as<JsonArray>()) {
    DeviceRecord r;
    r.fingerprint      = o["fingerprint"] | "";
    r.name             = o["name"] | "";
    r.lastIp           = o["lastIp"] | "";
    r.lastSeenMs       = o["lastSeenMs"] | 0;
    r.provisionedAtMs  = o["provisionedAtMs"] | 0;
    r.reachable        = false; // unconfirmed until the next mDNS/poll cycle
    if (r.fingerprint.length()) gRecords.push_back(r);
  }
}

std::vector<DeviceRecord> registryGetAll() {
  Lock lock;
  return gRecords;
}

bool registryGet(const String& fingerprint, DeviceRecord& out) {
  Lock lock;
  DeviceRecord* r = findMutable(fingerprint);
  if (!r) return false;
  out = *r;
  return true;
}

void registryNoteSeen(const String& fingerprint, const String& ip, const String& defaultName) {
  Lock lock;
  DeviceRecord* r = findMutable(fingerprint);
  if (r) {
    if (ip.length()) r->lastIp = ip;
    r->lastSeenMs = millis();
    r->reachable  = true;
    return;
  }
  DeviceRecord nr;
  nr.fingerprint = fingerprint;
  nr.name        = defaultName;
  nr.lastIp      = ip;
  nr.lastSeenMs  = millis();
  nr.reachable   = true;
  gRecords.push_back(nr);
  saveLocked();
}

void registryUpsertProvisioned(const String& fingerprint, const String& name, const String& ip) {
  Lock lock;
  DeviceRecord* r = findMutable(fingerprint);
  if (!r) {
    gRecords.push_back(DeviceRecord{});
    r               = &gRecords.back();
    r->fingerprint  = fingerprint;
  }
  r->name            = name;
  r->lastIp          = ip;
  r->lastSeenMs      = millis();
  r->provisionedAtMs = millis();
  r->reachable       = true;
  saveLocked();
}

void registryRename(const String& fingerprint, const String& name) {
  Lock lock;
  DeviceRecord* r = findMutable(fingerprint);
  if (!r) return;
  r->name = name;
  saveLocked();
}

void registryMarkUnreachable(const String& fingerprint) {
  Lock lock;
  DeviceRecord* r = findMutable(fingerprint);
  if (r) r->reachable = false;
}

void registryForget(const String& fingerprint) {
  Lock lock;
  for (size_t i = 0; i < gRecords.size(); i++) {
    if (gRecords[i].fingerprint == fingerprint) {
      gRecords.erase(gRecords.begin() + i);
      break;
    }
  }
  saveLocked();
}
