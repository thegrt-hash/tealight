#include <ESPmDNS.h>
#include "mdns_service.h"
#include "config.h"

static bool gStarted = false;

void mdnsStart(const String& hostname, const String& fingerprint, const String& name) {
  if (gStarted) MDNS.end();
  MDNS.begin(hostname);
  MDNS.addService(MDNS_SERVICE, MDNS_PROTO, API_PORT);
  MDNS.addServiceTxt(MDNS_SERVICE, MDNS_PROTO, "fp", fingerprint.c_str());
  MDNS.addServiceTxt(MDNS_SERVICE, MDNS_PROTO, "name", name.c_str());
  MDNS.addServiceTxt(MDNS_SERVICE, MDNS_PROTO, "fw", FW_VERSION);
  gStarted = true;
}

void mdnsStop() {
  if (!gStarted) return;
  MDNS.end();
  gStarted = false;
}
