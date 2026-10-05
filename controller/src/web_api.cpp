#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "web_api.h"
#include "config.h"
#include "registry.h"
#include "espnow_control.h"
#include "espnow_proto.h"
#include "modes.h"
#include "ble_provisioner.h"

static AsyncWebServer server(CONTROLLER_PORT);

// Periodically nudge the fleet to report, so the cache for awake/live devices
// stays fresh. Sleeping devices simply announce when they next wake.
static void queryTask(void*) {
  for (;;) {
    espnowBroadcastQuery();
    vTaskDelay(pdMS_TO_TICKS(DEVICE_QUERY_INTERVAL_MS));
  }
}

static void sendJson(AsyncWebServerRequest* req, JsonDocument& doc) {
  AsyncResponseStream* res = req->beginResponseStream("application/json");
  serializeJson(doc, *res);
  req->send(res);
}

// Build the TlFields + fieldMask from whatever subset the request supplied.
static uint8_t fieldsFromJson(JsonVariant& json, TlFields& f) {
  uint8_t mask = 0;
  if (!json["mode"].isNull())       { f.mode       = json["mode"]       | 0; mask |= TL_F_MODE; }
  else if (!json["modeName"].isNull()) {
    String wanted = json["modeName"].as<const char*>();
    for (int i = 0; i < TL_MODE_COUNT; i++) {
      if (wanted.equalsIgnoreCase(tlModeName(i))) { f.mode = i; mask |= TL_F_MODE; break; }
    }
  }
  if (!json["hue"].isNull())        { f.hue        = json["hue"]        | 0; mask |= TL_F_HUE; }
  if (!json["sat"].isNull())        { f.sat        = json["sat"]        | 0; mask |= TL_F_SAT; }
  if (!json["speed"].isNull())      { f.speed      = json["speed"]      | 0; mask |= TL_F_SPEED; }
  if (!json["intensity"].isNull())  { f.intensity  = json["intensity"]  | 0; mask |= TL_F_INTENSITY; }
  if (!json["brightness"].isNull()) { f.brightness = json["brightness"] | 0; mask |= TL_F_BRIGHTNESS; }
  return mask;
}

static void handleGetDevices(AsyncWebServerRequest* req) {
  uint32_t now = millis();
  JsonDocument doc;
  JsonArray    arr = doc.to<JsonArray>();
  for (auto& rec : registryGetAll()) {
    JsonObject o         = arr.add<JsonObject>();
    o["fingerprint"]     = rec.fingerprint;
    o["name"]            = rec.name;
    o["mac"]             = rec.macStr;
    o["lastSeenMs"]      = rec.lastSeenMs;
    o["provisionedAtMs"] = rec.provisionedAtMs;
    o["reachable"]       = rec.haveMac && (now - rec.lastSeenMs) < DEVICE_OFFLINE_MS;

    String stateJson;
    if (espnowCachedStateJson(rec.fingerprint, stateJson)) {
      JsonDocument stateDoc;
      if (!deserializeJson(stateDoc, stateJson)) o["state"] = stateDoc.as<JsonObject>();
    }
  }
  sendJson(req, doc);
}

static void handleGetDeviceModes(AsyncWebServerRequest* req) {
  JsonDocument doc;
  JsonArray    arr = doc.to<JsonArray>();
  for (int i = 0; i < TL_MODE_COUNT; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["index"] = i;
    o["name"]  = tlModeName(i);
  }
  sendJson(req, doc);
}

// Resolve "all" -> broadcast, or a fingerprint -> that device's MAC. Returns
// false (and sends an error) if the fingerprint is unknown or never heard from.
static bool resolveTarget(AsyncWebServerRequest* req, const String& fp, bool& broadcast, uint8_t macOut[6]) {
  if (fp.equalsIgnoreCase("all")) { broadcast = true; return true; }
  DeviceRecord rec;
  if (!registryGet(fp, rec)) {
    req->send(404, "application/json", "{\"error\":\"unknown device\"}");
    return false;
  }
  if (!rec.haveMac) {
    req->send(503, "application/json", "{\"error\":\"device not heard from yet\"}");
    return false;
  }
  broadcast = false;
  memcpy(macOut, rec.mac, 6);
  return true;
}

static void handlePostDeviceState(AsyncWebServerRequest* req, JsonVariant& json) {
  String fp = json["fingerprint"] | "";
  if (fp.length() == 0) {
    req->send(400, "application/json", "{\"error\":\"fingerprint required\"}");
    return;
  }
  TlFields f;
  uint8_t mask = fieldsFromJson(json, f);
  if (mask == 0) {
    req->send(400, "application/json", "{\"error\":\"no state fields\"}");
    return;
  }
  bool broadcast; uint8_t mac[6];
  if (!resolveTarget(req, fp, broadcast, mac)) return;
  if (broadcast) espnowBroadcastState(mask, f);
  else           espnowSendState(mac, mask, f);
  req->send(200, "application/json", "{\"ok\":true}");
}

