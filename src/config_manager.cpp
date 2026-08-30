#include "config_manager.h"

#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

namespace {
Preferences preferences;
WebServer server(80);
DNSServer dns;
RuntimeConfig* activeConfig = nullptr;
bool active = false;
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

String page() {
  const RuntimeConfig& c = *activeConfig;
  String out = "<!doctype html><html><head><meta name=viewport content=\"width=device-width,initial-scale=1\"><title>RemoteTerm setup</title>";
  out += "<style>body{font:16px system-ui;max-width:600px;margin:2rem auto;padding:0 1rem;background:#101418;color:#eee}input,select{width:100%;box-sizing:border-box;padding:.7rem;margin:.3rem 0 1rem;background:#202832;color:#fff;border:1px solid #52606d;border-radius:4px}button{padding:.8rem 1.2rem;background:#36a167;color:white;border:0;border-radius:4px;font-weight:bold}label{display:block}small{color:#aab4bf}</style></head><body>";
  out += "<h1>MeshCore RemoteTerm Display</h1><p>Configure Wi-Fi and the RemoteTerm server. The display will restart after saving.</p>";
  out += "<form method=post action=/save><label>Wi-Fi network<input name=ssid required value=\"" + htmlEscape(c.wifiSsid) + "\"></label>";
  out += "<label>Wi-Fi password<input name=wifi_password type=password value=\"" + htmlEscape(c.wifiPassword) + "\"></label>";
  out += "<label>RemoteTerm host<input name=host required placeholder=192.168.1.50 value=\"" + htmlEscape(c.remoteHost) + "\"></label>";
  out += "<label>RemoteTerm port<input name=port type=number min=1 max=65535 value=\"" + String(c.remotePort) + "\"></label>";
  out += "<label>Transport<select name=tls><option value=0" + String(c.remoteTls ? "" : " selected") + ">HTTP / WS</option><option value=1" + String(c.remoteTls ? " selected" : "") + ">HTTPS / WSS</option></select></label>";
  out += "<label>Basic Auth username<input name=username value=\"" + htmlEscape(c.remoteUsername) + "\"></label>";
  out += "<label>Basic Auth password<input name=remote_password type=password value=\"" + htmlEscape(c.remotePassword) + "\"></label>";
  out += "<button type=submit>Save and restart</button></form><p><small>Connect to this device's Wi-Fi network and open http://192.168.4.1 if your phone does not open the page automatically.</small></p></body></html>";
  return out;
}

void handleRoot() { server.send(200, "text/html", page()); }
void handleNotFound() { server.send(200, "text/html", page()); }
void handleSave() {
  if (!activeConfig || !server.hasArg("ssid") || !server.hasArg("host")) {
    server.send(400, "text/plain", "Wi-Fi SSID and RemoteTerm host are required");
    return;
  }
  activeConfig->wifiSsid = server.arg("ssid");
  activeConfig->wifiPassword = server.arg("wifi_password");
  activeConfig->remoteHost = server.arg("host");
  activeConfig->remotePort = static_cast<uint16_t>(server.arg("port").toInt());
  if (activeConfig->remotePort == 0) activeConfig->remotePort = REMOTETERM_PORT;
  activeConfig->remoteTls = server.arg("tls") == "1";
  activeConfig->remoteUsername = server.arg("username");
  activeConfig->remotePassword = server.arg("remote_password");
  saveRuntimeConfig(*activeConfig);
  server.send(200, "text/html", "<html><body><h1>Saved</h1><p>Restarting...</p></body></html>");
  delay(600);
  ESP.restart();
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
  Preferences p;
  p.begin("remoteterm", true);
  config.wifiSsid = p.getString("wifi_ssid", config.wifiSsid);
  config.wifiPassword = p.getString("wifi_pass", config.wifiPassword);
  config.remoteHost = p.getString("rt_host", config.remoteHost);
  config.remotePort = static_cast<uint16_t>(p.getUShort("rt_port", config.remotePort));
  config.remoteTls = p.getBool("rt_tls", config.remoteTls);
  config.remoteUsername = p.getString("rt_user", config.remoteUsername);
  config.remotePassword = p.getString("rt_pass", config.remotePassword);
  p.end();
}

void saveRuntimeConfig(const RuntimeConfig& c) {
  Preferences p;
  p.begin("remoteterm", false);
  p.putString("wifi_ssid", c.wifiSsid);
  p.putString("wifi_pass", c.wifiPassword);
  p.putString("rt_host", c.remoteHost);
  p.putUShort("rt_port", c.remotePort);
  p.putBool("rt_tls", c.remoteTls);
  p.putString("rt_user", c.remoteUsername);
  p.putString("rt_pass", c.remotePassword);
  p.end();
}

void clearRuntimeConfig() {
  Preferences p;
  p.begin("remoteterm", false);
  p.clear();
  p.end();
}

bool setupAccessPoint(RuntimeConfig& config) {
  activeConfig = &config;
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0));
  const String ssid = String("RemoteTerm-") + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);
  if (!WiFi.softAP(ssid.c_str(), SETUP_AP_PASSWORD)) return false;
  dns.start(53, "*", apIp);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleNotFound);
  server.begin();
  active = true;
  Serial.printf("Setup AP started: %s / http://%s\n", ssid.c_str(), apIp.toString().c_str());
  return true;
}

void setupAccessPointLoop() {
  if (!active) return;
  dns.processNextRequest();
  server.handleClient();
}

bool setupAccessPointActive() { return active; }
