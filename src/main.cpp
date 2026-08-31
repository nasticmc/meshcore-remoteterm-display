#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "config_manager.h"
#include "display_config.h"
#include "models.h"
#include "remoteterm_client.h"
#include "ui.h"
#include "ota_update.h"

RuntimeConfig runtimeConfig;
RemoteTermDisplay lcd;
AppState appState;
RemoteTermUI ui(lcd, appState, runtimeConfig);
RemoteTermClient remoteTerm(appState, runtimeConfig);

unsigned long lastWifiAttempt = 0;
bool clientStarted = false;
unsigned long wifiStartedAt = 0;
unsigned long lastClockSync = 0;
String serialLine;

void serialHelp() {
  Serial.println("RemoteTerm serial configuration");
  Serial.println("  help                         Show this help");
  Serial.println("  show                         Show settings (secrets redacted)");
  Serial.println("  nvs-status                   Diagnose NVS namespace and saved keys");
  Serial.println("  set wifi-ssid <value>        Set Wi-Fi SSID");
  Serial.println("  set wifi-password <value>    Set Wi-Fi password (write-only)");
  Serial.println("  set host <value>              Set RemoteTerm host");
  Serial.println("  set port <1-65535>            Set RemoteTerm port");
  Serial.println("  set tls <on|off>              Set HTTPS/WSS transport");
  Serial.println("  set username <value>          Set Basic Auth username");
  Serial.println("  set remoteterm-password <value>  Set Basic Auth password (write-only)");
  Serial.println("  set rotation <0-7>            Set display rotation");
  Serial.println("  save                         Save settings to NVS");
  Serial.println("  clear                        Clear saved settings (keeps running values)");
  Serial.println("  reboot                       Save settings and restart");
  Serial.println("  ap                           Start the setup access point");
}

void serialShow() {
  Serial.println("--- RemoteTerm settings ---");
  Serial.printf("wifi-ssid: %s\n", runtimeConfig.wifiSsid.c_str());
  Serial.println("wifi-password: <redacted>");
  Serial.printf("host: %s\n", runtimeConfig.remoteHost.c_str());
  Serial.printf("port: %u\n", static_cast<unsigned>(runtimeConfig.remotePort));
  Serial.printf("tls: %s\n", runtimeConfig.remoteTls ? "on" : "off");
  Serial.printf("username: %s\n", runtimeConfig.remoteUsername.c_str());
  Serial.println("remoteterm-password: <redacted>");
  Serial.printf("rotation: %u\n", static_cast<unsigned>(runtimeConfig.displayRotation));
  Serial.printf("channel-selection: %s (%u selected)\n",
                runtimeConfig.channelSelectionConfigured ? "configured" : "all",
                static_cast<unsigned>(runtimeConfig.selectedChannelCount));
  Serial.printf("firmware: %s\n", REMOTETERM_FIRMWARE_VERSION);
}

String serialArgument(const String& line, const char* command) {
  const String prefix = String(command) + " ";
  if (!line.startsWith(prefix)) return "";
  String value = line.substring(prefix.length());
  value.trim();
  return value;
}

