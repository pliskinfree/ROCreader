#include "txt_transcode_service.h"

#include <algorithm>
#include <iostream>
#include <system_error>
#include <unordered_set>

std::vector<std::string> CollectScannedTxtFiles(const TxtTranscodeServiceDeps &deps) {
  std::vector<std::string> out;
  std::unordered_set<std::string> seen;
  for (const auto &root : deps.books_roots) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec) || ec) continue;
    const auto opts = std::filesystem::directory_options::skip_permission_denied;
    for (std::filesystem::recursive_directory_iterator it(root, opts, ec), end; it != end; it.increment(ec)) {
      if (ec) {
        ec.clear();
        continue;
      }
      if (!filesystem_compat::IsRegularFile(*it, ec) || ec) {
        ec.clear();
        continue;
      }
      const std::string path = it->path().string();
      if (!deps.get_lower_ext || deps.get_lower_ext(path) != ".txt") continue;
      const std::string key = deps.normalize_path_key ? deps.normalize_path_key(path) : path;
      if (seen.insert(key).second) out.push_back(path);
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

void StartTxtTranscodeJob(TxtTranscodeJob &job, const TxtTranscodeServiceDeps &deps) {
  if (job.active) return;
  job = TxtTranscodeJob{};
  job.files = CollectScannedTxtFiles(deps);
  job.total = job.files.size();
  job.active = job.total > 0;
  job.current_file.clear();
  if (deps.verbose_log) {
    std::cout << "[native_h700] txt transcode queued: files=" << job.total << "\n";
  }
}

void ProcessTxtTranscodeStep(TxtTranscodeJob &job, const TxtTranscodeServiceDeps &deps) {
  auto finish_job = [&]() {
    job.active = false;
    job.current_file.clear();
    if (deps.clear_runtime_cache_files) deps.clear_runtime_cache_files();
    if (deps.verbose_log || job.failed > 0) {
      std::cout << "[native_h700] txt transcode finished: processed=" << job.processed
                << " converted=" << job.converted
                << " failed=" << job.failed << "\n";
    }
  };

  if (!job.active) return;
  if (job.processed >= job.total) {
    finish_job();
    return;
  }

  const size_t idx = job.processed;
  const std::filesystem::path file_path(job.files[idx]);
  job.current_file = file_path.filename().string();

  std::string raw;
  std::string utf8;
  std::string detected_encoding;
  bool success = deps.read_file_bytes && deps.decode_text_bytes_to_utf8 &&
                 deps.read_file_bytes(file_path, raw) &&
                 deps.decode_text_bytes_to_utf8(raw, utf8, &detected_encoding);
  bool converted = false;
  if (success && utf8 != raw) {
    success = deps.write_file_bytes_atomically && deps.write_file_bytes_atomically(file_path, utf8);
    converted = success;
  }

  if (!success) {
    ++job.failed;
    std::cout << "[native_h700] txt transcode failed: " << file_path.string() << "\n";
  } else if (converted) {
    ++job.converted;
    if (deps.verbose_log) {
      std::cout << "[native_h700] txt transcoded: " << file_path.string()
                << " encoding=" << detected_encoding << "\n";
    }
  }

  ++job.processed;
  if (job.processed >= job.total) finish_job();
}
