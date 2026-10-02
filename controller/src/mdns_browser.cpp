#include <ESPmDNS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "mdns_browser.h"
#include "registry.h"
#include "config.h"

static SemaphoreHandle_t gQueryMutex = nullptr;

struct QueryLock {
  QueryLock() { xSemaphoreTake(gQueryMutex, portMAX_DELAY); }
  ~QueryLock() { xSemaphoreGive(gQueryMutex); }
};

// Assumes the caller holds gQueryMutex. Runs one query pass, updates the
// registry for every tealight found, and returns the IP for `wantFingerprint`
// if it was among the results (empty string otherwise).
static String runQueryLocked(const String& wantFingerprint) {
  String found;
  int n = MDNS.queryService(MDNS_SERVICE, MDNS_PROTO); // blocks ~3s internally
  for (int i = 0; i < n; i++) {
    if (!MDNS.hasTxt(i, "fp")) continue; // not one of ours
    String fp   = MDNS.txt(i, "fp");
    String ip   = MDNS.IP(i).toString();
    String name = MDNS.hasTxt(i, "name") ? MDNS.txt(i, "name") : ("Tealight-" + fp.substring(fp.length() - 4));
    registryNoteSeen(fp, ip, name);
    if (fp == wantFingerprint) found = ip;
  }
  return found;
}

String mdnsResolveByFingerprint(const String& fingerprint) {
  QueryLock lock;
  return runQueryLocked(fingerprint);
}

static void browseTask(void*) {
  for (;;) {
    {
      QueryLock lock;
      runQueryLocked("");
    }
    vTaskDelay(pdMS_TO_TICKS(MDNS_BROWSE_INTERVAL_MS));
  }
}

void mdnsBrowserInit() {
  gQueryMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(browseTask, "mdns_browse", 4096, nullptr, 1, nullptr, 0);
}
