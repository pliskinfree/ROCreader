#pragma once

#include "book_scanner.h"
#include "filesystem_compat.h"

#include <string>
#include <unordered_set>
#include <vector>

struct OnlineSourceCategory {
  std::string name;
  std::string url;
};

struct OnlineSourceEntry {
  std::string id;
  std::string name;
  std::string url;
  std::string type = "manual_web";
  bool visible = true;
  bool enabled = true;
  std::vector<OnlineSourceCategory> categories;
};

struct OnlineCatalogItem {
  std::string id;
  std::string title;
  std::string cover_url;
  std::string download_url;
  std::string file_ext = ".cbz";
  std::string local_path;
  bool marked_for_local = false;
};

struct OnlineSourceState {
  std::filesystem::path config_path;
  std::filesystem::path download_root;
  std::vector<OnlineSourceEntry> sources;
  int selected_source_index = -1;
  int active_source_index = -1;
  int selected_row = 0;
  int selected_button = 0;
  bool panel_active = false;
  bool connected = false;
  bool connect_pending = false;
  bool connecting = false;
  bool disconnect_requested = false;
  int loaded_category_index = -1;
  int pending_category_index = -1;
  bool category_loading = false;
  size_t cover_download_cursor = 0;
  bool covers_loading = false;
  std::string loaded_category_url;
  std::string next_page_url;
  bool next_page_loading = false;
  std::vector<OnlineCatalogItem> catalog_items;
  std::unordered_set<std::string> marked_local_keys;
  std::vector<OnlineCatalogItem> marked_local_items;
  std::string status_message;
};
