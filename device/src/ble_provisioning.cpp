#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "ble_provisioning.h"
#include "config.h"
#include "fingerprint.h"
#include "settings.h"

static NimBLECharacteristic* gStatusChar = nullptr;
static bool                  gAdvertising = false;
static volatile bool         gNamedEvent  = false;
static bool                  gLastHadName = false;

static void publishStatus(bool notify) {
  JsonDocument doc;
  doc["status"]      = settingsHasName() ? "named" : "unnamed";
  doc["fingerprint"] = getFingerprint();
  doc["name"]        = settingsName();
  String out;
  serializeJson(doc, out);
  if (!gStatusChar) return;
  gStatusChar->setValue(out);
  if (notify) gStatusChar->notify();
}

class NameCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& /*connInfo*/) override {
    JsonDocument doc;
    if (deserializeJson(doc, c->getValue().c_str())) return; // ignore malformed
    String name = doc["name"] | "";
    if (name.length() == 0) name = getDefaultName();
    settingsSetName(name);
    gNamedEvent = true;
    publishStatus(true);
  }
};
static NameCallbacks gNameCallbacks;

void bleInit() {
  String name = settingsName();
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

  NimBLECharacteristic* nameChar =
      svc->createCharacteristic(BLE_CHR_CREDS_UUID, NIMBLE_PROPERTY::WRITE);
  nameChar->setCallbacks(&gNameCallbacks);

  gStatusChar = svc->createCharacteristic(BLE_CHR_STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  gLastHadName = settingsHasName();
  publishStatus(false);

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

bool bleIsAdvertising() {
  return gAdvertising;
}

void bleService(uint32_t /*now*/) {
  bool hasName = settingsHasName();
  if (hasName != gLastHadName) {
    gLastHadName = hasName;
    publishStatus(true);
  }
}

bool bleConsumeNamedEvent() {
  if (!gNamedEvent) return false;
  gNamedEvent = false;
  return true;
}
