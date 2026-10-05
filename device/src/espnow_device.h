#pragma once
#include <Arduino.h>

// ESP-NOW control transport for a tealight. Replaces the old WiFi + REST API:
// the device never associates to an AP, it just parks its radio on
// FLEET_CHANNEL and listens for the controller's commands.
//
//   setup():  espnowInit(); then espnowAnnounce();
//   loop():   espnowService(now);   // flushes any pending report/announce

void espnowInit();               // WiFi STA (no connect) + fixed channel + esp_now
void espnowService(uint32_t now); // send queued announce/report; call every loop
void espnowAnnounce();           // broadcast presence (cold boot / wake / rename)
