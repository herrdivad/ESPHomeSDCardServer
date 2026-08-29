#pragma once

#include <cstddef>
#include <string>

namespace esphome {
namespace cyd_sd_http_server {

static constexpr size_t MAX_VIRTUAL_PATH_LENGTH = 240;
static constexpr size_t MAX_PATH_SEGMENT_LENGTH = 255;

enum class PathError {
  NONE,
  EMPTY,
  NOT_ABSOLUTE,
  CONTROL_CHARACTER,
  BACKSLASH,
  TRAVERSAL,
  TOO_LONG,
  SEGMENT_TOO_LONG,
  INVALID_ENCODING,
  ROOT_NOT_ALLOWED,
  RESERVED_NAME,
};

const char *path_error_to_string(PathError error);
PathError percent_decode(const std::string &input, std::string *output);
PathError normalize_virtual_path(const std::string &input, bool allow_root, std::string *output);
std::string join_mount_path(const std::string &mount_point, const std::string &virtual_path);
std::string virtual_basename(const std::string &virtual_path);
bool is_reserved_internal_path(const std::string &virtual_path);

}  // namespace cyd_sd_http_server
}  // namespace esphome

