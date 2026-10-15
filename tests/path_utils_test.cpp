#include <cassert>
#include <string>

#include "path_utils.h"

using esphome::cyd_sd_http_server::PathError;
using esphome::cyd_sd_http_server::normalize_virtual_path;
using esphome::cyd_sd_http_server::percent_decode;

int main() {
  std::string value;
  assert(normalize_virtual_path("/", true, &value) == PathError::NONE && value == "/");
  assert(normalize_virtual_path("//images///photo.jpg/", false, &value) == PathError::NONE && value == "/images/photo.jpg");
  assert(normalize_virtual_path("/unicode/naïve.txt", false, &value) == PathError::NONE);
  assert(normalize_virtual_path("relative", false, &value) == PathError::NOT_ABSOLUTE);
  assert(normalize_virtual_path("/", false, &value) == PathError::ROOT_NOT_ALLOWED);
  assert(normalize_virtual_path("/a/../b", false, &value) == PathError::TRAVERSAL);
  assert(normalize_virtual_path("/a/./b", false, &value) == PathError::TRAVERSAL);
  assert(normalize_virtual_path("/a\\b", false, &value) == PathError::BACKSLASH);
  assert(normalize_virtual_path(std::string("/a\0b", 4), false, &value) == PathError::CONTROL_CHARACTER);
  assert(normalize_virtual_path("/.cyd-upload-1.part", false, &value) == PathError::RESERVED_NAME);
  assert(normalize_virtual_path("/a.cyd-backup-2.part", false, &value) == PathError::RESERVED_NAME);
  assert(normalize_virtual_path("/" + std::string(256, 'x'), false, &value) == PathError::SEGMENT_TOO_LONG);

  assert(percent_decode("%2Funicode%2Fna%C3%AFve.txt", &value) == PathError::NONE);
  assert(value == "/unicode/naïve.txt");
  assert(percent_decode("%2e%2e%2Fsecret", &value) == PathError::NONE);
  assert(normalize_virtual_path("/" + value, false, &value) == PathError::TRAVERSAL);
  assert(percent_decode("%", &value) == PathError::INVALID_ENCODING);
  assert(percent_decode("%GG", &value) == PathError::INVALID_ENCODING);
  return 0;
}

