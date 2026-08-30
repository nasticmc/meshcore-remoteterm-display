#include "ui.h"
#include <Arduino.h>

namespace {
constexpr uint16_t C_BG = 0x0841;
constexpr uint16_t C_PANEL = 0x10A2;
constexpr uint16_t C_TEXT = 0xFFFF;
constexpr uint16_t C_MUTED = 0xA514;
constexpr uint16_t C_ACCENT = 0x07E0;
constexpr uint16_t C_WARN = 0xFD20;
}

RemoteTermUI::RemoteTermUI(RemoteTermDisplay& display, AppState& state) : _lcd(display), _state(state) {}

void RemoteTermUI::begin() {
  _lcd.init();
  _lcd.setRotation(1);
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
  _lcd.fillRect(0, 0, w, 36, C_PANEL);
  _lcd.setTextSize(2);
  _lcd.setTextColor(C_TEXT, C_PANEL);
  _lcd.setCursor(8, 9);
  _lcd.print("MeshCore RemoteTerm Display");

  String state = _state.wsConnected ? "LIVE" : (_state.wifiConnected ? "POLL" : "WIFI");
  _lcd.setTextColor(_state.wsConnected ? C_ACCENT : C_WARN, C_PANEL);
  int tw = _lcd.textWidth(state);
  _lcd.setCursor(w - tw - 8, 9);
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
  int top = 39;
  int bottom = h - 39;
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

  int y = top + 4;
  for (size_t i = start; i < _state.messageCount && y < bottom - 20; ++i) {
    const MessageInfo& m = _state.messages[i];
    String sender = m.sender.length() ? m.sender : (m.outgoing ? "You" : "MeshCore");
    String tm = shortTime(m.receivedAt);

    _lcd.setTextSize(1);
    _lcd.setTextColor(m.outgoing ? C_ACCENT : C_TEXT, C_BG);
    _lcd.setCursor(8, y);
    _lcd.print(sender);
    if (tm.length()) {
      _lcd.setTextColor(C_MUTED, C_BG);
      int tw = _lcd.textWidth(tm);
      _lcd.setCursor(w - tw - 8, y);
      _lcd.print(tm);
    }
    y += 13;
    drawWrapped(m.text, 8, y, w - 16, 3, C_TEXT, 1);
    y += 5;
    _lcd.drawFastHLine(8, y, w - 16, C_PANEL);
    y += 5;
  }
}

void RemoteTermUI::drawFooter() {
  int w = _lcd.width();
  int h = _lcd.height();
  _lcd.fillRect(0, h - 36, w, 36, C_PANEL);
  _lcd.setTextSize(2);
  _lcd.setTextColor(C_MUTED, C_PANEL);
  _lcd.setCursor(8, h - 27);
  _lcd.print("<");
  _lcd.setCursor(w - 18, h - 27);
  _lcd.print(">");

  String name = "--";
  if (_state.channelCount && _state.selectedChannel >= 0 && _state.selectedChannel < (int)_state.channelCount)
    name = _state.channels[_state.selectedChannel].name;
  if (!name.startsWith("#") && name != "--") name = "#" + name;

  _lcd.setTextColor(C_TEXT, C_PANEL);
  int tw = _lcd.textWidth(name);
  _lcd.setCursor(max(28, (w - tw) / 2), h - 27);
  _lcd.print(name);
}

void RemoteTermUI::render(bool force) {
  uint32_t h = stateHash();
  if (!force && h == _lastRenderHash) return;
  _lastRenderHash = h;
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
