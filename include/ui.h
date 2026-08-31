#pragma once
#include "display_config.h"
#include "models.h"

class RemoteTermUI {
 public:
  RemoteTermUI(RemoteTermDisplay& display, AppState& state, RuntimeConfig& config);
  void begin();
  void render(bool force = false);
  int pollChannelGesture(); // channel +/-; 2 settings; 3 save; 4 older; 5 newer

 private:
  RemoteTermDisplay& _lcd;
  AppState& _state;
  RuntimeConfig& _config;
  uint32_t _lastRenderHash = 0;
  bool _touchDown = false;
  int32_t _touchStartX = 0;
  int32_t _touchStartY = 0;
  unsigned long _touchStartAt = 0;
  bool _settingsMode = false;
  int _messageScroll = 0;
  bool _clockMode = false;
  unsigned long _lastInteractionAt = 0;
  uint16_t _lastTouchX = 0;
  uint16_t _lastTouchY = 0;

  uint32_t stateHash() const;
  void drawHeader();
  void drawMessages();
  void drawFooter();
  void drawSettings();
  void drawClock();
  void drawButton(int x, int y, int width, int height, const char* label, bool accent = false);
  void showClockMode();
  void drawWrapped(const String& text, int x, int& y, int width, int maxLines, uint16_t color, float fontSize);
  String shortTime(const String& iso) const;
};
