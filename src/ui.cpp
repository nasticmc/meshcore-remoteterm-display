#include "ui.h"
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

namespace {
// EastMesh dark-mode palette shared with the Hermes project. These are RGB565
// values for the LovyanGFX renderer, kept here so this project remains standalone.
constexpr uint16_t C_BG = 0x2104;      // #222222
constexpr uint16_t C_PANEL = 0x3186;   // #303030
constexpr uint16_t C_TEXT = 0xE75E;    // #E6EAF0
constexpr uint16_t C_MUTED = 0x9D36;   // #9AA4B2
constexpr uint16_t C_ACCENT = 0x350C;  // #36A167
constexpr uint16_t C_WARN = 0xDCC5;    // #D99A28
}

RemoteTermUI::RemoteTermUI(RemoteTermDisplay& display, AppState& state, RuntimeConfig& config) : _lcd(display), _state(state), _config(config) {}

void RemoteTermUI::begin() {
  _lcd.init();
  _lcd.setTouchRotation(0);
  _lcd.setRotation(_config.displayRotation);
  _lcd.setBrightness(190);
  _lcd.fillScreen(C_BG);
  _lcd.setTextDatum(lgfx::top_left);
  _lcd.setFont(&fonts::Font0);
  _lastInteractionAt = millis();
}

uint32_t RemoteTermUI::stateHash() const {
  uint32_t h = 2166136261u;
  auto mix = [&h](uint32_t v) { h ^= v; h *= 16777619u; };
  mix(_state.channelCount);
  mix(_state.selectedChannel);
  mix(_state.messageCount);
  mix(_state.wifiConnected);
  mix(_state.wsConnected);
  mix(_state.apiHealthy);
  mix(_settingsMode);
  mix(_config.selectedChannelCount);
  mix(_clockMode);
  mix(_state.timeValid);
  mix(_state.timeEpoch);
  if (_state.messageCount) {
    mix((uint32_t)_state.messages[_state.messageCount - 1].id);
    mix(_state.messages[_state.messageCount - 1].text.length());
  }
  return h;
}

void RemoteTermUI::drawClock() {
  const int w = _lcd.width();
  const int h = _lcd.height();
  _lcd.fillRect(0, 47, w, h - 89, C_BG);
  _lcd.setTextDatum(lgfx::middle_center);
  if (_state.timeValid) {
    struct tm now;
    time_t epoch = static_cast<time_t>(_state.timeEpoch);
    localtime_r(&epoch, &now);
    char timeText[8];
    strftime(timeText, sizeof(timeText), "%H:%M", &now);
    _lcd.setTextSize(w >= 400 ? 5 : 3);
    _lcd.setTextColor(C_ACCENT, C_BG);
    _lcd.drawString(timeText, w / 2, h / 2 - 12);
    char dateText[32];
    strftime(dateText, sizeof(dateText), "%a %d %b %Y", &now);
    _lcd.setTextSize(1);
    _lcd.setTextColor(C_MUTED, C_BG);
    _lcd.drawString(dateText, w / 2, h / 2 + 28);
  } else {
    _lcd.setTextSize(2);
    _lcd.setTextColor(C_MUTED, C_BG);
    _lcd.drawString(WiFi.status() == WL_CONNECTED ? "SYNCING TIME" : "WAITING FOR WI-FI", w / 2, h / 2);
  }
  _lcd.setTextDatum(lgfx::top_left);
}

void RemoteTermUI::showClockMode() {
  _clockMode = true;
  _messageScroll = 0;
  render(true);
}

String RemoteTermUI::shortTime(const String& iso) const {
  int t = iso.indexOf('T');
  if (t >= 0 && iso.length() >= (size_t)(t + 6)) return iso.substring(t + 1, t + 6);
  if (iso.length() >= 5) return iso.substring(max(0, (int)iso.length() - 8), max(0, (int)iso.length() - 3));
  return iso;
}

void RemoteTermUI::drawButton(int x, int y, int width, int height, const char* label, bool accent) {
  _lcd.fillRoundRect(x, y, width, height, 4, accent ? C_ACCENT : C_BG);
  _lcd.drawRoundRect(x, y, width, height, 4, C_MUTED);
  _lcd.setTextDatum(lgfx::middle_center);
  _lcd.setTextSize(1);
  _lcd.setTextColor(accent ? C_BG : C_TEXT, accent ? C_ACCENT : C_BG);
  _lcd.drawString(label, x + width / 2, y + height / 2);
  _lcd.setTextDatum(lgfx::top_left);
}