void serialCommand(String line) {
  line.trim();
  if (!line.length()) return;
  if (line == "help") { serialHelp(); return; }
  if (line == "show") { serialShow(); return; }
  if (line == "nvs-status") { printRuntimeConfigNvsStatus(Serial); return; }
  if (line == "save") { Serial.println(saveRuntimeConfig(runtimeConfig) ? "Settings saved" : "Settings save failed"); return; }
  if (line == "clear") { clearRuntimeConfig(); Serial.println("Saved settings cleared; reboot to apply defaults"); return; }
  if (line == "reboot") { const bool saved = saveRuntimeConfig(runtimeConfig); Serial.println(saved ? "Settings saved; restarting" : "Settings save failed; not restarting"); if (saved) { delay(250); ESP.restart(); } return; }
  if (line == "ap") { setupAccessPoint(runtimeConfig); return; }

  String value = serialArgument(line, "set wifi-ssid");
  if (value.length()) { runtimeConfig.wifiSsid = value; Serial.println(saveRuntimeConfig(runtimeConfig) ? "Wi-Fi SSID updated and saved" : "Wi-Fi SSID updated but save failed"); return; }
  value = serialArgument(line, "set wifi-password");
  if (value.length()) { runtimeConfig.wifiPassword = value; Serial.println(saveRuntimeConfig(runtimeConfig) ? "Wi-Fi password updated and saved" : "Wi-Fi password updated but save failed"); return; }
  value = serialArgument(line, "set host");
  if (value.length()) { runtimeConfig.remoteHost = value; Serial.println(saveRuntimeConfig(runtimeConfig) ? "RemoteTerm host updated and saved" : "RemoteTerm host updated but save failed"); return; }
  value = serialArgument(line, "set port");
  if (value.length()) {
    const long port = value.toInt();
    if (port >= 1 && port <= 65535) { runtimeConfig.remotePort = static_cast<uint16_t>(port); Serial.println(saveRuntimeConfig(runtimeConfig) ? "RemoteTerm port updated and saved" : "RemoteTerm port updated but save failed"); }
    else Serial.println("Invalid port");
    return;
  }
  value = serialArgument(line, "set tls");
  if (value.length()) {
    if (value == "on" || value == "off") { runtimeConfig.remoteTls = value == "on"; Serial.println(saveRuntimeConfig(runtimeConfig) ? "TLS setting updated and saved" : "TLS setting updated but save failed"); }
    else Serial.println("Use set tls on or set tls off");
    return;
  }
  value = serialArgument(line, "set username");
  if (value.length()) { runtimeConfig.remoteUsername = value; Serial.println(saveRuntimeConfig(runtimeConfig) ? "Basic Auth username updated and saved" : "Basic Auth username updated but save failed"); return; }
  value = serialArgument(line, "set remoteterm-password");
  if (value.length()) { runtimeConfig.remotePassword = value; Serial.println(saveRuntimeConfig(runtimeConfig) ? "Basic Auth password updated and saved" : "Basic Auth password updated but save failed"); return; }
  value = serialArgument(line, "set rotation");
  if (value.length()) {
    const long rotation = value.toInt();
    if (rotation >= 0 && rotation <= 7) { runtimeConfig.displayRotation = static_cast<uint8_t>(rotation); Serial.println(saveRuntimeConfig(runtimeConfig) ? "Rotation updated and saved; reboot to apply" : "Rotation updated but save failed"); }
    else Serial.println("Invalid rotation; use 0 through 7");
    return;
  }
  Serial.println("Unknown command; type help");
}

void serialLoop() {
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      serialCommand(serialLine);
      serialLine = "";
    } else if (serialLine.length() < 240) {
      serialLine += c;
    }
  }
}

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

void updateClock() {
  if (WiFi.status() == WL_CONNECTED && (lastClockSync == 0 || millis() - lastClockSync >= 3600000UL)) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    setenv("TZ", "AEST-10AEDT,M10.1.0,M4.1.0/3", 1);
    tzset();
    lastClockSync = millis();
  }
  struct tm now;
  if (getLocalTime(&now, 10)) {
    appState.timeValid = now.tm_year >= (2024 - 1900);
    appState.timeEpoch = static_cast<uint32_t>(mktime(&now));
  } else {
    appState.timeValid = false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(250);
  loadRuntimeConfig(runtimeConfig);
  Serial.println("Type 'help' for serial configuration commands");
  ui.begin();
  ui.render(true);
  if (runtimeConfig.hasWifi()) connectWifi();
  else setupAccessPoint(runtimeConfig);
}

void loop() {
  serialLoop();
  bool nowConnected = WiFi.status() == WL_CONNECTED;
  updateClock();
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
      setupSettingsServer(runtimeConfig);
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
  if (nav == 2) {
    ui.render(true);
  } else if (nav == 3) {
    saveRuntimeConfig(runtimeConfig);
    ESP.restart();
  } else if (nav == 4 || nav == 5) {
    ui.render(true);
  } else if (nav != 0 && appState.channelCount) {
    remoteTerm.selectChannel(appState.selectedChannel + nav);
    ui.render(true);
  } else {
    ui.render(false);
  }

  delay(10);
}
