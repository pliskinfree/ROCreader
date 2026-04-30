#include "cover_cache_runtime.h"

#include <SDL.h>

CoverCacheRuntime::CoverCacheRuntime(size_t max_entries, size_t max_bytes)
    : max_entries_(max_entries), max_bytes_(max_bytes) {}

SDL_Texture *CoverCacheRuntime::FindTexture(const std::string &key) {
  auto it = entries_.find(key);
  if (it == entries_.end()) return nullptr;
  it->second.last_use = SDL_GetTicks();
  return it->second.texture;
}

void CoverCacheRuntime::PutTexture(
    const std::string &key, SDL_Texture *texture, bool owned,
    const std::function<void(SDL_Texture *, int &, int &)> &get_texture_size,
    const std::function<void(SDL_Texture *)> &before_destroy) {
  int w = 0;
  int h = 0;
  if (texture && get_texture_size) get_texture_size(texture, w, h);
  const size_t bytes =
      (owned && w > 0 && h > 0) ? (static_cast<size_t>(w) * static_cast<size_t>(h) * 4u) : 0u;
  Entry entry{texture, w, h, bytes, SDL_GetTicks(), owned};

  auto it = entries_.find(key);
  if (it != entries_.end()) {
    DestroyEntry(it->second, before_destroy);
    it->second = entry;
  } else {
    entries_[key] = entry;
  }
  Prune(before_destroy);
}

void CoverCacheRuntime::Clear(const std::function<void(SDL_Texture *)> &before_destroy) {
  for (auto &kv : entries_) {
    DestroyEntry(kv.second, before_destroy);
  }
  entries_.clear();
  manual_path_cache_.clear();
}

size_t CoverCacheRuntime::TotalBytes() const {
  size_t total = 0;
  for (const auto &kv : entries_) total += kv.second.bytes;
  return total;
}

void CoverCacheRuntime::Prune(const std::function<void(SDL_Texture *)> &before_destroy) {
  while (entries_.size() > max_entries_ || TotalBytes() > max_bytes_) {
    auto oldest = entries_.end();
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
      if (oldest == entries_.end() || it->second.last_use < oldest->second.last_use) oldest = it;
    }
    if (oldest == entries_.end()) break;
    DestroyEntry(oldest->second, before_destroy);
    entries_.erase(oldest);
  }
}

void CoverCacheRuntime::DestroyEntry(Entry &entry,
                                     const std::function<void(SDL_Texture *)> &before_destroy) {
  if (!entry.texture || !entry.owned) return;
  if (before_destroy) before_destroy(entry.texture);
  SDL_DestroyTexture(entry.texture);
  entry.texture = nullptr;
}
