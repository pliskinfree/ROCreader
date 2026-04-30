#pragma once

#include <SDL.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

class CoverCacheRuntime {
public:
  CoverCacheRuntime(size_t max_entries, size_t max_bytes);

  SDL_Texture *FindTexture(const std::string &key);
  void PutTexture(const std::string &key, SDL_Texture *texture, bool owned,
                  const std::function<void(SDL_Texture *, int &, int &)> &get_texture_size,
                  const std::function<void(SDL_Texture *)> &before_destroy);
  void Clear(const std::function<void(SDL_Texture *)> &before_destroy);

  std::unordered_map<std::string, std::string> &ManualPathCache() { return manual_path_cache_; }

private:
  struct Entry {
    SDL_Texture *texture = nullptr;
    int w = 0;
    int h = 0;
    size_t bytes = 0;
    uint32_t last_use = 0;
    bool owned = true;
  };

  size_t TotalBytes() const;
  void Prune(const std::function<void(SDL_Texture *)> &before_destroy);
  void DestroyEntry(Entry &entry, const std::function<void(SDL_Texture *)> &before_destroy);

  size_t max_entries_ = 0;
  size_t max_bytes_ = 0;
  std::unordered_map<std::string, Entry> entries_;
  std::unordered_map<std::string, std::string> manual_path_cache_;
};
