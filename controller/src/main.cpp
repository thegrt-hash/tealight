#include <Arduino.h>
#include <WiFi.h>
#include "secrets.h"
#include "config.h"
#include "registry.h"
#include "espnow_control.h"
#include "ble_provisioner.h"
#include "web_api.h"

void setup() {
  Serial.begin(MONITOR_SPEED);
  Serial.println("[controller] booting");

  WiFi.mode(WIFI_STA);
  WiFi.begin(CONTROLLER_WIFI_SSID, CONTROLLER_WIFI_PASS);
  Serial.print("[controller] connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.printf("\n[controller] WiFi up, ip=%s\n", WiFi.localIP().toString().c_str());

  registryInit();
  espnowControlInit();  // ESP-NOW command transport + report/announce intake
  bleProvisionerInit(); // BLE central: discovery + one-at-a-time naming
  webApiInit();         // REST API + web UI, async

  Serial.println("[controller] up");
}

void loop() {
  // Everything happens on background tasks and the async web server;
  // nothing needs servicing from the Arduino loop task.
  delay(1000);
}
