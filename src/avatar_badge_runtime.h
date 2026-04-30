#pragma once

#include "contributor_avatar_runtime.h"

#include <SDL.h>

#include <functional>
#include <string>
#include <vector>

struct AvatarBadgeRuntimeDeps {
  SDL_Renderer *renderer = nullptr;
  const std::vector<ContributorAvatarEntry> &entries;
  std::function<int(int)> scale_px;
  std::function<SDL_Texture *(SDL_Renderer *, SDL_Texture *, int, int)> create_scaled_texture;
  std::function<void(SDL_Texture *, int, int)> remember_texture_size;
  std::function<void(SDL_Texture *)> before_destroy;
};

class AvatarBadgeRuntime {
public:
  void Configure(const AvatarBadgeRuntimeDeps &deps);
  void Shutdown();
  void SelectIndex(int selected_index);
  void SelectSavedOrDefault(const std::string &saved_label);

  int SelectedIndex() const { return selected_index_; }
  SDL_Texture *BadgeTexture() const { return badge_texture_; }

private:
  int FindDefaultIndex() const;
  int FindIndexByLabel(const std::string &saved_label) const;
  void DestroyBadgeTexture();

  SDL_Renderer *renderer_ = nullptr;
  const std::vector<ContributorAvatarEntry> *entries_ = nullptr;
  std::function<int(int)> scale_px_;
  std::function<SDL_Texture *(SDL_Renderer *, SDL_Texture *, int, int)> create_scaled_texture_;
  std::function<void(SDL_Texture *, int, int)> remember_texture_size_;
  std::function<void(SDL_Texture *)> before_destroy_;
  SDL_Texture *badge_texture_ = nullptr;
  int selected_index_ = -1;
};
