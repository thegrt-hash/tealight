#include <WebServer.h>
#include <ArduinoJson.h>
#include "web_api.h"
#include "state.h"
#include "effects.h"
#include "fingerprint.h"
#include "wifi_manager.h"
#include "config.h"

static WebServer server(API_PORT);

static void sendJson(JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleGetState() {
  DeviceState s = stateGet();
  JsonDocument doc;
  doc["mode"]       = static_cast<uint8_t>(s.mode);
  doc["modeName"]   = modeName(s.mode);
  doc["hue"]        = s.params.hue;
  doc["sat"]        = s.params.sat;
  doc["speed"]      = s.params.speed;
  doc["intensity"]  = s.params.intensity;
  doc["brightness"] = s.brightness;
  sendJson(doc);
}

static void handlePostState() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"missing body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"error\":\"invalid json\"}");
    return;
  }

  DeviceState s = stateGet();

  if (!doc["modeName"].isNull()) {
    String wanted = doc["modeName"].as<const char*>();
    for (uint8_t i = 0; i < static_cast<uint8_t>(Mode::COUNT); i++) {
      if (wanted.equalsIgnoreCase(modeName(static_cast<Mode>(i)))) {
        s.mode = static_cast<Mode>(i);
        break;
      }
    }
  } else if (!doc["mode"].isNull()) {
    uint8_t idx = doc["mode"] | 255;
    if (idx < static_cast<uint8_t>(Mode::COUNT)) s.mode = static_cast<Mode>(idx);
  }

  s.params.hue       = doc["hue"] | s.params.hue;
  s.params.sat       = doc["sat"] | s.params.sat;
  s.params.speed     = doc["speed"] | s.params.speed;
  s.params.intensity = doc["intensity"] | s.params.intensity;
  s.brightness       = doc["brightness"] | s.brightness;

  stateSet(s);
  handleGetState(); // respond with the resulting full state
}

static void handleGetModes() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (uint8_t i = 0; i < static_cast<uint8_t>(Mode::COUNT); i++) {
    JsonObject o = arr.add<JsonObject>();
    o["index"] = i;
    o["name"]  = modeName(static_cast<Mode>(i));
  }
  sendJson(doc);
}

static void handleGetInfo() {
  JsonDocument doc;
  doc["fingerprint"] = getFingerprint();
  doc["name"]        = wifiGetName();
  doc["fw"]          = FW_VERSION;
  doc["uptimeMs"]    = millis();
  doc["ip"]          = wifiGetIp();
  sendJson(doc);
}

static void handlePostIdentify() {
  identifyTrigger(millis());
  server.send(200, "application/json", "{\"ok\":true}");
}

void webApiInit() {
  server.on("/api/state", HTTP_GET, handleGetState);
  server.on("/api/state", HTTP_POST, handlePostState);
  server.on("/api/modes", HTTP_GET, handleGetModes);
  server.on("/api/info", HTTP_GET, handleGetInfo);
  server.on("/api/identify", HTTP_POST, handlePostIdentify);
  server.begin();
}

void webApiService() {
  server.handleClient();
}
