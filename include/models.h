#pragma once
#include <Arduino.h>
#include "config.h"

struct ChannelInfo {
  String key;
  String name;
};

struct MessageInfo {
  int64_t id = 0;
  String conversationKey;
  String sender;
  String text;
  String receivedAt;
  bool outgoing = false;
};

struct AppState {
  ChannelInfo channels[MAX_CHANNELS];
  size_t channelCount = 0;
  int selectedChannel = 0;

  MessageInfo messages[MAX_MESSAGES];
  size_t messageCount = 0;

  bool wifiConnected = false;
  bool wsConnected = false;
  bool apiHealthy = false;
  bool timeValid = false;
  uint32_t timeEpoch = 0;
  String status = "Starting";
};
