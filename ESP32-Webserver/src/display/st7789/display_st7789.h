#pragma once

#include "display/display_config.h"
#include "pins.h"

#if DISPLAY_TYPE == DISPLAY_TYPE_ST7789
#include <LovyanGFX.hpp>

#ifndef TFT_SCK  
    #define TFT_SCK SPI_SCK
#endif

#ifndef TFT_MOSI
    #define TFT_MOSI SPI_MOSI
#endif

#ifndef TFT_MISO
    #define TFT_MISO SPI_MISO
#endif


class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel;
    lgfx::Bus_SPI _bus;

public:

    LGFX() {
        auto bus_cfg = _bus.config();
    #if defined(CONFIG_IDF_TARGET_ESP32S3)
        bus_cfg.spi_host = SPI2_HOST;
    #else
    bus_cfg.spi_host = VSPI_HOST;
    #endif
        bus_cfg.freq_write = 40000000;
        bus_cfg.freq_read  = 16000000;
        bus_cfg.spi_mode = 0;
        bus_cfg.pin_sclk = TFT_SCK;
        bus_cfg.pin_mosi = TFT_MOSI;
        bus_cfg.pin_miso = TFT_MISO;
        bus_cfg.pin_dc   = TFT_DC;
        _bus.config(bus_cfg);

        auto panel_cfg = _panel.config();
        panel_cfg.pin_cs    = TFT_CS;
        panel_cfg.pin_rst   = TFT_RST;
        panel_cfg.pin_busy  = -1;
        panel_cfg.panel_width = 170;
        panel_cfg.panel_height = 320;
        panel_cfg.offset_x = 35;
        panel_cfg.offset_y = 0;
        panel_cfg.readable = false;
        panel_cfg.invert   = true;
        panel_cfg.rgb_order = false;
      #if defined(CONFIG_IDF_TARGET_ESP32S3)
        panel_cfg.bus_shared = false;
      #else
        panel_cfg.bus_shared = true;
      #endif  
        _panel.config(panel_cfg);

        _panel.setBus(&_bus);
        setPanel(&_panel);
    }
};



#endif