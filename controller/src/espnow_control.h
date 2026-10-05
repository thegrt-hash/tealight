#pragma once
#include <Arduino.h>

// ESP-NOW transport for the controller. Sends commands to tealights (unicast by
// MAC, or broadcast to the whole fleet) and absorbs their reports/announces into
// the registry + a last-known-state cache the web UI reads.

struct TlFields {
  uint8_t mode       = 0;
  uint8_t hue        = 0;
  uint8_t sat        = 0;
  uint8_t speed      = 0;
  uint8_t intensity  = 0;
  uint8_t brightness = 0;
};

void espnowControlInit();  // call after WiFi is connected

void espnowSendState(const uint8_t mac[6], uint8_t fieldMask, const TlFields& f);
void espnowBroadcastState(uint8_t fieldMask, const TlFields& f);
void espnowSendIdentify(const uint8_t mac[6]);
void espnowBroadcastIdentify();
void espnowBroadcastQuery();

// Last state a device reported, serialized like the old /api/state body
// ({mode,modeName,hue,sat,speed,intensity,brightness}). False if none cached.
bool espnowCachedStateJson(const String& fingerprint, String& out);