static void handlePostDeviceIdentify(AsyncWebServerRequest* req, JsonVariant& json) {
  String fp = json["fingerprint"] | "";
  if (fp.length() == 0) {
    req->send(400, "application/json", "{\"error\":\"fingerprint required\"}");
    return;
  }
  bool broadcast; uint8_t mac[6];
  if (!resolveTarget(req, fp, broadcast, mac)) return;
  if (broadcast) espnowBroadcastIdentify();
  else           espnowSendIdentify(mac);
  req->send(200, "application/json", "{\"ok\":true}");
}

static void handlePostDeviceRename(AsyncWebServerRequest* req, JsonVariant& json) {
  String fp   = json["fingerprint"] | "";
  String name = json["name"] | "";
  if (fp.length() == 0 || name.length() == 0) {
    req->send(400, "application/json", "{\"error\":\"fingerprint and name required\"}");
    return;
  }
  registryRename(fp, name);
  req->send(200, "application/json", "{\"ok\":true}");
}

static void handlePostDeviceForget(AsyncWebServerRequest* req, JsonVariant& json) {
  String fp = json["fingerprint"] | "";
  if (fp.length() == 0) {
    req->send(400, "application/json", "{\"error\":\"fingerprint required\"}");
    return;
  }
  registryForget(fp);
  req->send(200, "application/json", "{\"ok\":true}");
}

// ---- BLE naming wizard ----

static const char* provisionStateToString(ProvisionState s) {
  switch (s) {
    case ProvisionState::IDLE:       return "idle";
    case ProvisionState::SCANNING:   return "scanning";
    case ProvisionState::CONNECTING: return "connecting";
    case ProvisionState::WAITING:    return "waiting";
    case ProvisionState::SUCCESS:    return "success";
    case ProvisionState::FAILED:     return "failed";
  }
  return "idle";
}

static String gProvisionName;
static bool   gProvisionRecorded = false;

static void handlePostDiscoveryStart(AsyncWebServerRequest* req) {
  bleDiscoveryStart();
  req->send(200, "application/json", "{\"ok\":true}");
}

static void handleGetDiscoveryFound(AsyncWebServerRequest* req) {
  JsonDocument doc;
  JsonArray    arr = doc.to<JsonArray>();
  for (auto& f : bleDiscoveryFound()) {
    JsonObject o  = arr.add<JsonObject>();
    o["address"]  = f.address;
    o["addrType"] = f.addrType;
    o["name"]     = f.name;
    o["rssi"]     = f.rssi;
  }
  sendJson(req, doc);
}

static void handlePostDiscoveryProvision(AsyncWebServerRequest* req, JsonVariant& json) {
  String  address  = json["address"] | "";
  uint8_t addrType = json["addrType"] | 0;
  String  name     = json["name"] | "";
  if (address.length() == 0) {
    req->send(400, "application/json", "{\"error\":\"address required\"}");
    return;
  }
  gProvisionName     = name;
  gProvisionRecorded = false;
  if (!bleProvisionStart(address, addrType, name)) {
    req->send(409, "application/json", "{\"error\":\"a scan or naming run is already active\"}");
    return;
  }
  req->send(200, "application/json", "{\"ok\":true}");
}

static void handleGetDiscoveryStatus(AsyncWebServerRequest* req) {
  ProvisionState st = bleProvisionerState();
  String         fp, msg;
  bleProvisionResult(fp, msg);

  if (st == ProvisionState::SUCCESS && !gProvisionRecorded && fp.length()) {
    registryUpsertProvisioned(fp, gProvisionName.length() ? gProvisionName : fp);
    gProvisionRecorded = true;
  }

  JsonDocument doc;
  doc["state"]       = provisionStateToString(st);
  doc["fingerprint"] = fp;
  doc["msg"]         = msg;
  sendJson(req, doc);
}

void webApiInit() {
  LittleFS.begin(true);

  xTaskCreatePinnedToCore(queryTask, "device_query", 4096, nullptr, 1, nullptr, 1);

  server.on("/api/devices", HTTP_GET, handleGetDevices);
  server.on("/api/device/modes", HTTP_GET, handleGetDeviceModes);
  server.on("/api/discovery/start", HTTP_POST, handlePostDiscoveryStart);
  server.on("/api/discovery/found", HTTP_GET, handleGetDiscoveryFound);
  server.on("/api/discovery/status", HTTP_GET, handleGetDiscoveryStatus);

  auto addJsonRoute = [](const char* path, ArJsonRequestHandlerFunction fn) {
    AsyncCallbackJsonWebHandler* h = new AsyncCallbackJsonWebHandler(path, fn);
    h->setMethod(HTTP_POST);
    server.addHandler(h);
  };
  addJsonRoute("/api/device/state", handlePostDeviceState);
  addJsonRoute("/api/device/identify", handlePostDeviceIdentify);
  addJsonRoute("/api/device/rename", handlePostDeviceRename);
  addJsonRoute("/api/device/forget", handlePostDeviceForget);
  addJsonRoute("/api/discovery/provision", handlePostDiscoveryProvision);

  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.begin();
}
