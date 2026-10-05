#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "ble_provisioner.h"
#include "config.h"

static SemaphoreHandle_t gMutex = nullptr;
struct Lock {
  Lock() { xSemaphoreTake(gMutex, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(gMutex); }
};

static ProvisionState gState = ProvisionState::IDLE;
static std::vector<FoundDevice> gFound;

static bool gWantScan      = false;
static bool gWantProvision = false;
struct PendingProvision {
  String  address;
  uint8_t addrType;
  String  name;
};
static PendingProvision gPending;

static String gResultFp, gResultMsg;

static void fail(const char* msg) {
  Lock lock;
  gState     = ProvisionState::FAILED;
  gResultMsg = msg;
}

static void provisionOne(const PendingProvision& req) {
  { Lock lock; gState = ProvisionState::CONNECTING; gResultFp = ""; gResultMsg = ""; }

  NimBLEAddress addr(std::string(req.address.c_str()), req.addrType);
  NimBLEClient* client = NimBLEDevice::createClient();
  client->setConnectTimeout(8000);

  if (!client->connect(addr)) {
    NimBLEDevice::deleteClient(client);
    fail("BLE connect failed");
    return;
  }

  NimBLERemoteService* svc = client->getService(BLE_SVC_UUID);
  if (!svc) {
    client->disconnect();
    NimBLEDevice::deleteClient(client);
    fail("tealight service not found");
    return;
  }

  // Read the authoritative fingerprint before naming.
  String fingerprint, infoName;
  NimBLERemoteCharacteristic* infoChr = svc->getCharacteristic(BLE_CHR_INFO_UUID);
  if (infoChr && infoChr->canRead()) {
    JsonDocument doc;
    if (!deserializeJson(doc, infoChr->readValue().c_str())) {
      fingerprint = String((const char*)(doc["fingerprint"] | ""));
      infoName    = String((const char*)(doc["name"] | ""));
    }
  }
  if (fingerprint.length() == 0) {
    client->disconnect();
    NimBLEDevice::deleteClient(client);
    fail("could not read device fingerprint");
    return;
  }

  NimBLERemoteCharacteristic* nameChr = svc->getCharacteristic(BLE_CHR_CREDS_UUID);
  if (!nameChr || !nameChr->canWrite()) {
    client->disconnect();
    NimBLEDevice::deleteClient(client);
    fail("tealight name characteristic missing");
    return;
  }

  { Lock lock; gState = ProvisionState::WAITING; }

  JsonDocument nameDoc;
  nameDoc["name"] = req.name.length() ? req.name : infoName;
  String nameOut;
  serializeJson(nameDoc, nameOut);

  if (!nameChr->writeValue(nameOut.c_str(), nameOut.length(), true)) {
    client->disconnect();
    NimBLEDevice::deleteClient(client);
    fail("failed to write device name");
    return;
  }

  client->disconnect();
  NimBLEDevice::deleteClient(client);

  Lock lock;
  gState    = ProvisionState::SUCCESS;
  gResultFp = fingerprint;
  gResultMsg = "";
}

static void bleTask(void*) {
  for (;;) {
    bool doScan = false, doProvision = false;
    PendingProvision req;
    {
      Lock lock;
      if (gWantScan) {
        doScan    = true;
        gWantScan = false;
      } else if (gWantProvision) {
        doProvision    = true;
        gWantProvision = false;
        req            = gPending;
      }
    }

    if (doScan) {
      { Lock lock; gState = ProvisionState::SCANNING; }
      NimBLEDevice::getScan()->setActiveScan(true);
      NimBLEScanResults results = NimBLEDevice::getScan()->getResults(BLE_SCAN_MS, false);

      std::vector<FoundDevice> found;
      for (int i = 0; i < results.getCount(); i++) {
        const NimBLEAdvertisedDevice* dev = results.getDevice(i);
        if (!dev->isAdvertisingService(NimBLEUUID(BLE_SVC_UUID))) continue;
        FoundDevice f;
        f.address  = String(dev->getAddress().toString().c_str());
        f.addrType = dev->getAddress().getType();
        f.name     = String(dev->getName().c_str());
        f.rssi     = dev->getRSSI();
        found.push_back(f);
      }

      Lock lock;
      gFound = found;
      gState = ProvisionState::IDLE;
    } else if (doProvision) {
      provisionOne(req);
    } else {
      vTaskDelay(pdMS_TO_TICKS(150));
    }
  }
}

void bleProvisionerInit() {
  gMutex = xSemaphoreCreateMutex();
  NimBLEDevice::init("TealightController");
  xTaskCreatePinnedToCore(bleTask, "ble_provisioner", 6144, nullptr, 1, nullptr, 0);
}

static bool isBusyLocked() {
  return gState == ProvisionState::SCANNING || gState == ProvisionState::CONNECTING ||
         gState == ProvisionState::WAITING;
}

void bleDiscoveryStart() {
  Lock lock;
  if (!isBusyLocked()) {
    gFound.clear();
    gWantScan = true;
  }
}

std::vector<FoundDevice> bleDiscoveryFound() {
  Lock lock;
  return gFound;
}

bool bleProvisionStart(const String& address, uint8_t addrType, const String& name) {
  Lock lock;
  if (isBusyLocked()) return false;
  gPending       = PendingProvision{address, addrType, name};
  gWantProvision = true;
  return true;
}

ProvisionState bleProvisionerState() {
  Lock lock;
  return gState;
}

void bleProvisionResult(String& fingerprint, String& msg) {
  Lock lock;
  fingerprint = gResultFp;
  msg         = gResultMsg;
}
