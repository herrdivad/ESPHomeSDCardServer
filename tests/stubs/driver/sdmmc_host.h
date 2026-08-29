#pragma once
#include "esp_err.h"
using gpio_num_t = int;
struct sdmmc_host_t {};
struct sdmmc_slot_config_t {
  int width{1};
  gpio_num_t clk{-1};
  gpio_num_t cmd{-1};
  gpio_num_t d0{-1};
  gpio_num_t d1{-1};
  gpio_num_t d2{-1};
  gpio_num_t d3{-1};
  unsigned flags{0};
};
#define SDMMC_HOST_DEFAULT() sdmmc_host_t{}
#define SDMMC_SLOT_CONFIG_DEFAULT() sdmmc_slot_config_t{}
static constexpr unsigned SDMMC_SLOT_FLAG_INTERNAL_PULLUP = 1;

