#include "config_manager.h"

#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include "ota_update.h"

namespace {
WebServer server(80);
DNSServer dns;
RuntimeConfig* activeConfig = nullptr;
bool active = false;
bool apMode = false;
const IPAddress apIp(192, 168, 4, 1);

String htmlEscape(const String& value) {
  String out;
  for (char c : value) {
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '\"') out += "&quot;";
    else out += c;
  }
  return out;
}

String checked(bool value) { return value ? " checked" : ""; }

String page() {
  const RuntimeConfig& c = *activeConfig;
  String out = "<!doctype html><html><head><meta name=viewport content=\"width=device-width,initial-scale=1\"><title>RemoteTerm settings</title>";
  out += "<style>body{font:16px system-ui;max-width:680px;margin:2rem auto;padding:0 1rem;background:#222;color:#e6eaf0}input,select{width:100%;box-sizing:border-box;padding:.7rem;margin:.3rem 0 1rem;background:#303030;color:#e6eaf0;border:1px solid #59636d;border-radius:4px}button{padding:.8rem 1.2rem;background:#36a167;color:white;border:0;border-radius:4px;font-weight:bold}label{display:block}.channels{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:.4rem;margin:.5rem 0 1rem}.channels label{background:#303030;padding:.6rem;border-radius:4px}.channels input{width:auto;margin:0 .4rem 0 0}small{color:#9aa4b2}</style></head><body>";
  out += "<h1>MeshCore RemoteTerm Display</h1><p>Settings are saved in ESP32 non-volatile storage and applied after restart.</p>";
  out += "<form method=post action=/save><label>Wi-Fi network<input name=ssid required value=\"" + htmlEscape(c.wifiSsid) + "\"></label>";
  out += "<label>Wi-Fi password<input name=wifi_password type=password placeholder=\"Leave blank to keep current password\"></label>";
  out += "<label>RemoteTerm host<input name=host required placeholder=192.168.1.50 value=\"" + htmlEscape(c.remoteHost) + "\"></label>";
  out += "<label>RemoteTerm port<input name=port type=number min=1 max=65535 value=\"" + String(c.remotePort) + "\"></label>";
  out += "<label>Transport<select name=tls><option value=0" + String(c.remoteTls ? "" : " selected") + ">HTTP / WS</option><option value=1" + String(c.remoteTls ? " selected" : "") + ">HTTPS / WSS</option></select></label>";
  out += "<label>Basic Auth username<input name=username value=\"" + htmlEscape(c.remoteUsername) + "\"></label>";
  out += "<label>Basic Auth password<input name=remote_password type=password placeholder=\"Leave blank to keep current password\"></label>";
  out += "<label>Display rotation<select name=rotation>";
  for (uint8_t rotation = 0; rotation < 8; ++rotation)
    out += "<option value=\"" + String(rotation) + "\"" + String(c.displayRotation == rotation ? " selected" : "") + ">" + String(rotation) + "</option>";
  out += "</select></label>";
  out += "<h2>Channels</h2><small>When no channels are selected, all channels are shown. Cached channels appear after the display has connected to RemoteTerm.</small><p><button type=button onclick=\"document.querySelectorAll('input[name=channel]').forEach(x=>x.checked=true)\">Select all</button> <button type=button onclick=\"document.querySelectorAll('input[name=channel]').forEach(x=>x.checked=false)\">Clear all</button></p><div class=channels>";
  for (size_t i = 0; i < c.cachedChannelCount; ++i) {
    const auto& channel = c.cachedChannels[i];
    out += "<label><input type=checkbox name=channel value=\"" + htmlEscape(channel.key) + "\"" + checked(c.channelSelected(channel.key) && c.channelSelectionConfigured) + ">" + htmlEscape(channel.name) + "</label>";
  }
  if (!c.cachedChannelCount) out += "<p><small>No cached channels yet. Save the server settings, allow a connection, then revisit this page.</small></p>";
  out += "</div><button type=submit>Save and restart</button></form>";
  out += "<form method=post action=/ota style=margin-top:1rem><button type=submit>Check for OTA update</button></form>";
  out += "<p><small>Setup AP: connect to RemoteTerm-XXXX with password \"configure\" and open http://192.168.4.1.</small></p></body></html>";
  return out;
}

