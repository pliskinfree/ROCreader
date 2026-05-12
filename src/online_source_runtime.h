#pragma once

#include "online_source_types.h"

#include <vector>

void InitializeOnlineSourceState(OnlineSourceState &state,
                                 const std::filesystem::path &config_path,
                                 const std::filesystem::path &download_root);
void ReloadOnlineSourceConfig(OnlineSourceState &state);
void ConnectOnlineSource(OnlineSourceState &state);
void DisconnectOnlineSource(OnlineSourceState &state);
void ClearOnlineSourceDownloads(OnlineSourceState &state);
bool LoadOnlineCatalogForCategory(OnlineSourceState &state, int category_index);
bool LoadOnlineCatalogNextPage(OnlineSourceState &state);
std::vector<BookItem> BuildOnlineShelfItems(const OnlineSourceState &state, bool marked_only = false);
std::filesystem::path OnlineCoverPathForItem(const OnlineSourceState &state, const BookItem &item);
bool DownloadOnlineCoverForItem(const OnlineSourceState &state, const BookItem &item);
bool DownloadOnlineBookForItem(OnlineSourceState &state, const BookItem &item, BookItem &out_local_item);
bool OnlineCatalogCoverExists(const OnlineSourceState &state, const OnlineCatalogItem &catalog_item);
bool DownloadOnlineCoverForCatalogItem(const OnlineSourceState &state, const OnlineCatalogItem &catalog_item);
