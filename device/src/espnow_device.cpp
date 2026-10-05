#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>
#include "espnow_device.h"
#include "espnow_proto.h"
#include "config.h"
#include "state.h"
#include "settings.h"
#include "fingerprint.h"
#include "power.h"

static const uint8_t kBroadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Learned from the first command we hear, so reports can go straight back.
static uint8_t  gControllerMac[6] = {0};
static bool     gHaveController   = false;

// Outbound work, set from the recv callback and flushed in espnowService().
static volatile bool gPendingAnnounce = false;
static volatile bool gPendingReport   = false;
static volatile uint8_t gReportSeq     = 0;

static void addPeer(const uint8_t mac[6]) {
  if (esp_now_is_peer_exist(mac)) return;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = FLEET_CHANNEL;
  peer.ifidx   = WIFI_IF_STA;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

static void fillIdentity(TlMsg& m) {
  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  memcpy(m.mac, mac, 6);
  strncpy(m.fingerprint, getFingerprint().c_str(), sizeof(m.fingerprint) - 1);
  strncpy(m.name, settingsName().c_str(), sizeof(m.name) - 1);
}

static void buildReport(TlMsg& m, uint8_t seq) {
  memset(&m, 0, sizeof(m));
  m.magic   = TL_MAGIC;
  m.version = TL_PROTO_VER;
  m.type    = TL_MSG_REPORT;
  m.seq     = seq;
  m.fieldMask = TL_F_ALL;
  DeviceState s = stateGet();
  m.mode       = static_cast<uint8_t>(s.mode);
  m.hue        = s.params.hue;
  m.sat        = s.params.sat;
  m.speed      = s.params.speed;
  m.intensity  = s.params.intensity;
  m.brightness = s.brightness;
  fillIdentity(m);
}

static void applySetState(const TlMsg& m) {
  DeviceState s = stateGet();
  if ((m.fieldMask & TL_F_MODE) && m.mode < static_cast<uint8_t>(Mode::COUNT))
    s.mode = static_cast<Mode>(m.mode);
  if (m.fieldMask & TL_F_HUE)        s.params.hue       = m.hue;
  if (m.fieldMask & TL_F_SAT)        s.params.sat       = m.sat;
  if (m.fieldMask & TL_F_SPEED)      s.params.speed     = m.speed;
  if (m.fieldMask & TL_F_INTENSITY)  s.params.intensity = m.intensity;
  if (m.fieldMask & TL_F_BRIGHTNESS) s.brightness       = m.brightness;
  stateSet(s);
}

// ESP-NOW recv callback. Runs on the WiFi task — keep it short, no esp_now_send
// here; just mutate state and flag the outbound work for espnowService().
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

  memcpy(gControllerMac, src, 6);
  gHaveController = true;

  switch (m.type) {
    case TL_CMD_SET_STATE:
      applySetState(m);
      powerNoteActivity();                       // re-open the full live window
      if (m.flags & TL_FLAG_WANT_REPORT) { gReportSeq = m.seq; gPendingReport = true; }
      break;
    case TL_CMD_IDENTIFY:
      identifyTrigger(millis());
      powerNoteActivity();
      break;
    case TL_CMD_QUERY:
      gReportSeq = m.seq;
      gPendingReport = true;
      break;
    default:
      break;                                     // ignore device->controller types
  }
}

void espnowInit() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);                  // ensure we never associate
  esp_wifi_set_channel(FLEET_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[espnow] init failed");
    return;
  }
  esp_now_register_recv_cb(onRecv);
  addPeer(kBroadcast);
  Serial.printf("[espnow] up on channel %u\n", FLEET_CHANNEL);
}

void espnowAnnounce() {
  gPendingAnnounce = true;
}

void espnowService(uint32_t /*now*/) {
  if (gPendingReport) {
    gPendingReport = false;
    if (gHaveController) {
      addPeer(gControllerMac);
      TlMsg m; buildReport(m, gReportSeq);
      esp_now_send(gControllerMac, (uint8_t*)&m, sizeof(m));
    }
  }
  if (gPendingAnnounce) {
    gPendingAnnounce = false;
    TlMsg m; buildReport(m, 0);
    m.type = TL_MSG_ANNOUNCE;
    esp_now_send(kBroadcast, (uint8_t*)&m, sizeof(m));  // broadcast peer added at init
  }
}
