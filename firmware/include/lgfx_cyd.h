#pragma once

#include <LovyanGFX.hpp>

class LGFX_CYD : public lgfx::LGFX_Device {
 public:
  LGFX_CYD() {
    {
      auto config = bus_.config();
      config.spi_host = SPI2_HOST;
      config.spi_mode = 0;
      config.freq_write = 27000000;
      config.freq_read = 16000000;
      config.spi_3wire = true;
      config.use_lock = true;
      config.dma_channel = SPI_DMA_CH_AUTO;
      config.pin_sclk = 14;
      config.pin_mosi = 13;
      config.pin_miso = -1;
      config.pin_dc = 2;
      bus_.config(config);
      panel_.setBus(&bus_);
    }

    {
      auto config = panel_.config();
      config.pin_cs = 15;
      config.pin_rst = 12;
      config.pin_busy = -1;
      config.memory_width = 240;
      config.memory_height = 320;
      config.panel_width = 240;
      config.panel_height = 320;
      config.offset_x = 0;
      config.offset_y = 0;
      config.offset_rotation = 6;
      config.invert = true;
      config.rgb_order = false;
      config.readable = false;
      panel_.config(config);
    }

    setPanel(&panel_);
  }

 private:
  lgfx::Bus_SPI bus_;
  lgfx::Panel_ILI9341_2 panel_;
};
