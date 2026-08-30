#include "ui.h"
#include <Arduino.h>

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
  _lcd.setRotation(_config.displayRotation);
  _lcd.setBrightness(190);
  _lcd.fillScreen(C_BG);
  _lcd.setTextDatum(lgfx::top_left);
  _lcd.setFont(&fonts::Font0);
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
  if (_state.messageCount) {
    mix((uint32_t)_state.messages[_state.messageCount - 1].id);
    mix(_state.messages[_state.messageCount - 1].text.length());
  }
  return h;
}

String RemoteTermUI::shortTime(const String& iso) const {
  int t = iso.indexOf('T');
  if (t >= 0 && iso.length() >= (size_t)(t + 6)) return iso.substring(t + 1, t + 6);
  if (iso.length() >= 5) return iso.substring(max(0, (int)iso.length() - 8), max(0, (int)iso.length() - 3));
  return iso;
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
  int start = (int)_state.messageCount - 1;
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
  _lcd.fillCircle(17, h - 20, 8, C_MUTED);
  _lcd.fillCircle(17, h - 20, 3, C_PANEL);
  _lcd.fillRect(15, h - 33, 5, 5, C_MUTED);
  _lcd.fillRect(15, h - 12, 5, 5, C_MUTED);
  _lcd.fillRect(4, h - 22, 5, 5, C_MUTED);
  _lcd.fillRect(25, h - 22, 5, 5, C_MUTED);
  _lcd.setTextSize(w >= 400 ? 2 : 1);
  _lcd.setTextColor(C_MUTED, C_PANEL);
  _lcd.setCursor(32, h - 28);
  _lcd.print("<");
  _lcd.setCursor(w - (w >= 400 ? 18 : 14), h - 28);
  _lcd.print(">");

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
  uint32_t h = stateHash();
  if (!force && h == _lastRenderHash) return;
  _lastRenderHash = h;
  if (_settingsMode) {
    drawSettings();
    return;
  }
  drawHeader();
  drawMessages();
  drawFooter();
}

int RemoteTermUI::pollChannelGesture() {
  uint16_t x, y;
  bool touched = _lcd.getTouch(&x, &y);
  if (touched && !_touchDown) {
    _touchDown = true;
    _touchStartX = x;
    _touchStartY = y;
    _touchStartAt = millis();
    return 0;
  }
  if (touched || !_touchDown) return 0;

  _touchDown = false;
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
  if (_touchStartX < 34 && _touchStartY >= _lcd.height() - 50) {
    _settingsMode = true;
    render(true);
    return 2;
  }
  int dx = (int)x - _touchStartX;
  unsigned long held = millis() - _touchStartAt;
  if (held > 1200) return 0;

  // Swipe anywhere, or tap left/right footer thirds.
  if (abs(dx) > 45) return dx < 0 ? +1 : -1;
  if (_touchStartY >= _lcd.height() - 55) {
    if (_touchStartX < _lcd.width() / 3) return -1;
    if (_touchStartX > (_lcd.width() * 2) / 3) return +1;
  }
  return 0;
}
