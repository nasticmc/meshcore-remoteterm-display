#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "config_manager.h"
#include "display_config.h"
#include "models.h"
#include "remoteterm_client.h"
#include "ui.h"
#include "ota_update.h"

RemoteTermDisplay lcd;
AppState appState;
RemoteTermUI ui(lcd, appState);
RuntimeConfig runtimeConfig;
RemoteTermClient remoteTerm(appState, runtimeConfig);

unsigned long lastWifiAttempt = 0;
bool clientStarted = false;
unsigned long wifiStartedAt = 0;

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastWifiAttempt < WIFI_RETRY_MS && lastWifiAttempt != 0) return;
  lastWifiAttempt = millis();

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_NAME);
  WiFi.begin(runtimeConfig.wifiSsid.c_str(), runtimeConfig.wifiPassword.c_str());
  if (wifiStartedAt == 0) wifiStartedAt = millis();
  appState.status = "Connecting Wi-Fi";
}

void setup() {
  Serial.begin(115200);
  delay(250);
  loadRuntimeConfig(runtimeConfig);
  ui.begin();
  ui.render(true);
  if (runtimeConfig.hasWifi()) connectWifi();
  else setupAccessPoint(runtimeConfig);
}

void loop() {
  bool nowConnected = WiFi.status() == WL_CONNECTED;
  setupAccessPointLoop();
  if (!nowConnected && !setupAccessPointActive() && wifiStartedAt && millis() - wifiStartedAt >= WIFI_CONNECT_TIMEOUT_MS) {
    appState.status = "Setup AP";
    ui.render(true);
    setupAccessPoint(runtimeConfig);
  }
  if (nowConnected != appState.wifiConnected) {
    appState.wifiConnected = nowConnected;
    if (nowConnected) {
      appState.status = "RemoteTerm";
      Serial.printf("Wi-Fi connected: %s\n", WiFi.localIP().toString().c_str());
      remoteTerm.begin();
      otaBegin();
      clientStarted = true;
    } else {
      appState.wsConnected = false;
      appState.apiHealthy = false;
      appState.status = "Wi-Fi offline";
      clientStarted = false;
    }
    ui.render(true);
  }

  if (!nowConnected && !setupAccessPointActive()) connectWifi();
  if (nowConnected && clientStarted) remoteTerm.loop();
  if (nowConnected && clientStarted) otaLoop();

  int nav = ui.pollChannelGesture();
  if (nav != 0 && appState.channelCount) {
    remoteTerm.selectChannel(appState.selectedChannel + nav);
    ui.render(true);
  } else {
    ui.render(false);
  }

  delay(10);
}
