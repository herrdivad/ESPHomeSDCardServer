# Freenove ESP32-S3 Display Pinout for ESPHome

This document extracts the ESP32-S3 GPIO assignments from the Freenove documentation repository and groups them by the pins/components that usually need to be declared in ESPHome YAML.

## Board Variants

| Freenove model | Size | Display driver | Touch controller | Notes |
|---|---:|---|---|---|
| `FNK0104A` | 2.8 inch | ILI9341 | none | Same 2.8 inch display family, no touch. |
| `FNK0104B` / `FNK0104AB` | 2.8 inch | ILI9341 / `ILI9341_2_DRIVER` | FT6336U, I2C address `0x38` | The sketches use the macro `FNK0104AB_2P8_240x320_ILI9341`. |
| `FNK0104N` | 3.5 inch | ST77922, QSPI | ST77922 touch, I2C address `0x55` | Uses a different SDMMC/audio/I2C pin set from the other boards. |
| `FNK0104S` | 4.0 inch | ST7796 | FT6336U, I2C address `0x38` | The README says `ST7789`, but the sketches, TFT_eSPI setup, datasheet set, and schematic use `ST7796`. |

## Common Pins

| Function | ESPHome use | `FNK0104AB` / `FNK0104B` / `FNK0104S` | `FNK0104N` | Notes |
|---|---|---:|---:|---|
| Boot button | `binary_sensor.gpio` | GPIO0 | GPIO0 | Active low, use `INPUT_PULLUP`. Also boot/download strap. |
| Reset button | hardware reset | `CHIP_PU` / reset net | `CHIP_PU` / reset net | Not a normal ESPHome GPIO. |
| USB D- | USB CDC/programming | GPIO19 | GPIO19 | On-board USB-C. |
| USB D+ | USB CDC/programming | GPIO20 | GPIO20 | On-board USB-C. |
| RGB LED data | `light: esp32_rmt_led_strip` / NeoPixel | GPIO42 | GPIO40 | One WS2812B-compatible LED, GRB order. |
| Battery ADC | `sensor: adc` | GPIO9 | GPIO8 | Divider is 2:1 in example code: voltage = ADC millivolts * 2. |
| Audio amplifier enable | `output.gpio` / custom audio enable | GPIO1 | GPIO1 | Named `AUDIO_EN` / `AP_ENABLE`; example code drives it low during init. |

## External Connectors

### UART Header `P2`

| Header pin | Signal | ESP32-S3 GPIO | ESPHome key |
|---:|---|---:|---|
| 1 | `+5` | - | Power rail |
| 2 | `GND` | - | Ground |
| 3 | `TXD0` | GPIO43 | `uart.tx_pin` |
| 4 | `RXD0` | GPIO44 | `uart.rx_pin` |

The tutorial states that with USB CDC enabled, the USB port is `Serial` and the hardware UART should be used as `Serial1` in Arduino terms.

### I2C Header `P4`

| Board | Header signal | ESP32-S3 GPIO | ESPHome key | Shared with |
|---|---|---:|---|---|
| `FNK0104AB` / `FNK0104B` / `FNK0104S` | `IIC_SDA` | GPIO16 | `i2c.sda` | FT6336U touch, ES8311 codec |
| `FNK0104AB` / `FNK0104B` / `FNK0104S` | `IIC_SCL` | GPIO15 | `i2c.scl` | FT6336U touch, ES8311 codec |
| `FNK0104N` | `IIC_SDA` | GPIO38 | `i2c.sda` | ST77922 touch, ES8311 codec |
| `FNK0104N` | `IIC_SCL` | GPIO39 | `i2c.scl` | ST77922 touch, ES8311 codec |

Header power pins are `VCC3V3` and `GND`.

### Extended IO Header `P3`

| Board | Header signals | ESP32-S3 GPIOs | Notes |
|---|---|---|---|
| `FNK0104AB` / `FNK0104B` / `FNK0104S` | `IO2`, `IO3`, `IO14`, `IO21` | GPIO2, GPIO3, GPIO14, GPIO21 | These are the main exposed DIO pins on the 2.8/4.0 inch schematic. |
| `FNK0104N` | `IO45`, `IO46` | GPIO45, GPIO46 | These are the exposed DIO pins on the 3.5 inch schematic. Treat boot-strapping/high-number S3 pins carefully during boot. |

## Display Pins

### 2.8 inch `FNK0104AB` / `FNK0104B`: ILI9341 SPI

