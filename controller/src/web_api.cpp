#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <map>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "web_api.h"
#include "config.h"
#include "registry.h"
#include "device_proxy.h"
#include "ble_provisioner.h"

static AsyncWebServer server(CONTROLLER_PORT);

// ---- Background device-state poller ----
// GET /api/devices serves this cache rather than proxying live on every
// call, so the browser's ~2s UI refresh never fans out N device requests.
struct CachedState {
  bool   valid = false;
  String json; // raw /api/state body, forwarded as-is
};
static std::map<String, CachedState> gStateCache;
static SemaphoreHandle_t             gCacheMutex;

static void pollTask(void*) {
  for (;;) {
    for (auto& rec : registryGetAll()) {
      ProxyResult r = deviceProxyRequest(rec.fingerprint, "GET", "/api/state");
      xSemaphoreTake(gCacheMutex, portMAX_DELAY);
      CachedState& cs = gStateCache[rec.fingerprint];
      cs.valid        = r.ok;
      if (r.ok) cs.json = r.body;
      xSemaphoreGive(gCacheMutex);
      vTaskDelay(pdMS_TO_TICKS(50)); // stagger requests instead of bursting the whole fleet at once
    }
    vTaskDelay(pdMS_TO_TICKS(DEVICE_POLL_INTERVAL_MS));
  }
}

static void sendJson(AsyncWebServerRequest* req, JsonDocument& doc) {
  AsyncResponseStream* res = req->beginResponseStream("application/json");
  serializeJson(doc, *res);
  req->send(res);
}

static void sendProxyResult(AsyncWebServerRequest* req, const ProxyResult& r) {
  if (r.httpStatus == 404) {
    req->send(404, "application/json", "{\"error\":\"unknown device\"}");
  } else if (!r.ok) {
    req->send(504, "application/json", "{\"error\":\"device unreachable\"}");
  } else {
    req->send(r.httpStatus, "application/json", r.body.length() ? r.body : "{}");
  }
}

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

// Latches so a polled /api/discovery/status doesn't re-write the registry
// (and re-persist to flash) on every single poll once a run succeeds.
static String gProvisionName;
static bool   gProvisionRecorded = false;

static void handleGetDevices(AsyncWebServerRequest* req) {
  JsonDocument doc;
  JsonArray    arr = doc.to<JsonArray>();
  for (auto& rec : registryGetAll()) {
    JsonObject o          = arr.add<JsonObject>();
    o["fingerprint"]      = rec.fingerprint;
    o["name"]             = rec.name;
    o["ip"]               = rec.lastIp;
    o["lastSeenMs"]       = rec.lastSeenMs;
    o["provisionedAtMs"]  = rec.provisionedAtMs;
    o["reachable"]        = rec.reachable;

    xSemaphoreTake(gCacheMutex, portMAX_DELAY);
    auto it           = gStateCache.find(rec.fingerprint);
    bool haveState    = it != gStateCache.end() && it->second.valid;
    String stateJson  = haveState ? it->second.json : "";
    xSemaphoreGive(gCacheMutex);

    if (haveState) {
      JsonDocument stateDoc;
      if (!deserializeJson(stateDoc, stateJson)) {
        o["state"] = stateDoc.as<JsonObject>();
      }
    }
  }
  sendJson(req, doc);
}

static void handleGetDeviceModes(AsyncWebServerRequest* req) {
  if (!req->hasParam("fp")) {
    req->send(400, "application/json", "{\"error\":\"fp required\"}");
    return;
  }
  String fp = req->getParam("fp")->value();
  sendProxyResult(req, deviceProxyRequest(fp, "GET", "/api/modes"));
}

static void handlePostDeviceState(AsyncWebServerRequest* req, JsonVariant& json) {
  String fp = json["fingerprint"] | "";
  if (fp.length() == 0) {
    req->send(400, "application/json", "{\"error\":\"fingerprint required\"}");
    return;
  }
  String body;
  serializeJson(json, body);
  sendProxyResult(req, deviceProxyRequest(fp, "POST", "/api/state", body));
}

static void handlePostDeviceIdentify(AsyncWebServerRequest* req, JsonVariant& json) {
  String fp = json["fingerprint"] | "";
  if (fp.length() == 0) {
    req->send(400, "application/json", "{\"error\":\"fingerprint required\"}");
    return;
  }
  sendProxyResult(req, deviceProxyRequest(fp, "POST", "/api/identify"));
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
  String  ssid     = json["ssid"] | "";
  String  pass     = json["pass"] | "";
  String  name     = json["name"] | "";
  if (address.length() == 0 || ssid.length() == 0) {
    req->send(400, "application/json", "{\"error\":\"address and ssid required\"}");
    return;
  }
  gProvisionName     = name;
  gProvisionRecorded = false;
  if (!bleProvisionStart(address, addrType, ssid, pass, name)) {
    req->send(409, "application/json", "{\"error\":\"a scan or provisioning run is already active\"}");
    return;
  }
  req->send(200, "application/json", "{\"ok\":true}");
}

static void handleGetDiscoveryStatus(AsyncWebServerRequest* req) {
  ProvisionState st = bleProvisionerState();
  String         fp, ip, msg;
  bleProvisionResult(fp, ip, msg);

  if (st == ProvisionState::SUCCESS && !gProvisionRecorded && fp.length()) {
    registryUpsertProvisioned(fp, gProvisionName.length() ? gProvisionName : fp, ip);
    gProvisionRecorded = true;
  }

  JsonDocument doc;
  doc["state"]       = provisionStateToString(st);
  doc["fingerprint"] = fp;
  doc["ip"]          = ip;
  doc["msg"]         = msg;
  sendJson(req, doc);
}

void webApiInit() {
  LittleFS.begin(true); // format on first boot if the filesystem isn't there yet

  gCacheMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(pollTask, "device_poll", 6144, nullptr, 1, nullptr, 1);

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
