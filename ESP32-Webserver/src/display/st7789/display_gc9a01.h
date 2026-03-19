#pragma once

#include "display/display_config.h"

#if DISPLAY_TYPE == DISPLAY_TYPE_ST7789


#include <LovyanGFX.hpp>


class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel;
    lgfx::Bus_SPI _bus;

public:
    LGFX() {
        auto bus_cfg = _bus.config();
        bus_cfg.spi_host = SPI2_HOST;
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
        panel_cfg.bus_shared = false;
        _panel.config(panel_cfg);

        _panel.setBus(&_bus);
        setPanel(&_panel);
    }
};



#endif