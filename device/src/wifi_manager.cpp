#include <WiFi.h>
#include <Preferences.h>
#include "wifi_manager.h"
#include "fingerprint.h"
#include "config.h"

static Preferences gPrefs;
static String      gSsid, gPass, gName;
static WifiState   gState        = WifiState::IDLE;
static uint8_t      gAttempts     = 0;
static uint32_t     gAttemptStart = 0;

static void beginConnect(uint32_t now) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(gSsid.c_str(), gPass.c_str());
  gState       = WifiState::CONNECTING;
  gAttemptStart = now;
}

void wifiInit() {
  gPrefs.begin("tealight", false);
  gSsid = gPrefs.getString("ssid", "");
  gPass = gPrefs.getString("pass", "");
  gName = gPrefs.getString("name", "");

  if (gSsid.length() > 0) {
    gAttempts = 0;
    beginConnect(millis());
  } else {
    gState = WifiState::IDLE;
  }
}

void wifiService(uint32_t now) {
  switch (gState) {
    case WifiState::CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        gState = WifiState::CONNECTED;
      } else if (now - gAttemptStart >= WIFI_CONNECT_TIMEOUT_MS) {
        gAttempts++;
        if (gAttempts >= WIFI_MAX_BOOT_RETRIES) {
          gState = WifiState::FAILED;
        } else {
          beginConnect(now);
        }
      }
      break;

    case WifiState::CONNECTED:
      // Transient runtime drop: keep retrying indefinitely, don't count
      // against the boot-retry budget and don't fall back to BLE.
      if (WiFi.status() != WL_CONNECTED) {
        gAttempts = 0;
        beginConnect(now);
      }
      break;

    case WifiState::IDLE:
    case WifiState::FAILED:
      // Waits for BLE provisioning to call wifiSetCredentials().
      break;
  }
}

bool wifiHasCredentials() {
  return gSsid.length() > 0;
}

WifiState wifiGetState() {
  return gState;
}

String wifiGetIp() {
  return gState == WifiState::CONNECTED ? WiFi.localIP().toString() : String("");
}

String wifiGetName() {
  return gName.length() > 0 ? gName : getDefaultName();
}

void wifiSetCredentials(const String& ssid, const String& pass, const String& name) {
  gSsid = ssid;
  gPass = pass;
  gName = name;

  gPrefs.putString("ssid", gSsid);
  gPrefs.putString("pass", gPass);
  gPrefs.putString("name", gName);

  gAttempts = 0;
  beginConnect(millis());
}

void wifiClearCredentials() {
  gPrefs.clear();
  gSsid = "";
  gPass = "";
  gName = "";
  WiFi.disconnect(true);
  gState = WifiState::IDLE;
}
