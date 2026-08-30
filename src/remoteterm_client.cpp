#include "remoteterm_client.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <algorithm>
#include <functional>

namespace {
String b64(const String& in) {
  static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String out;
  int val = 0, valb = -6;
  for (uint8_t c : in) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      out += table[(val >> valb) & 0x3F];
      valb -= 6;
    }
  }
  if (valb > -6) out += table[((val << 8) >> (valb + 8)) & 0x3F];
  while (out.length() % 4) out += '=';
  return out;
}

String pickString(JsonVariantConst obj, const char* a, const char* b = nullptr, const char* c = nullptr) {
  if (obj[a].is<const char*>()) return String(obj[a].as<const char*>());
  if (b && obj[b].is<const char*>()) return String(obj[b].as<const char*>());
  if (c && obj[c].is<const char*>()) return String(obj[c].as<const char*>());
  return "";
}
}

RemoteTermClient::RemoteTermClient(AppState& state, RuntimeConfig& config) : _state(state), _config(config) {}

String RemoteTermClient::baseUrl() const {
  String s = _config.remoteTls ? "https://" : "http://";
  s += _config.remoteHost;
  s += ":";
  s += _config.remotePort;
  return s;
}

String RemoteTermClient::authHeader() const {
  if (_config.remoteUsername.length() == 0) return "";
  return "Basic " + b64(_config.remoteUsername + ":" + _config.remotePassword);
}

bool RemoteTermClient::httpGet(const String& path, String& body) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  bool began = false;
  http.setConnectTimeout(2500);
  http.setTimeout(5000);
  if (_config.remoteTls) {
    auto* client = new WiFiClientSecure();
#if TLS_INSECURE
    client->setInsecure();
#endif
    began = http.begin(*client, baseUrl() + path);
    if (!began) { delete client; return false; }
    String auth = authHeader();
    if (auth.length()) http.addHeader("Authorization", auth);
    int code = http.GET();
    if (code >= 200 && code < 300) body = http.getString();
    http.end();
    delete client;
    return code >= 200 && code < 300;
  }

  WiFiClient client;
  began = http.begin(client, baseUrl() + path);
  if (!began) return false;
  String auth = authHeader();
  if (auth.length()) http.addHeader("Authorization", auth);
  int code = http.GET();
  if (code >= 200 && code < 300) body = http.getString();
  http.end();
  return code >= 200 && code < 300;
}

bool RemoteTermClient::parseChannels(const String& json) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;

  JsonArrayConst arr;
  if (doc.is<JsonArray>()) arr = doc.as<JsonArrayConst>();
  else if (doc["channels"].is<JsonArray>()) arr = doc["channels"].as<JsonArrayConst>();
  else return false;

  String oldKey;
  if (_state.channelCount && _state.selectedChannel >= 0 && _state.selectedChannel < (int)_state.channelCount)
    oldKey = _state.channels[_state.selectedChannel].key;

  _state.channelCount = 0;
  _config.cachedChannelCount = 0;
  for (JsonVariantConst item : arr) {
    if (_state.channelCount >= MAX_CHANNELS) break;
    String key = pickString(item, "key", "channel_key", "conversation_key");
    String name = pickString(item, "name", "channel_name", "label");
    if (!key.length()) continue;
    if (!name.length()) name = key.substring(0, min((size_t)8, key.length()));
    if (_config.cachedChannelCount < MAX_CONFIG_CHANNELS)
      _config.cachedChannels[_config.cachedChannelCount++] = {key, name};
    if (_config.channelSelected(key) && _state.channelCount < MAX_CHANNELS)
      _state.channels[_state.channelCount++] = {key, name};
  }

  if (_state.channelCount == 0) {
    _state.selectedChannel = 0;
    return true;
  }

  int restored = 0;
  if (oldKey.length()) {
    for (size_t i = 0; i < _state.channelCount; ++i) {
      if (_state.channels[i].key == oldKey) { restored = (int)i; break; }
    }
  }
  _state.selectedChannel = restored;
  return true;
}

bool RemoteTermClient::ingestMessage(JsonVariantConst obj) {
  String type = pickString(obj, "type", "message_type");
  if (type.length() && type != "CHAN") return false;

  String key = pickString(obj, "conversation_key", "channel_key");
  if (_state.channelCount == 0 || _state.selectedChannel < 0 || _state.selectedChannel >= (int)_state.channelCount) return false;
  if (key.length() && key != _state.channels[_state.selectedChannel].key) return false;

  int64_t id = 0;
  if (obj["id"].is<int64_t>()) id = obj["id"].as<int64_t>();
  else if (obj["message_id"].is<int64_t>()) id = obj["message_id"].as<int64_t>();

  if (id != 0) {
    for (size_t i = 0; i < _state.messageCount; ++i)
      if (_state.messages[i].id == id) return false;
  }

  MessageInfo msg;
  msg.id = id;
  msg.conversationKey = key;
  msg.sender = pickString(obj, "sender_name", "sender", "from_name");
  msg.text = pickString(obj, "text", "message", "body");
  msg.receivedAt = pickString(obj, "received_at", "created_at", "timestamp");
  msg.outgoing = obj["outgoing"] | false;
  if (!msg.text.length()) return false;

  if (_state.messageCount >= MAX_MESSAGES) {
    for (size_t i = 1; i < _state.messageCount; ++i) _state.messages[i - 1] = _state.messages[i];
    _state.messageCount--;
  }
  _state.messages[_state.messageCount++] = msg;
  return true;
}

