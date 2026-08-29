#include "path_utils.h"

#include <cctype>
#include <vector>

namespace esphome {
namespace cyd_sd_http_server {

namespace {
int hex_value(char value) {
  if (value >= '0' && value <= '9')
    return value - '0';
  if (value >= 'a' && value <= 'f')
    return value - 'a' + 10;
  if (value >= 'A' && value <= 'F')
    return value - 'A' + 10;
  return -1;
}
}  // namespace

const char *path_error_to_string(PathError error) {
  switch (error) {
    case PathError::NONE:
      return "NONE";
    case PathError::EMPTY:
      return "EMPTY";
    case PathError::NOT_ABSOLUTE:
      return "NOT_ABSOLUTE";
    case PathError::CONTROL_CHARACTER:
      return "CONTROL_CHARACTER";
    case PathError::BACKSLASH:
      return "BACKSLASH";
    case PathError::TRAVERSAL:
      return "TRAVERSAL";
    case PathError::TOO_LONG:
      return "TOO_LONG";
    case PathError::SEGMENT_TOO_LONG:
      return "SEGMENT_TOO_LONG";
    case PathError::INVALID_ENCODING:
      return "INVALID_ENCODING";
    case PathError::ROOT_NOT_ALLOWED:
      return "ROOT_NOT_ALLOWED";
    case PathError::RESERVED_NAME:
      return "RESERVED_NAME";
  }
  return "UNKNOWN";
}

PathError percent_decode(const std::string &input, std::string *output) {
  output->clear();
  output->reserve(input.size());
  for (size_t index = 0; index < input.size(); index++) {
    if (input[index] != '%') {
      output->push_back(input[index]);
      continue;
    }
    if (index + 2 >= input.size())
      return PathError::INVALID_ENCODING;
    const int high = hex_value(input[index + 1]);
    const int low = hex_value(input[index + 2]);
    if (high < 0 || low < 0)
      return PathError::INVALID_ENCODING;
    output->push_back(static_cast<char>((high << 4) | low));
    index += 2;
  }
  return PathError::NONE;
}

bool is_reserved_internal_path(const std::string &virtual_path) {
  return virtual_path.find(".cyd-upload-") != std::string::npos ||
         virtual_path.find(".cyd-backup-") != std::string::npos;
}

PathError normalize_virtual_path(const std::string &input, bool allow_root, std::string *output) {
  output->clear();
  if (input.empty())
    return PathError::EMPTY;
  if (input.front() != '/')
    return PathError::NOT_ABSOLUTE;

  std::vector<std::string> segments;
  std::string segment;
  for (size_t index = 1; index <= input.size(); index++) {
    const bool at_end = index == input.size();
    const unsigned char value = at_end ? '/' : static_cast<unsigned char>(input[index]);
    if (!at_end && (value == 0 || value < 0x20 || value == 0x7F))
      return PathError::CONTROL_CHARACTER;
    if (!at_end && value == '\\')
      return PathError::BACKSLASH;
    if (value == '/') {
      if (segment.empty())
        continue;  // Repeated and trailing slashes are normalized.
      if (segment == "." || segment == "..")
        return PathError::TRAVERSAL;
      if (segment.size() > MAX_PATH_SEGMENT_LENGTH)
        return PathError::SEGMENT_TOO_LONG;
      segments.push_back(segment);
      segment.clear();
      continue;
    }
    segment.push_back(static_cast<char>(value));
  }

  if (segments.empty()) {
    if (!allow_root)
      return PathError::ROOT_NOT_ALLOWED;
    *output = "/";
    return PathError::NONE;
  }

  for (const auto &item : segments) {
    output->push_back('/');
    output->append(item);
  }
  if (output->size() > MAX_VIRTUAL_PATH_LENGTH) {
    output->clear();
    return PathError::TOO_LONG;
  }
  if (is_reserved_internal_path(*output)) {
    output->clear();
    return PathError::RESERVED_NAME;
  }
  return PathError::NONE;
}

std::string join_mount_path(const std::string &mount_point, const std::string &virtual_path) {
  if (virtual_path == "/")
    return mount_point;
  return mount_point + virtual_path;
}

std::string virtual_basename(const std::string &virtual_path) {
  const size_t offset = virtual_path.find_last_of('/');
  return offset == std::string::npos ? virtual_path : virtual_path.substr(offset + 1);
}

}  // namespace cyd_sd_http_server
}  // namespace esphome

