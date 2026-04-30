#pragma once

#include <cstddef>
#include <string>

struct ReaderChapterAnchor {
  std::string title;
  int page = 0;
  int scroll_y = 0;
  size_t source_offset = 0;
};
