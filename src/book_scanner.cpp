#include "book_scanner.h"
#include "path_adapter.h"
#include "runtime_log.h"

#include <algorithm>
#include <cctype>
#include "filesystem_compat.h"

namespace fs = std::filesystem;

namespace {
bool HasSupportedExt(const fs::path &p) {
  if (!p.has_extension()) {
    return false;
  }
  std::string ext = p.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return ext == ".pdf" || ext == ".txt" || ext == ".epub";
}
} // namespace

std::vector<BookItem> BookScanner::Scan(const std::string &root) {
  return ScanRoots({root});
}

std::vector<BookItem> BookScanner::ScanRoots(const std::vector<std::string> &roots) {
  std::vector<BookItem> out;
  runtime_log::Line("scanner: ScanRoots begin count=" + std::to_string(roots.size()));
  for (const auto &root : roots) {
    runtime_log::Line("scanner: root begin " + root);
    std::vector<BookItem> scanned = ScanPath(root, true);
    runtime_log::Line("scanner: root done " + root + " items=" + std::to_string(scanned.size()));
    out.insert(out.end(), scanned.begin(), scanned.end());
  }

  std::sort(out.begin(), out.end(), [](const BookItem &a, const BookItem &b) {
    if (a.is_dir != b.is_dir) {
      return a.is_dir > b.is_dir;
    }
    return a.name < b.name;
  });

  runtime_log::Line("scanner: ScanRoots done total=" + std::to_string(out.size()));
  return out;
}

std::vector<BookItem> BookScanner::ScanPath(const std::string &path, bool allow_subdirs) {
  std::vector<BookItem> out;
  fs::path root_path(path);
  if (!fs::exists(root_path) || !fs::is_directory(root_path)) {
    runtime_log::Line("scanner: skip missing dir " + path);
    return out;
  }

  std::error_code ec;
  for (const auto &entry : fs::directory_iterator(root_path, ec)) {
    if (ec) {
      runtime_log::Line("scanner: iterator error " + path + " code=" + std::to_string(ec.value()) +
                        " message=" + ec.message());
      ec.clear();
      break;
    }
    const bool is_dir = filesystem_compat::IsDirectory(entry);
    if (is_dir && !allow_subdirs) {
      continue;
    }

    BookItem item;
    item.name = entry.path().filename().string();
    item.path = is_dir ? path_adapter::ResolveReadableDirPath(entry.path())
                       : path_adapter::ResolveReadableFilePath(entry);
    item.real_path = item.path;
    item.native_fs_path = entry.path();
    item.is_dir = is_dir;

    if (item.is_dir || HasSupportedExt(entry.path())) {
      out.push_back(std::move(item));
    }
  }

  std::sort(out.begin(), out.end(), [](const BookItem &a, const BookItem &b) {
    if (a.is_dir != b.is_dir) {
      return a.is_dir > b.is_dir;
    }
    return a.name < b.name;
  });

  return out;
}
