#pragma once
#include <Arduino.h>

// REST API consumed only by the controller (no on-device UI). Routes:
//   GET  /api/state    -> current mode/color/speed/intensity/brightness
//   POST /api/state    -> partial patch, JSON body; returns new full state
//   GET  /api/modes    -> [{index,name}, ...] for whatever modes this build has
//   GET  /api/info     -> fingerprint/name/fw/uptime/ip
//   POST /api/identify -> brief flash so a human can find the physical device
void webApiInit();
void webApiService(); // call every loop(); cheap when idle
