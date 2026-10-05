#include <LittleFS.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>
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

static String macToStr(const uint8_t mac[6]) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

static bool strToMac(const String& s, uint8_t mac[6]) {
  int v[6];
  if (sscanf(s.c_str(), "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6)
    return false;
  for (int i = 0; i < 6; i++) mac[i] = (uint8_t)v[i];
  return true;
}

// Assumes the caller already holds gMutex.
static void saveLocked() {
  JsonDocument doc;
  JsonArray    arr = doc.to<JsonArray>();
  for (auto& r : gRecords) {
    JsonObject o         = arr.add<JsonObject>();
    o["fingerprint"]     = r.fingerprint;
    o["name"]            = r.name;
    o["mac"]             = r.macStr;
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
    r.fingerprint     = o["fingerprint"] | "";
    r.name            = o["name"] | "";
    r.macStr          = o["mac"] | "";
    r.provisionedAtMs = o["provisionedAtMs"] | 0;
    if (r.macStr.length()) r.haveMac = strToMac(r.macStr, r.mac);
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

void registryNoteSeen(const String& fingerprint, const uint8_t mac[6], const String& defaultName) {
  Lock lock;
  DeviceRecord* r = findMutable(fingerprint);
  if (r) {
    bool wasKnown = r->haveMac;
    memcpy(r->mac, mac, 6);
    r->macStr     = macToStr(mac);
    r->haveMac    = true;
    r->lastSeenMs = millis();
    if (!wasKnown) saveLocked(); // persist the MAC the first time we learn it
    return;
  }
  DeviceRecord nr;
  nr.fingerprint = fingerprint;
  nr.name        = defaultName;
  memcpy(nr.mac, mac, 6);
  nr.macStr      = macToStr(mac);
  nr.haveMac     = true;
  nr.lastSeenMs  = millis();
  gRecords.push_back(nr);
  saveLocked();
}

void registryUpsertProvisioned(const String& fingerprint, const String& name) {
  Lock lock;
  DeviceRecord* r = findMutable(fingerprint);
  if (!r) {
    gRecords.push_back(DeviceRecord{});
    r              = &gRecords.back();
    r->fingerprint = fingerprint;
  }
  r->name            = name;
  r->provisionedAtMs = millis();
  saveLocked();
}

void registryRename(const String& fingerprint, const String& name) {
  Lock lock;
  DeviceRecord* r = findMutable(fingerprint);
  if (!r) return;
  r->name = name;
  saveLocked();
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