void RemoteTermClient::sortMessages() {
  std::sort(_state.messages, _state.messages + _state.messageCount,
            [](const MessageInfo& a, const MessageInfo& b) {
              if (a.receivedAt == b.receivedAt) return a.id < b.id;
              return a.receivedAt < b.receivedAt;
            });
}

bool RemoteTermClient::parseMessages(const String& json, bool replace) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;
  JsonArrayConst arr;
  if (doc.is<JsonArray>()) arr = doc.as<JsonArrayConst>();
  else if (doc["messages"].is<JsonArray>()) arr = doc["messages"].as<JsonArrayConst>();
  else if (doc["items"].is<JsonArray>()) arr = doc["items"].as<JsonArrayConst>();
  else return false;

  if (replace) _state.messageCount = 0;
  for (JsonVariantConst item : arr) ingestMessage(item);
  sortMessages();
  return true;
}

bool RemoteTermClient::refreshChannels() {
  String body;
  if (!httpGet("/api/channels", body)) {
    _state.apiHealthy = false;
    _state.status = "RemoteTerm unavailable";
    return false;
  }
  if (!parseChannels(body)) {
    _state.apiHealthy = false;
    _state.status = "Channel JSON error";
    return false;
  }
  _state.apiHealthy = true;
  _lastChannelRefresh = millis();
  return true;
}

bool RemoteTermClient::loadSelectedMessages(bool initial) {
  if (_state.channelCount == 0) return false;
  const String& key = _state.channels[_state.selectedChannel].key;
  String encodedKey = key;
  encodedKey.replace("%", "%25");
  encodedKey.replace(" ", "%20");
  encodedKey.replace("#", "%23");
  encodedKey.replace("?", "%3F");
  encodedKey.replace("&", "%26");
  String path = "/api/messages?type=CHAN&conversation_key=" + encodedKey + "&limit=" + String(INITIAL_MESSAGE_LIMIT);
  String body;
  if (!httpGet(path, body)) {
    _state.apiHealthy = false;
    return false;
  }
  bool ok = parseMessages(body, initial);
  _state.apiHealthy = ok;
  if (ok) _lastPoll = millis();
  return ok;
}

void RemoteTermClient::selectChannel(int index) {
  if (_state.channelCount == 0) return;
  while (index < 0) index += _state.channelCount;
  index %= _state.channelCount;
  if (index == _state.selectedChannel) return;
  _state.selectedChannel = index;
  _state.messageCount = 0;
  loadSelectedMessages(true);
}

void RemoteTermClient::wsEvent(WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    _state.wsConnected = true;
    _state.status = "Live";
    return;
  }
  if (type == WStype_DISCONNECTED) {
    _state.wsConnected = false;
    if (_state.wifiConnected) _state.status = "Polling";
    return;
  }
  if (type != WStype_TEXT || !payload || !length) return;

  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) return;

  bool changed = false;
  // Current RemoteTerm broadcasts event envelopes; tolerate several shapes.
  if (doc["type"].is<const char*>() && String(doc["type"].as<const char*>()) == "message") {
    if (doc["data"].is<JsonObject>()) changed = ingestMessage(doc["data"]);
    else if (doc["message"].is<JsonObject>()) changed = ingestMessage(doc["message"]);
    else changed = ingestMessage(doc.as<JsonVariantConst>());
  } else if (doc["event"].is<const char*>() && String(doc["event"].as<const char*>()) == "message") {
    if (doc["data"].is<JsonObject>()) changed = ingestMessage(doc["data"]);
  } else if (doc["conversation_key"].is<const char*>()) {
    changed = ingestMessage(doc.as<JsonVariantConst>());
  }
  if (changed) sortMessages();
}

void RemoteTermClient::reconnectWebSocket() {
  _ws.disconnect();
  _ws.onEvent([this](WStype_t t, uint8_t* p, size_t l) { wsEvent(t, p, l); });
  _ws.setReconnectInterval(WS_RETRY_MS);
  _ws.enableHeartbeat(15000, 3000, 2);

  String hdr;
  String auth = authHeader();
  if (auth.length()) hdr = "Authorization: " + auth + "\r\n";
  if (hdr.length()) _ws.setExtraHeaders(hdr.c_str());

  if (_config.remoteTls) _ws.beginSSL(_config.remoteHost, _config.remotePort, "/api/ws");
  else _ws.begin(_config.remoteHost, _config.remotePort, "/api/ws");
  _lastWsAttempt = millis();
}

void RemoteTermClient::begin() {
  if (refreshChannels()) loadSelectedMessages(true);
  reconnectWebSocket();
}

void RemoteTermClient::loop() {
  _ws.loop();
  if (WiFi.status() != WL_CONNECTED) return;

  unsigned long now = millis();
  if (now - _lastChannelRefresh >= CHANNEL_REFRESH_MS) refreshChannels();

  // Poll as a safety net. It also catches API changes / websocket envelopes we
  // don't recognize, and keeps the selected channel current after WS outages.
  if (now - _lastPoll >= POLL_FALLBACK_MS) loadSelectedMessages(false);
}
