#include "avatar_badge_runtime.h"

#include <algorithm>

void AvatarBadgeRuntime::Configure(const AvatarBadgeRuntimeDeps &deps) {
  renderer_ = deps.renderer;
  entries_ = &deps.entries;
  scale_px_ = deps.scale_px;
  create_scaled_texture_ = deps.create_scaled_texture;
  remember_texture_size_ = deps.remember_texture_size;
  before_destroy_ = deps.before_destroy;
}

void AvatarBadgeRuntime::Shutdown() {
  DestroyBadgeTexture();
  selected_index_ = -1;
}

void AvatarBadgeRuntime::SelectIndex(int selected_index) {
  selected_index_ = -1;
  DestroyBadgeTexture();
  if (!entries_ || selected_index < 0 || selected_index >= static_cast<int>(entries_->size())) return;

  SDL_Texture *source = (*entries_)[selected_index].texture;
  if (!source || !renderer_ || !create_scaled_texture_) return;

  const int badge_size = scale_px_ ? scale_px_(28) : 28;
  badge_texture_ = create_scaled_texture_(renderer_, source, badge_size, badge_size);
  if (!badge_texture_) return;

  selected_index_ = selected_index;
  if (remember_texture_size_) remember_texture_size_(badge_texture_, badge_size, badge_size);
}

void AvatarBadgeRuntime::SelectSavedOrDefault(const std::string &saved_label) {
  const int saved_index = FindIndexByLabel(saved_label);
  SelectIndex(saved_index >= 0 ? saved_index : FindDefaultIndex());
}

int AvatarBadgeRuntime::FindDefaultIndex() const {
  if (!entries_ || entries_->empty()) return 0;
  for (size_t i = 0; i < entries_->size(); ++i) {
    if ((*entries_)[i].raw_label.find("BloodROC") != std::string::npos) return static_cast<int>(i);
  }
  for (size_t i = 0; i < entries_->size(); ++i) {
    if ((*entries_)[i].raw_label.find("MAX") != std::string::npos) return static_cast<int>(i);
  }
  return 0;
}

int AvatarBadgeRuntime::FindIndexByLabel(const std::string &saved_label) const {
  if (!entries_ || saved_label.empty()) return -1;
  for (size_t i = 0; i < entries_->size(); ++i) {
    if ((*entries_)[i].raw_label == saved_label) return static_cast<int>(i);
  }
  return -1;
}

void AvatarBadgeRuntime::DestroyBadgeTexture() {
  if (!badge_texture_) return;
  if (before_destroy_) before_destroy_(badge_texture_);
  SDL_DestroyTexture(badge_texture_);
  badge_texture_ = nullptr;
}
