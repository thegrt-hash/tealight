#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "ble_provisioning.h"
#include "config.h"
#include "fingerprint.h"
#include "wifi_manager.h"

static NimBLECharacteristic* gStatusChar = nullptr;
static bool                  gAdvertising = false;
// Out-of-range sentinel so the first bleService() tick always publishes.
static WifiState gLastReported = static_cast<WifiState>(0xFF);

static const char* statusToString(WifiState s) {
  switch (s) {
    case WifiState::IDLE:       return "idle";
    case WifiState::CONNECTING: return "connecting";
    case WifiState::CONNECTED:  return "connected";
    case WifiState::FAILED:     return "failed";
  }
  return "idle";
}

static void publishStatus(bool notify) {
  JsonDocument doc;
  doc["status"]      = statusToString(wifiGetState());
  doc["fingerprint"] = getFingerprint();
  doc["ip"]          = wifiGetIp();
  doc["msg"]         = "";
  String out;
  serializeJson(doc, out);
  gStatusChar->setValue(out);
  if (notify) gStatusChar->notify();
}

class CredsCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& connInfo) override {
    JsonDocument doc;
    if (deserializeJson(doc, c->getValue().c_str())) {
      return; // ignore malformed writes
    }
    String ssid = doc["ssid"] | "";
    String pass = doc["pass"] | "";
    String name = doc["name"] | "";
    if (ssid.length() == 0) return;
    if (name.length() == 0) name = getDefaultName();

    wifiSetCredentials(ssid, pass, name);
    publishStatus(true);
  }
};
static CredsCallbacks gCredsCallbacks;

void bleInit() {
  String name = getDefaultName();
  NimBLEDevice::init(name.c_str());

  NimBLEServer*  server = NimBLEDevice::createServer();
  NimBLEService* svc    = server->createService(BLE_SVC_UUID);

  NimBLECharacteristic* infoChar = svc->createCharacteristic(BLE_CHR_INFO_UUID, NIMBLE_PROPERTY::READ);
  JsonDocument          infoDoc;
  infoDoc["fingerprint"] = getFingerprint();
  infoDoc["name"]        = name;
  infoDoc["fw"]          = FW_VERSION;
  String infoOut;
  serializeJson(infoDoc, infoOut);
  infoChar->setValue(infoOut);

  NimBLECharacteristic* credsChar =
      svc->createCharacteristic(BLE_CHR_CREDS_UUID, NIMBLE_PROPERTY::WRITE);
  credsChar->setCallbacks(&gCredsCallbacks);

  gStatusChar = svc->createCharacteristic(BLE_CHR_STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  publishStatus(false);

  // NimBLEService::start() is a deprecated no-op in 2.x — the GATT server
  // starts itself the first time NimBLEAdvertising::start() runs.
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setName(name.c_str());
  adv->addServiceUUID(svc->getUUID());
}

void bleStartAdvertising() {
  if (gAdvertising) return;
  NimBLEDevice::getAdvertising()->start();
  gAdvertising = true;
}

void bleStopAdvertising() {
  if (!gAdvertising) return;
  NimBLEDevice::getAdvertising()->stop();
  gAdvertising = false;
}

void bleService(uint32_t now) {
  (void)now;
  WifiState cur = wifiGetState();
  if (cur != gLastReported) {
    gLastReported = cur;
    publishStatus(true);
  }
}
