#pragma once

#include "filesystem_compat.h"
#include "online_source_types.h"

#include <string>
#include <vector>

struct ManualWebCatalogPage {
  std::string body;
  std::vector<OnlineCatalogItem> items;
  std::string next_page_url;
};

ManualWebCatalogPage LoadManualWebCatalogPage(const std::string &url, const std::string &source_url);
std::vector<OnlineCatalogItem> ParseManualWebCatalog(const std::string &html, const std::string &base_url);
std::string ParseManualWebNextPageUrl(const std::string &html, const std::string &base_url);
std::string ResolveManualWebDownloadUrl(const std::string &detail_url, const std::string &title,
                                        const std::string &source_url);
bool DownloadManualWebFile(const std::string &url, const std::filesystem::path &output_path,
                           const std::string &referer);
