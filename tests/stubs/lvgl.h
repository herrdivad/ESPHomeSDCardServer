#pragma once
#include <cstdint>

struct lv_obj_t {};
struct lv_image_dsc_t {};
enum lv_image_align_t {
  LV_IMAGE_ALIGN_DEFAULT,
  LV_IMAGE_ALIGN_STRETCH,
  LV_IMAGE_ALIGN_CONTAIN,
  LV_IMAGE_ALIGN_COVER,
};
struct lv_area_t {};
struct lv_draw_buf_t {};
struct lv_fs_drv_t {};
using lv_fs_mode_t = uint8_t;
using lv_fs_res_t = uint8_t;
using lv_fs_whence_t = uint8_t;
