#pragma once
#include <Arduino.h>
#include <vector>

// A tealight seen advertising, not yet provisioned. `address`/`addrType`
// are exactly what's needed to reconnect for provisioning — the UI just
// echoes them back in the provision request, no server-side indexing.
struct FoundDevice {
  String address;
  uint8_t addrType;
  String  name;
  int     rssi;
};

enum class ProvisionState : uint8_t { IDLE, SCANNING, CONNECTING, WAITING, SUCCESS, FAILED };

// Runs the BLE central role on its own FreeRTOS task — every call here is
// non-blocking; the task does the actual (slow) scanning/connecting.
void bleProvisionerInit();

// No-op if a scan or a provisioning run is already in progress.
void                      bleDiscoveryStart();
std::vector<FoundDevice>  bleDiscoveryFound();

// One device at a time, per the discovery UX: connects to `address`, writes
// the WiFi credentials, and waits for the device to report success/failure.
// Returns false (no-op) if a scan or another provisioning run is active.
bool bleProvisionStart(const String& address, uint8_t addrType, const String& ssid, const String& pass, const String& name);

ProvisionState bleProvisionerState();

// Valid once state is SUCCESS or FAILED; empty/blank otherwise.
void bleProvisionResult(String& fingerprint, String& ip, String& msg);