void RemoteTermUI::drawHeader() {
  int w = _lcd.width();
  _lcd.fillRect(0, 0, w, 42, C_PANEL);
  _lcd.fillRect(0, 40, w, 2, C_ACCENT);
  _lcd.setTextSize(w >= 400 ? 2 : 1);
  _lcd.setTextColor(C_TEXT, C_PANEL);
  _lcd.setCursor(10, w >= 400 ? 10 : 8);
  _lcd.print("MESHCORE REMOTETERM");

  String state = _state.wsConnected ? "LIVE" : (_state.wifiConnected ? "POLL" : "WIFI");
  if (_state.timeValid) {
    time_t epoch = static_cast<time_t>(_state.timeEpoch);
    struct tm now;
    localtime_r(&epoch, &now);
    char clockText[6];
    strftime(clockText, sizeof(clockText), "%H:%M", &now);
    _lcd.setTextColor(C_ACCENT, C_PANEL);
    _lcd.setTextSize(1);
    _lcd.setCursor(w >= 400 ? w / 2 - 15 : w / 2 - 12, w >= 400 ? 15 : 11);
    _lcd.print(clockText);
  }
  _lcd.setTextColor(_state.wsConnected ? C_ACCENT : C_WARN, C_PANEL);
  int tw = _lcd.textWidth(state);
  _lcd.setCursor(w - tw - 10, w >= 400 ? 10 : 8);
  _lcd.print(state);
}

void RemoteTermUI::drawWrapped(const String& text, int x, int& y, int width, int maxLines, uint16_t color, float fontSize) {
  _lcd.setTextSize(fontSize);
  _lcd.setTextColor(color, C_BG);
  String remaining = text;
  for (int line = 0; line < maxLines && remaining.length(); ++line) {
    int cut = remaining.length();
    while (cut > 1 && _lcd.textWidth(remaining.substring(0, cut)) > width) --cut;
    if (cut < (int)remaining.length()) {
      int space = remaining.lastIndexOf(' ', cut);
      if (space > 4) cut = space;
    }
    String piece = remaining.substring(0, cut);
    piece.trim();
    _lcd.setCursor(x, y);
    _lcd.print(piece);
    remaining = remaining.substring(cut);
    remaining.trim();
    y += 15;
  }
}

void RemoteTermUI::drawMessages() {
  int w = _lcd.width();
  int h = _lcd.height();
  int top = 47;
  int bottom = h - 42;
  _lcd.fillRect(0, top, w, bottom - top, C_BG);

  if (_state.channelCount == 0) {
    _lcd.setTextSize(2);
    _lcd.setTextColor(C_MUTED, C_BG);
    _lcd.setCursor(12, top + 18);
    _lcd.print(_state.wifiConnected ? "No channels found" : "Connecting to Wi-Fi...");
    return;
  }

  if (_state.messageCount == 0) {
    _lcd.setTextSize(2);
    _lcd.setTextColor(C_MUTED, C_BG);
    _lcd.setCursor(12, top + 18);
    _lcd.print("No messages");
    return;
  }

  // Work backwards from newest messages, estimating each card height, then
  // render the subset that fits from top to bottom.
  int available = bottom - top - 4;
  int start = (int)_state.messageCount - 1 - _messageScroll;
  if (start < 0) start = 0;
  int used = 0;
  for (; start >= 0; --start) {
    const MessageInfo& m = _state.messages[start];
    int estimated = 39 + min(3, max(1, (int)m.text.length() / max(18, w / 8))) * 15;
    if (used + estimated > available && start < (int)_state.messageCount - 1) break;
    used += estimated;
  }
  start++;

  int y = top + 6;
  for (size_t i = start; i < _state.messageCount && y < bottom - 20; ++i) {
    const MessageInfo& m = _state.messages[i];
    String sender = m.sender.length() ? m.sender : (m.outgoing ? "You" : "MeshCore");
    String tm = shortTime(m.receivedAt);
    int cardHeight = 38 + min(3, max(1, (int)m.text.length() / max(18, w / 8))) * 15;
    if (y + cardHeight > bottom) break;
    _lcd.fillRoundRect(5, y, w - 10, cardHeight - 4, 6, C_PANEL);
    _lcd.fillRect(5, y + 7, 3, cardHeight - 18, m.outgoing ? C_ACCENT : C_MUTED);

    _lcd.setTextSize(1);
    _lcd.setTextColor(m.outgoing ? C_ACCENT : C_TEXT, C_PANEL);
    _lcd.setCursor(15, y + 7);
    _lcd.print(sender);
    if (tm.length()) {
      _lcd.setTextColor(C_MUTED, C_PANEL);
      int tw = _lcd.textWidth(tm);
      _lcd.setCursor(w - tw - 14, y + 7);
      _lcd.print(tm);
    }
    y += 20;
    drawWrapped(m.text, 15, y, w - 25, 3, C_TEXT, 1);
    y += cardHeight - 39;
    y += 6;
  }
}

