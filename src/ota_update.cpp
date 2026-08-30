#include "ota_update.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace {
unsigned long lastCheck = 0;
bool checking = false;

String profileName() {
#if REMOTETERM_DISPLAY_PROFILE == 1044
  return "fnk0104s";
#else
  return "fnk0104b";
#endif
}

int versionPart(const String& value, int& position) {
  while (position < static_cast<int>(value.length()) &&
         (value[position] == 'v' || value[position] == 'V' || value[position] == '.')) ++position;
  int result = 0;
  bool found = false;
  while (position < static_cast<int>(value.length()) && isDigit(value[position])) {
    found = true;
    result = result * 10 + (value[position++] - '0');
  }
  return found ? result : 0;
}

int compareVersions(String left, String right) {
  int lp = 0, rp = 0;
  for (int part = 0; part < 3; ++part) {
    const int l = versionPart(left, lp);
    const int r = versionPart(right, rp);
    if (l != r) return l < r ? -1 : 1;
  }
  return 0;
}

bool downloadAndInstall(const String& url, const String& releaseTag) {
  WiFiClientSecure client;
#if TLS_INSECURE
  client.setInsecure();
#endif
  HTTPClient http;
  http.setConnectTimeout(OTA_CONNECT_TIMEOUT_MS);
  http.setTimeout(OTA_READ_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) return false;
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  const int length = http.getSize();
  if (!Update.begin(length > 0 ? length : UPDATE_SIZE_UNKNOWN)) {
    http.end();
    return false;
  }
  const size_t written = Update.writeStream(*http.getStreamPtr());
  const bool complete = (length <= 0 || written == static_cast<size_t>(length)) && Update.end(true);
  http.end();
  if (!complete || !Update.isFinished()) {
    Update.abort();
    return false;
  }
  Serial.printf("OTA installed %s (%u bytes); restarting\n", releaseTag.c_str(), static_cast<unsigned>(written));
  delay(500);
  ESP.restart();
  return true;
}

void checkForUpdate() {
  if (checking || WiFi.status() != WL_CONNECTED) return;
  checking = true;
  lastCheck = millis();
  WiFiClientSecure client;
#if TLS_INSECURE
  client.setInsecure();
#endif
  HTTPClient http;
  http.setConnectTimeout(OTA_CONNECT_TIMEOUT_MS);
  http.setTimeout(OTA_READ_TIMEOUT_MS);
  const String api = "https://api.github.com/repos/" REMOTETERM_GITHUB_REPOSITORY "/releases?per_page=20";
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  if (!http.begin(client, api)) { checking = false; return; }
  http.addHeader("Accept", "application/vnd.github+json");
  http.addHeader("User-Agent", "meshcore-remoteterm-display");
  if (http.GET() != HTTP_CODE_OK) { http.end(); checking = false; return; }

  JsonDocument releases;
  if (deserializeJson(releases, http.getStream())) { http.end(); checking = false; return; }
  http.end();
  const String wanted = "remoteterm-display-" + profileName() + ".bin";
  String selectedTag;
  String selectedUrl;
  for (JsonObject release : releases.as<JsonArray>()) {
    const String tag = release["tag_name"] | "";
    if (!tag.length() || compareVersions(REMOTETERM_FIRMWARE_VERSION, tag) >= 0) continue;
    for (JsonObject asset : release["assets"].as<JsonArray>()) {
      if (String(asset["name"] | "") == wanted) {
        if (!selectedTag.length() || compareVersions(selectedTag, tag) < 0) {
          selectedTag = tag;
          selectedUrl = asset["browser_download_url"] | "";
        }
        break;
      }
    }
  }
  if (selectedTag.length() && selectedUrl.length()) {
    Serial.printf("OTA update available: %s -> %s\n", REMOTETERM_FIRMWARE_VERSION, selectedTag.c_str());
    downloadAndInstall(selectedUrl, selectedTag);
  }
  checking = false;
}
}

void otaBegin() { lastCheck = 0; }

void otaLoop() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (lastCheck == 0 || millis() - lastCheck >= OTA_CHECK_INTERVAL_MS) checkForUpdate();
}