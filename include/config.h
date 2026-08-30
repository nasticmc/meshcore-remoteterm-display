#pragma once

#include <Arduino.h>
#include "firmware_version.h"

// Compile-time defaults. Copy config.example.h to config.h for a local build
// and edit these values; runtime setup values are stored in NVS by the device.
#define WIFI_SSID "your-wifi"
#define WIFI_PASSWORD "your-password"
#define REMOTETERM_HOST "192.168.1.50"
#define REMOTETERM_PORT 8000
#define REMOTETERM_TLS false
#define REMOTETERM_USERNAME ""
#define REMOTETERM_PASSWORD ""
#define DEVICE_NAME "RemoteTerm Display"
#define INITIAL_MESSAGE_LIMIT 40
#define MAX_CHANNELS 64
#define MAX_MESSAGES 80
#define POLL_FALLBACK_MS 15000UL
#define CHANNEL_REFRESH_MS 300000UL
#define WIFI_RETRY_MS 10000UL
#define WS_RETRY_MS 5000UL
#define TLS_INSECURE true
#define WIFI_CONNECT_TIMEOUT_MS 30000UL
#define SETUP_AP_TIMEOUT_MS 0UL
#define SETUP_AP_PASSWORD "configure"
#define OTA_CHECK_INTERVAL_MS 21600000UL
#define OTA_CONNECT_TIMEOUT_MS 5000
#define OTA_READ_TIMEOUT_MS 15000



struct RuntimeConfig {
  String wifiSsid;
  String wifiPassword;
  String remoteHost;
  uint16_t remotePort = REMOTETERM_PORT;
  bool remoteTls = REMOTETERM_TLS;
  String remoteUsername;
  String remotePassword;

  bool hasWifi() const { return wifiSsid.length() > 0; }
  bool hasRemoteTerm() const { return remoteHost.length() > 0; }
};

void loadRuntimeConfig(RuntimeConfig& config);
void saveRuntimeConfig(const RuntimeConfig& config);
void clearRuntimeConfig();
