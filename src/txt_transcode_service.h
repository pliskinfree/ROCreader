#pragma once

#include "reader_session_state.h"

#include "filesystem_compat.h"
#include <functional>
#include <string>
#include <vector>

struct TxtTranscodeServiceDeps {
  const std::vector<std::string> &books_roots;
  std::function<std::string(const std::string &)> normalize_path_key;
  std::function<std::string(const std::string &)> get_lower_ext;
  std::function<bool(const std::filesystem::path &, std::string &)> read_file_bytes;
  std::function<bool(const std::string &, std::string &, std::string *)> decode_text_bytes_to_utf8;
  std::function<bool(const std::filesystem::path &, const std::string &)> write_file_bytes_atomically;
  std::function<void()> clear_runtime_cache_files;
  bool verbose_log = false;
};

std::vector<std::string> CollectScannedTxtFiles(const TxtTranscodeServiceDeps &deps);
void StartTxtTranscodeJob(TxtTranscodeJob &job, const TxtTranscodeServiceDeps &deps);
void ProcessTxtTranscodeStep(TxtTranscodeJob &job, const TxtTranscodeServiceDeps &deps);
