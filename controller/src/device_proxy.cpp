#include <HTTPClient.h>
#include "device_proxy.h"
#include "registry.h"
#include "mdns_browser.h"
#include "config.h"

static ProxyResult doRequest(const String& ip, const char* method, const String& path, const String& jsonBody) {
  ProxyResult r;
  if (ip.length() == 0) return r;

  HTTPClient http;
  http.setTimeout(PROXY_TIMEOUT_MS);
  String url = "http://" + ip + path;
  if (!http.begin(url)) return r;

  int status;
  if (strcmp(method, "POST") == 0) {
    http.addHeader("Content-Type", "application/json");
    status = http.POST(jsonBody);
  } else {
    status = http.GET();
  }

  if (status > 0) {
    r.ok         = true;
    r.httpStatus = status;
    r.body       = http.getString();
  }
  http.end();
  return r;
}

ProxyResult deviceProxyRequest(const String& fingerprint, const char* method, const String& path, const String& jsonBody) {
  DeviceRecord rec;
  if (!registryGet(fingerprint, rec)) {
    ProxyResult r;
    r.httpStatus = 404;
    return r;
  }

  if (rec.lastIp.length()) {
    ProxyResult r = doRequest(rec.lastIp, method, path, jsonBody);
    if (r.ok) return r;
  }

  String freshIp = mdnsResolveByFingerprint(fingerprint);
  if (freshIp.length()) {
    ProxyResult r = doRequest(freshIp, method, path, jsonBody);
    if (r.ok) return r;
  }

  registryMarkUnreachable(fingerprint);
  ProxyResult failed;
  failed.httpStatus = 504;
  return failed;
}
