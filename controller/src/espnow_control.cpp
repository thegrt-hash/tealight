#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <map>
#include <string.h>
#include "espnow_control.h"
#include "espnow_proto.h"
#include "config.h"
#include "registry.h"
#include "modes.h"

static const uint8_t kBroadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t       gSeq = 0;

// Last-known state per fingerprint, filled from device reports/announces.
struct Cached {
  TlFields f;
  bool     valid = false;
};
static std::map<String, Cached> gCache;
static SemaphoreHandle_t        gCacheMutex = nullptr;

static void ensurePeer(const uint8_t mac[6]) {
  if (esp_now_is_peer_exist(mac)) return;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = FLEET_CHANNEL;
  peer.ifidx   = WIFI_IF_STA;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

static void sendCmd(const uint8_t mac[6], uint8_t type, uint8_t fieldMask, const TlFields& f) {
  ensurePeer(mac);
  TlMsg m;
  memset(&m, 0, sizeof(m));
  m.magic      = TL_MAGIC;
  m.version    = TL_PROTO_VER;
  m.type       = type;
  m.flags      = TL_FLAG_WANT_REPORT;
  m.seq        = ++gSeq;
  m.fieldMask  = fieldMask;
  m.mode       = f.mode;
  m.hue        = f.hue;
  m.sat        = f.sat;
  m.speed      = f.speed;
  m.intensity  = f.intensity;
  m.brightness = f.brightness;
  esp_now_send(mac, (uint8_t*)&m, sizeof(m));
}

// ESP-NOW recv callback (WiFi task) — keep it short.
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
static void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  const uint8_t* src = info->src_addr;
#else
static void onRecv(const uint8_t* src, const uint8_t* data, int len) {
#endif
  if (len < (int)sizeof(TlMsg)) return;
  TlMsg m;
  memcpy(&m, data, sizeof(m));
  if (m.magic != TL_MAGIC || m.version != TL_PROTO_VER) return;
  if (m.type != TL_MSG_REPORT && m.type != TL_MSG_ANNOUNCE) return;

  m.fingerprint[sizeof(m.fingerprint) - 1] = '\0';
  m.name[sizeof(m.name) - 1]               = '\0';
  String fp = String(m.fingerprint);
  if (fp.length() == 0) return;

  String defName = strlen(m.name) ? String(m.name) : ("Tealight-" + fp.substring(fp.length() - 4));
  registryNoteSeen(fp, src, defName);

  xSemaphoreTake(gCacheMutex, portMAX_DELAY);
  Cached& c = gCache[fp];
  c.valid       = true;
  c.f.mode      = m.mode;
  c.f.hue       = m.hue;
  c.f.sat       = m.sat;
  c.f.speed     = m.speed;
  c.f.intensity = m.intensity;
  c.f.brightness = m.brightness;
  xSemaphoreGive(gCacheMutex);
}

void espnowControlInit() {
  gCacheMutex = xSemaphoreCreateMutex();

  uint8_t ch = WiFi.channel();
  if (ch != FLEET_CHANNEL) {
    Serial.printf("[espnow] WARNING: WiFi channel is %u but FLEET_CHANNEL is %u — "
                  "lock your router's 2.4GHz radio to channel %u or the fleet won't hear commands\n",
                  ch, FLEET_CHANNEL, FLEET_CHANNEL);
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("[espnow] init failed");
    return;
  }
  esp_now_register_recv_cb(onRecv);
  ensurePeer(kBroadcast);
  Serial.printf("[espnow] control up (WiFi channel %u)\n", ch);
}

void espnowSendState(const uint8_t mac[6], uint8_t fieldMask, const TlFields& f) {
  sendCmd(mac, TL_CMD_SET_STATE, fieldMask, f);
}
void espnowBroadcastState(uint8_t fieldMask, const TlFields& f) {
  sendCmd(kBroadcast, TL_CMD_SET_STATE, fieldMask, f);
}
void espnowSendIdentify(const uint8_t mac[6]) {
  TlFields f;
  sendCmd(mac, TL_CMD_IDENTIFY, 0, f);
}
void espnowBroadcastIdentify() {
  TlFields f;
  sendCmd(kBroadcast, TL_CMD_IDENTIFY, 0, f);
}
void espnowBroadcastQuery() {
  TlFields f;
  sendCmd(kBroadcast, TL_CMD_QUERY, 0, f);
}

bool espnowCachedStateJson(const String& fingerprint, String& out) {
  xSemaphoreTake(gCacheMutex, portMAX_DELAY);
  auto it = gCache.find(fingerprint);
  bool ok = (it != gCache.end() && it->second.valid);
  TlFields f;
  if (ok) f = it->second.f;
  xSemaphoreGive(gCacheMutex);
  if (!ok) return false;

  JsonDocument doc;
  doc["mode"]       = f.mode;
  doc["modeName"]   = tlModeName(f.mode);
  doc["hue"]        = f.hue;
  doc["sat"]        = f.sat;
  doc["speed"]      = f.speed;
  doc["intensity"]  = f.intensity;
  doc["brightness"] = f.brightness;
  serializeJson(doc, out);
  return true;
}