void handleRoot() { server.send(200, "text/html", page()); }
void handleNotFound() { server.send(200, "text/html", page()); }

void handleOta() {
  if (WiFi.status() != WL_CONNECTED) {
    server.send(503, "text/plain", "OTA requires an active Wi-Fi connection");
    return;
  }
  server.send(200, "text/html", "<html><body><h1>OTA check started</h1><p>Watch the serial console for the result. The device will restart if an update is installed.</p></body></html>");
  delay(100);
  otaCheckNow();
}

void handleSave() {
  if (!activeConfig || !server.hasArg("ssid") || !server.hasArg("host")) {
    server.send(400, "text/plain", "Wi-Fi SSID and RemoteTerm host are required");
    return;
  }
  activeConfig->wifiSsid = server.arg("ssid");
  if (server.hasArg("wifi_password") && server.arg("wifi_password").length()) activeConfig->wifiPassword = server.arg("wifi_password");
  activeConfig->remoteHost = server.arg("host");
  const long port = server.arg("port").toInt();
  activeConfig->remotePort = (port >= 1 && port <= 65535) ? static_cast<uint16_t>(port) : REMOTETERM_PORT;
  activeConfig->remoteTls = server.arg("tls") == "1";
  activeConfig->remoteUsername = server.arg("username");
  if (server.hasArg("remote_password") && server.arg("remote_password").length()) activeConfig->remotePassword = server.arg("remote_password");
  const long rotation = server.arg("rotation").toInt();
  activeConfig->displayRotation = static_cast<uint8_t>((rotation >= 0 && rotation <= 7) ? rotation : DEFAULT_DISPLAY_ROTATION);

  activeConfig->selectedChannelCount = 0;
  activeConfig->channelSelectionConfigured = false;
  for (int i = 0; i < server.args() && activeConfig->selectedChannelCount < MAX_SELECTED_CHANNELS; ++i) {
    if (server.argName(i) != "channel") continue;
    activeConfig->selectedChannelKeys[activeConfig->selectedChannelCount++] = server.arg(i);
    activeConfig->channelSelectionConfigured = true;
  }
  if (!saveRuntimeConfig(*activeConfig)) {
    server.send(500, "text/plain", "Settings could not be saved to NVS; device was not restarted");
    return;
  }
  server.send(200, "text/html", "<html><body><h1>Saved</h1><p>Restarting...</p></body></html>");
  delay(600);
  ESP.restart();
}

void registerServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/ota", HTTP_POST, handleOta);
  server.onNotFound(handleNotFound);
  server.begin();
}
}

void loadRuntimeConfig(RuntimeConfig& config) {
  config.wifiSsid = WIFI_SSID;
  config.wifiPassword = WIFI_PASSWORD;
  config.remoteHost = REMOTETERM_HOST;
  config.remotePort = REMOTETERM_PORT;
  config.remoteTls = REMOTETERM_TLS;
  config.remoteUsername = REMOTETERM_USERNAME;
  config.remotePassword = REMOTETERM_PASSWORD;
  config.displayRotation = DEFAULT_DISPLAY_ROTATION;
  config.selectedChannelCount = 0;
  config.channelSelectionConfigured = false;
  config.cachedChannelCount = 0;

  Preferences p;
  const bool opened = p.begin("remoteterm", true);
  Serial.printf("NVS read namespace: %s\n", opened ? "available" : "missing/unavailable");
  if (!opened) return;
  config.wifiSsid = p.getString("wifi_ssid", config.wifiSsid);
  config.wifiPassword = p.getString("wifi_pass", config.wifiPassword);
  config.remoteHost = p.getString("rt_host", config.remoteHost);
  config.remotePort = static_cast<uint16_t>(p.getUShort("rt_port", config.remotePort));
  config.remoteTls = p.getBool("rt_tls", config.remoteTls);
  config.remoteUsername = p.getString("rt_user", config.remoteUsername);
  config.remotePassword = p.getString("rt_pass", config.remotePassword);
  config.displayRotation = p.getUChar("rotation", config.displayRotation) % 8;
  const String selected = p.getString("channels", "");
  config.channelSelectionConfigured = p.getBool("channels_set", false);
  int start = 0;
  while (config.selectedChannelCount < MAX_SELECTED_CHANNELS && start < static_cast<int>(selected.length())) {
    const int end = selected.indexOf('\x1f', start);
    const int stop = end < 0 ? selected.length() : end;
    if (stop > start) config.selectedChannelKeys[config.selectedChannelCount++] = selected.substring(start, stop);
    if (end < 0) break;
    start = end + 1;
  }
  p.end();
}