void RemoteTermUI::drawFooter() {
  int w = _lcd.width();
  int h = _lcd.height();
  _lcd.fillRect(0, h - 40, w, 40, C_PANEL);
  _lcd.fillRect(0, h - 40, w, 2, C_ACCENT);
  const int y = h - 34;
  const int bh = 26;
  const int bw = w >= 400 ? 42 : 34;
  const int gap = 4;
  drawButton(6, y, bw, bh, "SET");
  drawButton(6 + bw + gap, y, bw, bh, "UP");
  drawButton(6 + (bw + gap) * 2, y, bw, bh, "DOWN");
  drawButton(w - bw * 2 - gap - 6, y, bw, bh, "PREV");
  drawButton(w - bw - 6, y, bw, bh, "NEXT", true);

  String name = "--";
  if (_state.channelCount && _state.selectedChannel >= 0 && _state.selectedChannel < (int)_state.channelCount)
    name = _state.channels[_state.selectedChannel].name;
  if (!name.startsWith("#") && name != "--") name = "#" + name;

  _lcd.setTextColor(C_TEXT, C_PANEL);
  int tw = _lcd.textWidth(name);
  _lcd.setCursor(max(38, (w - tw) / 2), h - 28);
  _lcd.print(name);
}

void RemoteTermUI::drawSettings() {
  const int w = _lcd.width();
  const int h = _lcd.height();
  _lcd.fillScreen(C_BG);
  _lcd.fillRect(0, 0, w, 38, C_PANEL);
  _lcd.setTextSize(2);
  _lcd.setTextColor(C_ACCENT, C_PANEL);
  _lcd.setCursor(10, 10);
  _lcd.print("CHANNEL SETTINGS");
  _lcd.setTextSize(1);
  _lcd.setTextColor(C_MUTED, C_BG);
  _lcd.setCursor(10, 46);
  _lcd.print("Tap channels to show or hide them");
  const size_t count = min(_config.cachedChannelCount, static_cast<size_t>((h - 82) / 25));
  for (size_t i = 0; i < count; ++i) {
    const int y = 58 + static_cast<int>(i) * 25;
    _lcd.drawRect(10, y, 18, 18, C_MUTED);
    if (_config.channelSelectionConfigured && _config.channelSelected(_config.cachedChannels[i].key))
      _lcd.fillRect(14, y + 4, 10, 10, C_ACCENT);
    _lcd.setTextColor(C_TEXT, C_BG);
    _lcd.setCursor(38, y + 4);
    _lcd.print(_config.cachedChannels[i].name);
  }
  if (!count) {
    _lcd.setTextColor(C_MUTED, C_BG);
    _lcd.setCursor(10, 75);
    _lcd.print("No cached channels yet");
  }
  _lcd.fillRect(w - 100, h - 34, 92, 26, C_PANEL);
  _lcd.setTextColor(C_ACCENT, C_PANEL);
  _lcd.setCursor(w - 88, h - 26);
  _lcd.print("SAVE");
}

void RemoteTermUI::render(bool force) {
  if (!_clockMode && _state.messageCount && _lastInteractionAt &&
      millis() - _lastInteractionAt >= MESSAGE_VIEW_IDLE_MS) {
    _clockMode = true;
    _messageScroll = 0;
    force = true;
  }
  uint32_t h = stateHash();
  if (!force && h == _lastRenderHash) return;
  _lastRenderHash = h;
  if (_settingsMode) {
    drawSettings();
    return;
  }
  drawHeader();
  if (_state.messageCount == 0 || _clockMode) drawClock();
  else drawMessages();
  drawFooter();
}