| Display signal | ESP32-S3 GPIO | ESPHome-style key |
|---|---:|---|
| Driver | - | ILI9341-compatible |
| Width x height | - | `240x320` native |
| SPI MOSI / `LCD_MOSI` | GPIO11 | `spi.mosi_pin` |
| SPI MISO / `LCD_MISO` | GPIO13 | `spi.miso_pin` |
| SPI CLK / `LCD_SCK` | GPIO12 | `spi.clk_pin` |
| Chip select / `LCD_CS` | GPIO10 | `cs_pin` |
| Data/command / `LCD_RS` | GPIO46 | `dc_pin` |
| Reset | `-1` / board reset net | omit or use no reset pin |
| Backlight / `LCD_BL` | GPIO45 | `backlight_pin` / PWM output |
| Backlight active level | HIGH | `inverted: false` |
| TFT_eSPI frequency | 40 MHz | SPI data rate reference |
| TFT_eSPI color settings | `TFT_BGR`, inversion on | Use if the ESPHome driver exposes these options. |

### 4.0 inch `FNK0104S`: ST7796 SPI

| Display signal | ESP32-S3 GPIO | ESPHome-style key |
|---|---:|---|
| Driver | - | ST7796-compatible |
| Width x height | - | `320x480` native |
| SPI MOSI / `LCD_MOSI` | GPIO11 | `spi.mosi_pin` |
| SPI MISO / `LCD_MISO` | GPIO13 | `spi.miso_pin` if needed; TFT_eSPI setup does not define MISO |
| SPI CLK / `LCD_SCK` | GPIO12 | `spi.clk_pin` |
| Chip select / `LCD_CS` | GPIO10 | `cs_pin` |
| Data/command / `LCD_RS` | GPIO46 | `dc_pin` |
| Reset | `-1` / board reset net | omit or use no reset pin |
| Backlight / `LCD_BL` | GPIO45 | `backlight_pin` / PWM output |
| Backlight active level | HIGH | `inverted: false` |
| TFT_eSPI frequency | 80 MHz | SPI data rate reference |
| TFT_eSPI color settings | inversion on | Use if the ESPHome driver exposes this option. |

### 3.5 inch `FNK0104N`: ST77922 QSPI

| Display signal | ESP32-S3 GPIO | ESPHome-style key |
|---|---:|---|
| Driver | - | ST77922 custom/QSPI |
| Width x height | - | `320x480` native |
| QSPI host | `SPI2_HOST` | ESP-IDF bus reference |
| QSPI SCLK / `LCD_SCK` | GPIO12 | `clk_pin` |
| QSPI D0 / `LCD_SDA0` | GPIO11 | `data0` |
| QSPI D1 / `LCD_SDA1` | GPIO13 | `data1` |
| QSPI D2 / `LCD_SDA2` | GPIO14 | `data2` |
| QSPI D3 / `LCD_SDA3` | GPIO9 | `data3` |
| Chip select / `LCD_CS` | GPIO10 | `cs_pin` |
| Tearing effect / `LCD_TE` | GPIO42 | optional TE/input |
| Backlight / `LCD_BL` | GPIO41 | `backlight_pin` / PWM output |
| QSPI frequency | 80 MHz | Driver reference |

The repository uses a custom `ST77922` class for this panel. Confirm that the ESPHome display component you plan to use supports this ST77922 QSPI wiring before assuming a stock SPI display config will work.

## Touch Pins

| Board | Touch IC | I2C address | SDA | SCL | Reset | Interrupt | Notes |
|---|---|---:|---:|---:|---:|---:|---|
| `FNK0104B` / `FNK0104AB` | FT6336U | `0x38` | GPIO16 | GPIO15 | GPIO18 | GPIO17 | Capacitive touch. |
| `FNK0104S` | FT6336U | `0x38` | GPIO16 | GPIO15 | GPIO18 | GPIO17 | Capacitive touch. |
| `FNK0104N` | ST77922 touch controller | `0x55` | GPIO38 | GPIO39 | GPIO48 | GPIO47 | Up to 10 touch points in the bundled driver. |

## SD Card Pins

The SD slot is wired for 4-bit SDMMC.

| Board | `clk_pin` | `cmd_pin` | `data0_pin` | `data1_pin` | `data2_pin` | `data3_pin` |
|---|---:|---:|---:|---:|---:|---:|
| `FNK0104AB` / `FNK0104B` / `FNK0104S` | GPIO38 | GPIO40 | GPIO39 | GPIO41 | GPIO48 | GPIO47 |
| `FNK0104N` | GPIO5 | GPIO4 | GPIO6 | GPIO7 | GPIO2 | GPIO3 |

## Audio, Microphone, and Speaker

The board uses an ES8311 audio codec at I2C address `0x18`, an on-board analog MEMS microphone routed into the codec, and a small speaker connector `JP3` driven through an audio power amplifier.

