#pragma once

#include <Arduino.h>
#include "config.h"

// Runtime configuration is persisted in ESP32 NVS. It is intentionally kept
// separate from the display project and contains no credentials in source.
void loadRuntimeConfig(RuntimeConfig& config);
bool saveRuntimeConfig(const RuntimeConfig& config);
void clearRuntimeConfig();

// Starts the configuration page on the setup AP.
bool setupAccessPoint(RuntimeConfig& config);
// Starts the same configuration page on the normal Wi-Fi interface.
void setupSettingsServer(RuntimeConfig& config);
void setupAccessPointLoop();
bool setupAccessPointActive();