int RemoteTermUI::pollChannelGesture() {
  uint16_t x, y;
  bool touched = _lcd.getTouch(&x, &y);
  if (touched && !_touchDown) {
    _touchDown = true;
    _touchStartX = x;
    _touchStartY = y;
    _lastTouchX = x;
    _lastTouchY = y;
    _touchStartAt = millis();
    _lastInteractionAt = millis();
    return 0;
  }
  if (touched) {
    _lastTouchX = x;
    _lastTouchY = y;
    return 0;
  }
  if (!_touchDown) return 0;

  _touchDown = false;
  x = _lastTouchX;
  y = _lastTouchY;
  if (_settingsMode) {
    const int x = _touchStartX;
    const int y = _touchStartY;
    if (x >= _lcd.width() - 110 && y >= _lcd.height() - 45) {
      _settingsMode = false;
      return 3;
    }
    const int index = (y - 58) / 25;
    if (y >= 58 && index >= 0 && index < static_cast<int>(_config.cachedChannelCount)) {
      const String& key = _config.cachedChannels[index].key;
      size_t found = _config.selectedChannelCount;
      for (size_t i = 0; i < _config.selectedChannelCount; ++i)
        if (_config.selectedChannelKeys[i] == key) { found = i; break; }
      if (found < _config.selectedChannelCount) {
        for (size_t i = found + 1; i < _config.selectedChannelCount; ++i)
          _config.selectedChannelKeys[i - 1] = _config.selectedChannelKeys[i];
        --_config.selectedChannelCount;
      } else if (_config.selectedChannelCount < MAX_SELECTED_CHANNELS) {
        _config.selectedChannelKeys[_config.selectedChannelCount++] = key;
      }
      _config.channelSelectionConfigured = true;
      render(true);
    }
    return 0;
  }
  if (_clockMode && _touchStartY >= 47 && _touchStartY < _lcd.height() - 34) {
    _clockMode = false;
    _lastInteractionAt = millis();
    render(true);
    return 0;
  }
  const int footerY = _lcd.height() - 34;
  const int buttonHeight = 26;
  const int buttonWidth = _lcd.width() >= 400 ? 42 : 34;
  const int buttonGap = 4;
  if (_touchStartY >= footerY && _touchStartY < footerY + buttonHeight) {
    if (_touchStartX >= 6 && _touchStartX < 6 + buttonWidth) {
      _settingsMode = true;
      render(true);
      return 2;
    }
    if (_touchStartX >= 6 + buttonWidth + buttonGap && _touchStartX < 6 + (buttonWidth + buttonGap) * 2) {
      _messageScroll = min(max(0, _messageScroll + 1), max(0, static_cast<int>(_state.messageCount) - 1));
      render(true);
      return 4;
    }
    if (_touchStartX >= 6 + (buttonWidth + buttonGap) * 2 && _touchStartX < 6 + (buttonWidth + buttonGap) * 3) {
      _messageScroll = max(0, _messageScroll - 1);
      render(true);
      return 5;
    }
    if (_touchStartX >= _lcd.width() - buttonWidth * 2 - buttonGap - 6 && _touchStartX < _lcd.width() - buttonWidth - buttonGap - 6) return -1;
    if (_touchStartX >= _lcd.width() - buttonWidth - 6) return +1;
    return 0;
  }

  int dx = (int)x - _touchStartX;
  int dy = (int)y - _touchStartY;
  unsigned long held = millis() - _touchStartAt;
  if (held > 1200) return 0;

  // Swipe only inside the message viewport; footer actions are button-only.
  if (_touchStartY >= 47 && _touchStartY < footerY && abs(dy) > 35 && abs(dy) > abs(dx) && _state.messageCount) {
    _messageScroll += dy < 0 ? 1 : -1;
    _messageScroll = min(max(0, _messageScroll), max(0, static_cast<int>(_state.messageCount) - 1));
    render(true);
    return 0;
  }
  // Channel navigation is deliberately button-only. No broad horizontal
  // swipe region is allowed to change the selected channel.
  return 0;
}