| Board | ES8311/I2C SDA | ES8311/I2C SCL | I2S MCLK | I2S BCLK/SCK | I2S LRCLK/WS | I2S DOUT to codec/speaker | I2S DIN from codec/mic | Amp enable |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `FNK0104AB` / `FNK0104B` / `FNK0104S` | GPIO16 | GPIO15 | GPIO4 | GPIO5 | GPIO7 | GPIO8 | GPIO6 | GPIO1 |
| `FNK0104N` | GPIO38 | GPIO39 | GPIO17 | GPIO18 | GPIO21 | GPIO15 | GPIO16 | GPIO1 |

Speaker connector `JP3` exposes `SP+` and `SP-`. The microphone is not connected directly to an ESP32 ADC/I2S pin; it is connected to ES8311 analog inputs `MIC_P` and `MIC_N`.

## GPIO Allocation Summary

### `FNK0104AB` / `FNK0104B` / `FNK0104S`

| GPIO | Signal/function |
|---:|---|
| GPIO0 | Boot button, active low |
| GPIO1 | Audio amplifier enable, `AUDIO_EN` |
| GPIO2 | External `P3` `IO2` |
| GPIO3 | External `P3` `IO3` |
| GPIO4 | I2S MCLK |
| GPIO5 | I2S BCLK/SCK |
| GPIO6 | I2S DIN from ES8311/microphone |
| GPIO7 | I2S LRCLK/WS |
| GPIO8 | I2S DOUT to ES8311/speaker |
| GPIO9 | Battery ADC |
| GPIO10 | Display `LCD_CS` |
| GPIO11 | Display `LCD_MOSI` |
| GPIO12 | Display `LCD_SCK` |
| GPIO13 | Display `LCD_MISO` |
| GPIO14 | External `P3` `IO14` |
| GPIO15 | Shared I2C SCL: touch, ES8311, external `P4` |
| GPIO16 | Shared I2C SDA: touch, ES8311, external `P4` |
| GPIO17 | FT6336U touch interrupt |
| GPIO18 | FT6336U touch reset |
| GPIO19 | USB D- |
| GPIO20 | USB D+ |
| GPIO21 | External `P3` `IO21` |
| GPIO38 | SDMMC CLK |
| GPIO39 | SDMMC D0 |
| GPIO40 | SDMMC CMD |
| GPIO41 | SDMMC D1 |
| GPIO42 | RGB LED data |
| GPIO43 | UART `TXD0`, external `P2` |
| GPIO44 | UART `RXD0`, external `P2` |
| GPIO45 | Display backlight `LCD_BL` |
| GPIO46 | Display data/command `LCD_RS` |
| GPIO47 | SDMMC D3 |
| GPIO48 | SDMMC D2 |

### `FNK0104N`

| GPIO | Signal/function |
|---:|---|
| GPIO0 | Boot button, active low |
| GPIO1 | Audio amplifier enable, `AUDIO_EN` |
| GPIO2 | SDMMC D2 |
| GPIO3 | SDMMC D3 |
| GPIO4 | SDMMC CMD |
| GPIO5 | SDMMC CLK |
| GPIO6 | SDMMC D0 |
| GPIO7 | SDMMC D1 |
| GPIO8 | Battery ADC |
| GPIO9 | Display QSPI D3 / `LCD_SDA3` |
| GPIO10 | Display `LCD_CS` |
| GPIO11 | Display QSPI D0 / `LCD_SDA0` |
| GPIO12 | Display QSPI SCLK / `LCD_SCK` |
| GPIO13 | Display QSPI D1 / `LCD_SDA1` |
| GPIO14 | Display QSPI D2 / `LCD_SDA2` |
| GPIO15 | I2S DOUT to ES8311/speaker |
| GPIO16 | I2S DIN from ES8311/microphone |
| GPIO17 | I2S MCLK |
| GPIO18 | I2S BCLK/SCK |
| GPIO19 | USB D- |
| GPIO20 | USB D+ |
| GPIO21 | I2S LRCLK/WS |
| GPIO38 | Shared I2C SDA: touch, ES8311, external `P4` |
| GPIO39 | Shared I2C SCL: touch, ES8311, external `P4` |
| GPIO40 | RGB LED data |
| GPIO41 | Display backlight `LCD_BL` |
| GPIO42 | Display tearing effect `LCD_TE` |
| GPIO43 | UART `TXD0`, external `P2` |
| GPIO44 | UART `RXD0`, external `P2` |
| GPIO45 | External `P3` `IO45`, strapping pin |
| GPIO46 | External `P3` `IO46`, strapping pin |
| GPIO47 | ST77922 touch interrupt |
| GPIO48 | ST77922 touch reset |

## Suggested ESPHome Building Blocks

Use the tables above to fill these ESPHome sections:

