#pragma once
#include <Arduino.h>
#include <vector>

// A tealight seen advertising, not yet named. `address`/`addrType` are exactly
// what's needed to reconnect for naming — the UI echoes them back, no indexing.
struct FoundDevice {
  String  address;
  uint8_t addrType;
  String  name;
  int     rssi;
};

enum class ProvisionState : uint8_t { IDLE, SCANNING, CONNECTING, WAITING, SUCCESS, FAILED };

// Runs the BLE central role on its own FreeRTOS task — every call here is
// non-blocking; the task does the actual (slow) scanning/connecting.
void bleProvisionerInit();

void                      bleDiscoveryStart(); // no-op if a run is already active
std::vector<FoundDevice>  bleDiscoveryFound();

// Connects to `address`, reads the device's fingerprint, writes its friendly
// name, and marks SUCCESS. Returns false (no-op) if a run is already active.
bool bleProvisionStart(const String& address, uint8_t addrType, const String& name);

ProvisionState bleProvisionerState();

// Valid once state is SUCCESS or FAILED; blank otherwise.
void bleProvisionResult(String& fingerprint, String& msg);
