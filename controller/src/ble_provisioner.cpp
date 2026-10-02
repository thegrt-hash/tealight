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
  uint8_t addrType = 0;
  String  ssid, pass, name;
};
static PendingProvision gPending;

static String gResultFp, gResultIp, gResultMsg;

// Runs on NimBLE's own host task via the notify subscription — bridges the
// device's StatusNotify payload into our state machine.
static void onStatusNotify(NimBLERemoteCharacteristic* /*chr*/, uint8_t* data, size_t len, bool /*isNotify*/) {
  JsonDocument doc;
  if (deserializeJson(doc, (const char*)data, len)) return;
  String status = doc["status"] | "";

  Lock lock;
  if (status == "connected") {
    gState    = ProvisionState::SUCCESS;
    gResultFp = String((const char*)(doc["fingerprint"] | ""));
    gResultIp = String((const char*)(doc["ip"] | ""));
    gResultMsg = "";
  } else if (status == "failed") {
    gState     = ProvisionState::FAILED;
    gResultMsg = "device reported a WiFi connect failure";
  }
  // "idle"/"connecting" — keep waiting.
}

static void provisionOne(const PendingProvision& req) {
  { Lock lock; gState = ProvisionState::CONNECTING; gResultFp = ""; gResultIp = ""; gResultMsg = ""; }

  NimBLEAddress addr(std::string(req.address.c_str()), req.addrType);
  NimBLEClient*  client = NimBLEDevice::createClient();
  client->setConnectTimeout(8000);

  if (!client->connect(addr)) {
    NimBLEDevice::deleteClient(client);
    Lock lock;
    gState     = ProvisionState::FAILED;
    gResultMsg = "BLE connect failed";
    return;
  }

  NimBLERemoteService* svc = client->getService(BLE_SVC_UUID);
  if (!svc) {
    client->disconnect();
    NimBLEDevice::deleteClient(client);
    Lock lock;
    gState     = ProvisionState::FAILED;
    gResultMsg = "tealight service not found";
    return;
  }

  // Read the authoritative fingerprint/name before writing credentials.
  NimBLERemoteCharacteristic* infoChr = svc->getCharacteristic(BLE_CHR_INFO_UUID);
  String infoName;
  if (infoChr && infoChr->canRead()) {
    JsonDocument doc;
    if (!deserializeJson(doc, infoChr->readValue().c_str())) {
      infoName = String((const char*)(doc["name"] | ""));
    }
  }

  NimBLERemoteCharacteristic* statusChr = svc->getCharacteristic(BLE_CHR_STATUS_UUID);
  NimBLERemoteCharacteristic* credsChr  = svc->getCharacteristic(BLE_CHR_CREDS_UUID);
  if (!statusChr || !credsChr || !statusChr->canNotify() || !credsChr->canWrite()) {
    client->disconnect();
    NimBLEDevice::deleteClient(client);
    Lock lock;
    gState     = ProvisionState::FAILED;
    gResultMsg = "tealight characteristics missing";
    return;
  }

  statusChr->subscribe(true, onStatusNotify);

  JsonDocument credsDoc;
  credsDoc["ssid"] = req.ssid;
  credsDoc["pass"] = req.pass;
  credsDoc["name"] = req.name.length() ? req.name : infoName;
  String credsOut;
  serializeJson(credsDoc, credsOut);

  { Lock lock; gState = ProvisionState::WAITING; }

  if (!credsChr->writeValue(credsOut.c_str(), credsOut.length(), true)) {
    client->disconnect();
    NimBLEDevice::deleteClient(client);
    Lock lock;
    gState     = ProvisionState::FAILED;
    gResultMsg = "failed to write WiFi credentials";
    return;
  }

  // onStatusNotify (fired from NimBLE's host task) will flip gState to
  // SUCCESS/FAILED directly; we just wait for that or time out.
  uint32_t start = millis();
  while (millis() - start < BLE_PROVISION_TIMEOUT_MS) {
    {
      Lock lock;
      if (gState == ProvisionState::SUCCESS || gState == ProvisionState::FAILED) break;
    }
    delay(100);
  }
  {
    Lock lock;
    if (gState == ProvisionState::WAITING) {
      gState     = ProvisionState::FAILED;
      gResultMsg = "timed out waiting for the device to join WiFi";
    }
  }

  client->disconnect();
  NimBLEDevice::deleteClient(client);
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
      {
        Lock lock;
        gState = ProvisionState::SCANNING;
      }
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

// SUCCESS/FAILED are terminal-but-idle: a finished provisioning run (read
// via bleProvisionResult()) shouldn't permanently block the next command,
// only an operation still actually in flight should.
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

bool bleProvisionStart(const String& address, uint8_t addrType, const String& ssid, const String& pass, const String& name) {
  Lock lock;
  if (isBusyLocked()) return false;
  gPending       = PendingProvision{address, addrType, ssid, pass, name};
  gWantProvision = true;
  return true;
}

ProvisionState bleProvisionerState() {
  Lock lock;
  return gState;
}

void bleProvisionResult(String& fingerprint, String& ip, String& msg) {
  Lock lock;
  fingerprint = gResultFp;
  ip          = gResultIp;
  msg         = gResultMsg;
}