```yaml
i2c:
  sda: GPIO16  # or GPIO38 on FNK0104N
  scl: GPIO15  # or GPIO39 on FNK0104N

spi:
  clk_pin: GPIO12
  mosi_pin: GPIO11
  miso_pin: GPIO13  # wired on 2.8/4.0 inch boards; optional for write-only display use

binary_sensor:
  - platform: gpio
    pin:
      number: GPIO0
      mode: INPUT_PULLUP
      inverted: true
    name: Boot Button

sensor:
  - platform: adc
    pin: GPIO9  # GPIO8 on FNK0104N
    attenuation: 12db
    filters:
      - multiply: 2.0
```

For display, touch, SDMMC, and audio, choose the ESPHome component that matches your ESPHome version and substitute the exact GPIOs from the variant tables.

## Source References

Repository files used:

| Information | Source |
|---|---|
| Board variant names and advertised display drivers | `README.md` |
| 2.8 inch board schematic, touch I2C address, display connector, ESP32-S3 GPIO nets, external headers | `Schematic/2.8inch_ESP32-S3_Display_Schematic.pdf` |
| 3.5 inch board schematic, ST77922 connector, ESP32-S3 GPIO nets, external headers | `Schematic/3.5inch_ESP32-S3_Display_Schematic.pdf` |
| 4.0 inch board schematic, ST7796 connector, touch connector, ESP32-S3 GPIO nets, external headers | `Schematic/4.0inch_ESP32-S3_Display_Schematic.pdf` |
| SDMMC pin definitions | `Tutorial_With_Touch/Sketches/Sketch_06.1_SDMMC_Test/Sketch_06.1_SDMMC_Test.ino` |
| Audio, ES8311 I2C bus, I2S pin definitions, and SDMMC cross-check | `Tutorial_With_Touch/Sketches/Sketch_07.1_Music/Sketch_07.1_Music.ino`, `Tutorial_With_Touch/Sketches/Sketch_17.1_Lvgl_Music/Sketch_17.1_Lvgl_Music.ino`, `Tutorial_With_Touch/Sketches/Sketch_18.1_Lvgl_Multifunctionality/public.h` |
| Battery ADC pins and voltage multiplier | `Tutorial_With_Touch/Sketches/Sketch_05.1_Battery_Voltage/Sketch_05.1_Battery_Voltage.ino` |
| WS2812/RGB LED pins | `Tutorial_With_Touch/Sketches/Sketch_02.1_LedPixel/Sketch_02.1_LedPixel.ino`, `Tutorial_With_Touch/Sketches/Sketch_18.1_Lvgl_Multifunctionality/public.h` |
| Boot button GPIO0 behavior | `Tutorial_With_Touch/Sketches/Sketch_03.1_Button_RGB/Sketch_03.1_Button_RGB.ino`, `Tutorial_With_Touch/Sketches/Sketch_04.1_Button_Interrupt_UART/Sketch_04.1_Button_Interrupt_UART.ino`, `Tutorial_With_Touch/Tutorial.pdf` |
| FT6336U touch pins | `Tutorial_With_Touch/Sketches/Sketch_11.1_Touch/Sketch_11.1_Touch.ino`, `Tutorial_With_Touch/Sketches/Sketch_12.1_TFT_Touch_Draw/Sketch_12.1_TFT_Touch_Draw.ino` |
| FT6336U address | `Libraries/FNK0104AB/FT6336U_v1.0.2.zip` inner file `FT6336U_CTP_Controller/src/FT6336U.h` |
| 2.8 inch TFT_eSPI pins/settings | `Libraries/FNK0104AB/TFT_eSPI_Setups_v1.3.zip` inner file `TFT_eSPI_Setups/FNK0104AB_2.8_240x320_ILI9341.h` |
| 4.0 inch TFT_eSPI pins/settings | `Libraries/FNK0104S/TFT_eSPI_Setups_v1.2.zip` inner file `TFT_eSPI_Setups/FNK0104S_4.0_320x480_ST7796.h` |
| ST77922 QSPI display pins/settings and touch pins/address | `Libraries/FNK0104N/TFT_eSPI_v2.5.43.zip` inner files `TFT_eSPI/ST77922.h` and `TFT_eSPI/ST77922_Touch.h` |
| ES8311 codec I2C address | Schematics list `0x18`; codec code creates `ES8311_ADDRRES_0` in `Tutorial_With_Touch/Sketches/Sketch_07.1_Music/es8311.cpp` |
| ESP32-S3 module pin-name to GPIO mapping for `SPICLK_P` / `SPICLK_N` and strapping notes | `Datasheet/esp32-s3_datasheet_en.pdf`, `Datasheet/esp32-s3_hardware_design_guidelines_en.pdf` |
