#include "fingerprint.h"

static String gFingerprint;

String getFingerprint() {
  if (gFingerprint.length() == 0) {
    uint64_t mac = ESP.getEfuseMac();
    char buf[13];
    snprintf(buf, sizeof(buf), "%012llx", (unsigned long long)mac);
    gFingerprint = String(buf);
  }
  return gFingerprint;
}

String getDefaultName() {
  String fp = getFingerprint();
  String suffix = fp.substring(fp.length() - 4);
  suffix.toUpperCase();
  return "Tealight-" + suffix;
}

String getMdnsHostname() {
  return "tealight-" + getFingerprint();
}
