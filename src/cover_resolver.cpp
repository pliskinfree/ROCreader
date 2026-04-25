#include "cover_resolver.h"
#include "path_adapter.h"

#include <algorithm>
#include <array>
#include <cctype>
#include "filesystem_compat.h"
#include <unordered_set>

namespace fs = std::filesystem;

namespace {

const std::array<const char *, 5> kCoverExts = {".png", ".jpg", ".jpeg", ".webp", ".bmp"};
const std::array<const char *, 6> kFolderCoverNames = {
    "folder_cover.png", "cover.png", "folder_cover.jpg", "cover.jpg", "folder_cover.webp", "cover.webp"};

std::string FirstExistingWithBase(const std::vector<std::string> &roots, const std::string &base) {
  for (const auto &root : roots) {
    for (const char *ext : kCoverExts) {
      fs::path p = fs::path(root) / (base + ext);
      if (fs::exists(p) && fs::is_regular_file(p)) return path_adapter::StorePathString(p);
    }
  }
  return {};
}

void RTrim(std::string &s) {
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '-' || s.back() == '_')) {
    s.pop_back();
  }
}

bool AllDigits(const std::string &s) {
  return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
}

std::string TrimVersionSuffix(std::string name) {
  while (true) {
    if (name.empty()) break;
    size_t cut = std::string::npos;
    if ((name.back() == ')' || name.back() == ']') && name.size() >= 3) {
      const char open = name.back() == ')' ? '(' : '[';
      const size_t open_pos = name.find_last_of(open);
      if (open_pos != std::string::npos && open_pos + 1 < name.size() - 1) {
        const std::string inside = name.substr(open_pos + 1, name.size() - open_pos - 2);
        if (AllDigits(inside)) cut = open_pos;
      }
    }
    if (cut == std::string::npos) {
      const size_t pos = name.find_last_of("-_");
      if (pos != std::string::npos && pos + 1 < name.size()) {
        size_t digit_start = pos + 1;
        if (name[digit_start] == 'v' || name[digit_start] == 'V') ++digit_start;
        if (digit_start < name.size() &&
            std::all_of(name.begin() + static_cast<std::ptrdiff_t>(digit_start), name.end(),
                        [](unsigned char c) { return std::isdigit(c) != 0; })) {
          cut = pos;
        }
      }
    }
    if (cut == std::string::npos) break;
    std::string next = name.substr(0, cut);
    RTrim(next);
    if (next == name) break;
    name = next;
  }
  return name;
}

std::string TrimEpisodeSuffix(std::string name) {
  if ((name.size() >= 2) && (name.back() == ')' || name.back() == ']')) {
    const char open = name.back() == ')' ? '(' : '[';
    const size_t open_pos = name.find_last_of(open);
    if (open_pos != std::string::npos && open_pos + 1 < name.size()) {
      name = name.substr(0, open_pos);
    }
  }

  size_t end = name.size();
  while (end > 0 && (name[end - 1] == ' ' || name[end - 1] == '\t')) --end;
  size_t start = end;
  while (start > 0) {
    const unsigned char c = static_cast<unsigned char>(name[start - 1]);
    if (std::isdigit(c) || name[start - 1] == '-' || name[start - 1] == '_' ||
        name[start - 1] == '~' || name[start - 1] == ' ') {
      --start;
      continue;
    }
    break;
  }
  bool saw_digit = false;
  for (size_t i = start; i < end; ++i) {
    saw_digit = saw_digit || std::isdigit(static_cast<unsigned char>(name[i]));
  }
  if (saw_digit && start < end) name = name.substr(0, start);
  RTrim(name);
  return name;
}

void AddCandidate(std::vector<std::string> &out, const std::string &s, std::unordered_set<std::string> &seen) {
  if (s.empty()) return;
  if (seen.insert(s).second) out.push_back(s);
}

std::vector<std::string> BuildFuzzyCandidates(const std::string &base) {
  std::vector<std::string> out;
  std::unordered_set<std::string> seen;
  AddCandidate(out, TrimVersionSuffix(base), seen);
  AddCandidate(out, TrimEpisodeSuffix(base), seen);
  AddCandidate(out, TrimEpisodeSuffix(TrimVersionSuffix(base)), seen);
  std::vector<std::string> filtered;
  for (const auto &cand : out) {
    if (!cand.empty() && cand != base) filtered.push_back(cand);
  }
  return filtered;
}

} // namespace

namespace cover_resolver {

std::string ResolveCoverPathExact(const std::string &item_path,
                                  bool is_dir,
                                  const std::vector<std::string> &cover_roots) {
  fs::path p(item_path);
  if (item_path.empty()) return {};

  if (is_dir) {
    for (const char *name : kFolderCoverNames) {
      fs::path candidate = p / name;
      if (fs::exists(candidate) && fs::is_regular_file(candidate)) return path_adapter::StorePathString(candidate);
    }
  }

  const std::string base = p.stem().string();
  if (!base.empty()) {
    std::string found = FirstExistingWithBase(cover_roots, base);
    if (!found.empty()) return found;
  }

  return {};
}

std::string ResolveCoverPathFuzzy(const std::string &item_path,
                                  bool is_dir,
                                  const std::vector<std::string> &cover_roots) {
  fs::path p(item_path);
  if (item_path.empty()) return {};

  const std::string base = p.stem().string();
  if (!base.empty()) {
    const std::vector<std::string> candidates = BuildFuzzyCandidates(base);
    for (const auto &cand : candidates) {
      std::string found = FirstExistingWithBase(cover_roots, cand);
      if (!found.empty()) return found;
    }
  }

  if (!is_dir) {
    const fs::path parent = p.parent_path();
    if (!parent.empty()) {
      for (const char *name : kFolderCoverNames) {
        fs::path candidate = parent / name;
        if (fs::exists(candidate) && fs::is_regular_file(candidate)) return path_adapter::StorePathString(candidate);
      }
      const std::string parent_base = parent.filename().string();
      if (!parent_base.empty()) {
        const std::vector<std::string> parent_candidates = BuildFuzzyCandidates(parent_base);
        for (const auto &cand : parent_candidates) {
          std::string found = FirstExistingWithBase(cover_roots, cand);
          if (!found.empty()) return found;
        }
      }
    }
  }

  return {};
}

std::string ResolveCoverPath(const std::string &item_path, bool is_dir, const std::vector<std::string> &cover_roots) {
  std::string found = ResolveCoverPathExact(item_path, is_dir, cover_roots);
  if (!found.empty()) return found;
  return ResolveCoverPathFuzzy(item_path, is_dir, cover_roots);
}

} // namespace cover_resolver
