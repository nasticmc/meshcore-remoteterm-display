#pragma once
#include <LovyanGFX.hpp>

#ifndef REMOTETERM_DISPLAY_PROFILE
#define REMOTETERM_DISPLAY_PROFILE 1042
#endif

// Common FNK0104 ESP32-S3 wiring.
static constexpr int PIN_LCD_SCLK = 12;
static constexpr int PIN_LCD_MOSI = 11;
static constexpr int PIN_LCD_MISO = 13;
static constexpr int PIN_LCD_CS   = 10;
static constexpr int PIN_LCD_DC   = 46;
static constexpr int PIN_LCD_RST  = -1;
static constexpr int PIN_LCD_BL   = 45;

static constexpr int PIN_TOUCH_SDA = 16;
static constexpr int PIN_TOUCH_SCL = 15;
static constexpr int PIN_TOUCH_INT = 17;
static constexpr int PIN_TOUCH_RST = 18;

class RemoteTermDisplay : public lgfx::LGFX_Device {
  lgfx::Bus_SPI _bus;
#if REMOTETERM_DISPLAY_PROFILE == 1044
  lgfx::Panel_ST7796 _panel;
#else
  lgfx::Panel_ILI9341 _panel;
#endif
  lgfx::Light_PWM _light;
  lgfx::Touch_FT5x06 _touch;

 public:
  RemoteTermDisplay() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 80000000;
      cfg.freq_read = 20000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = PIN_LCD_SCLK;
      cfg.pin_mosi = PIN_LCD_MOSI;
      cfg.pin_miso = PIN_LCD_MISO;
      cfg.pin_dc = PIN_LCD_DC;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs = PIN_LCD_CS;
      cfg.pin_rst = PIN_LCD_RST;
      cfg.pin_busy = -1;
#if REMOTETERM_DISPLAY_PROFILE == 1044
      cfg.panel_width = 320;
      cfg.panel_height = 480;
      cfg.memory_width = 320;
      cfg.memory_height = 480;
#else
      cfg.panel_width = 240;
      cfg.panel_height = 320;
      cfg.memory_width = 240;
      cfg.memory_height = 320;
#endif
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.readable = true;
      cfg.invert = true;
      // The Freenove 4-inch ST7796 setup is RGB; the 2.8-inch ILI9341 is BGR.
      cfg.rgb_order = REMOTETERM_DISPLAY_PROFILE == 1044;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel.config(cfg);
    }
    {
      auto cfg = _light.config();
      cfg.pin_bl = PIN_LCD_BL;
      cfg.invert = false;
      cfg.freq = 12000;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    {
      auto cfg = _touch.config();
      cfg.x_min = 0;
      cfg.y_min = 0;
#if REMOTETERM_DISPLAY_PROFILE == 1044
      cfg.x_max = 319;
      cfg.y_max = 479;
#else
      cfg.x_max = 239;
      cfg.y_max = 319;
#endif
      cfg.pin_int = PIN_TOUCH_INT;
      cfg.bus_shared = false;
      cfg.offset_rotation = 0;
      cfg.i2c_port = 0;
      cfg.i2c_addr = 0x38;
      cfg.pin_sda = PIN_TOUCH_SDA;
      cfg.pin_scl = PIN_TOUCH_SCL;
      cfg.freq = 400000;
      _touch.config(cfg);
      _panel.setTouch(&_touch);
    }
    setPanel(&_panel);
  }
};
