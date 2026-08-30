#pragma once

#include <Arduino.h>
#include "config.h"

// Check the public GitHub Releases API and install the board-specific asset
// when its release tag is newer than REMOTETERM_FIRMWARE_VERSION.
void otaBegin();
void otaLoop();
void otaCheckNow();