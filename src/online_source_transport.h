#pragma once

#include "filesystem_compat.h"

#include <cstddef>
#include <string>
#include <vector>

void CancelOnlineSourceTransfers();
void ResetOnlineSourceTransferCancel();
bool OnlineSourceTransfersCancelled();
std::string CompactLogSnippet(const std::string &text, size_t max_len = 220);
std::string RunHiddenProcessCapture(const std::vector<std::string> &args);
std::string HttpGetText(const std::string &url, const std::string &referer = {});
std::string ManualWebFetch(const std::string &url, const std::string &referer);
std::string ManualWebResolveDownload(const std::string &detail_url, const std::string &title,
                                     const std::string &source_url);
bool ManualWebDownload(const std::string &url, const std::filesystem::path &output_path,
                       const std::string &referer);
std::string HttpPostJsonText(const std::string &url, const std::string &json,
                             const std::string &origin = {}, const std::string &referer = {});
bool DownloadFile(const std::string &url, const std::filesystem::path &output_path,
                  const std::string &referer = {});
std::string ProbeDownloadSize(const std::string &url, const std::string &referer = {});
std::string ExtractJsonStringValue(const std::string &json, const std::string &key);
