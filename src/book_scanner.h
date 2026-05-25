#pragma once

#include "filesystem_compat.h"
#include <string>
#include <vector>

enum class BookOrigin {
  Local,
  OnlineRemote,
  OnlineDownloaded,
};

struct BookItem {
  std::string name;
  std::string path;
  std::string real_path;
  std::filesystem::path native_fs_path;
  bool is_dir = false;
  int preload_category = -1;
  BookOrigin origin = BookOrigin::Local;
  bool is_remote = false;
  std::string remote_id;
  std::string remote_source_id;
  std::string remote_cover_url;
  std::string remote_download_url;
  std::string remote_local_path;
};

class BookScanner {
public:
  static std::vector<BookItem> Scan(const std::string &root);
  static std::vector<BookItem> ScanRoots(const std::vector<std::string> &roots);
  static std::vector<BookItem> ScanPath(const std::string &path, bool allow_subdirs);
};