bool saveRuntimeConfig(const RuntimeConfig& c) {
  Preferences p;
  if (!p.begin("remoteterm", false)) {
    Serial.println("NVS write namespace: open failed");
    return false;
  }
  bool ok = true;
  p.putString("wifi_ssid", c.wifiSsid); ok = p.isKey("wifi_ssid") && ok;
  p.putString("wifi_pass", c.wifiPassword); ok = p.isKey("wifi_pass") && ok;
  p.putString("rt_host", c.remoteHost); ok = p.isKey("rt_host") && ok;
  p.putUShort("rt_port", c.remotePort); ok = p.isKey("rt_port") && ok;
  p.putBool("rt_tls", c.remoteTls); ok = p.isKey("rt_tls") && ok;
  p.putString("rt_user", c.remoteUsername); ok = p.isKey("rt_user") && ok;
  p.putString("rt_pass", c.remotePassword); ok = p.isKey("rt_pass") && ok;
  p.putUChar("rotation", c.displayRotation); ok = p.isKey("rotation") && ok;
  String selected;
  for (size_t i = 0; i < c.selectedChannelCount; ++i) {
    if (i) selected += '\x1f';
    selected += c.selectedChannelKeys[i];
  }
  p.putString("channels", selected); ok = p.isKey("channels") && ok;
  p.putBool("channels_set", c.channelSelectionConfigured); ok = p.isKey("channels_set") && ok;
  p.end();
  Serial.printf("NVS write result: %s\n", ok ? "verified" : "failed");
  return ok;
}

void clearRuntimeConfig() {
  Preferences p;
  if (!p.begin("remoteterm", false)) {
    Serial.println("NVS clear: namespace open failed");
    return;
  }
  const bool cleared = p.clear();
  p.end();
  Serial.printf("NVS clear: %s\n", cleared ? "complete" : "failed");
}

void printRuntimeConfigNvsStatus(Stream& output) {
  Preferences p;
  if (!p.begin("remoteterm", true)) {
    output.println("NVS namespace: unavailable");
    output.println("NVS diagnosis: Preferences.begin failed");
    return;
  }
  output.println("NVS namespace: available");
  output.printf("NVS free entries: %u\n", static_cast<unsigned>(p.freeEntries()));
  const char* keys[] = {"wifi_ssid", "wifi_pass", "rt_host", "rt_port", "rt_tls", "rt_user", "rt_pass", "rotation", "channels", "channels_set"};
  for (const char* key : keys) output.printf("NVS key %-13s: %s\n", key, p.isKey(key) ? "present" : "missing");
  p.end();
}

bool setupAccessPoint(RuntimeConfig& config) {
  activeConfig = &config;
  apMode = true;
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0));
  const String ssid = String("RemoteTerm-") + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);
  if (!WiFi.softAP(ssid.c_str(), SETUP_AP_PASSWORD)) return false;
  dns.start(53, "*", apIp);
  registerServer();
  active = true;
  Serial.printf("Setup AP started: %s / http://%s\n", ssid.c_str(), apIp.toString().c_str());
  return true;
}

void setupSettingsServer(RuntimeConfig& config) {
  activeConfig = &config;
  apMode = false;
  registerServer();
  active = true;
  Serial.printf("Settings server: http://%s\n", WiFi.localIP().toString().c_str());
}

void setupAccessPointLoop() {
  if (!active) return;
  if (apMode) dns.processNextRequest();
  server.handleClient();
}

bool setupAccessPointActive() { return active && apMode; }
