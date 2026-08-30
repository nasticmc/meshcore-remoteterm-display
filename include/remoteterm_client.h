#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include "models.h"

class RemoteTermClient {
 public:
  RemoteTermClient(AppState& state, const RuntimeConfig& config);
  void begin();
  void loop();
  bool refreshChannels();
  bool loadSelectedMessages(bool initial = false);
  void selectChannel(int index);
  void reconnectWebSocket();

 private:
  AppState& _state;
  const RuntimeConfig& _config;
  WebSocketsClient _ws;
  unsigned long _lastPoll = 0;
  unsigned long _lastChannelRefresh = 0;
  unsigned long _lastWsAttempt = 0;

  String baseUrl() const;
  String authHeader() const;
  bool httpGet(const String& path, String& body);
  bool parseChannels(const String& json);
  bool parseMessages(const String& json, bool replace);
  bool ingestMessage(JsonVariantConst obj);
  void wsEvent(WStype_t type, uint8_t* payload, size_t length);
  void sortMessages();
};
