#pragma once

#include "settings_runtime.h"

struct LayoutMetrics;

struct MenuSceneLayoutMetrics {
  int screen_w = 0;
  int screen_h = 0;
  int top_bar_y = 0;
  int top_bar_h = 0;
  int bottom_bar_y = 0;
  int bottom_bar_h = 0;
  int settings_sidebar_w = 0;
  int settings_y_offset = 0;
  int settings_content_offset_y = 0;
  float ui_scale = 1.0f;
};

struct MenuSceneState {
  animation::TweenFloat anim{0.0f};
  bool closing = false;
  float toggle_guard = 0.0f;
  bool close_armed = true;
  int selected = 0;
  std::vector<SettingId> items;
};

struct MenuSceneAnimationConfig {
  bool animations_enabled = false;
  float close_duration_sec = 0.0f;
  float open_duration_sec = 0.0f;
  float toggle_guard_sec = 0.0f;
};

struct MenuSceneInputServices {
  SystemSettingsCallbacks system_settings_callbacks;
  TxtSettingsCallbacks txt_settings_callbacks;
  VersionUpdateCallbacks version_update_callbacks;
  SettingsRuntimeInputActions actions;
};

MenuSceneInputServices MakeMenuSceneInputServices(
    SystemSettingsCallbacks system_settings_callbacks,
    TxtSettingsCallbacks txt_settings_callbacks,
    VersionUpdateCallbacks version_update_callbacks,
    SettingsRuntimeInputActions actions);

struct MenuSceneInputContext {
  const InputManager &input;
  const NativeConfig &ui_cfg;
  float dt = 0.0f;
  MenuSceneState &menu_state;
  SystemSettingsState &system_settings_state;
  TxtSettingsState &txt_settings_state;
  ContributorAvatarState &contributor_avatar_state;
  size_t contributor_avatar_count = 0;
  VersionUpdateState &version_update_state;
  MenuSceneInputServices services;
};

struct MenuSceneRenderContext {
  SDL_Renderer *renderer = nullptr;
  UiAssets &ui_assets;
  const NativeConfig &cfg;
  InputProfile input_profile = InputProfile::DesktopDefault;
  MenuSceneState &menu_state;
  int sidebar_mask_max_alpha = 0;
  const TxtTranscodeJob &txt_transcode_job;
  const SystemSettingsState &system_settings_state;
  const TxtSettingsState &txt_settings_state;
  const std::vector<ContributorAvatarEntry> &contributor_avatar_entries;
  const ContributorAvatarState &contributor_avatar_state;
  const VersionUpdateState &version_update_state;
  MenuSceneLayoutMetrics layout;
  SettingsRuntimeRenderServices services;
};

struct MenuSceneRenderServiceCallbacks {
  std::function<void(int, int, int, int, SDL_Color, bool)> draw_rect;
  std::function<void(SDL_Texture *, int &, int &)> get_texture_size;
  std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_text_texture;
  std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_title_text_texture;
  std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_reader_text_texture;
  std::function<std::string(const std::string &, size_t)> utf8_ellipsize;
  std::function<void()> draw_volume_overlay;
};

SettingsRuntimeRenderServices MakeMenuSceneRenderServices(MenuSceneRenderServiceCallbacks callbacks);

MenuSceneLayoutMetrics MakeMenuSceneLayoutMetrics(const LayoutMetrics &layout);

class MenuScene {
public:
  void Tick(MenuSceneState &state, float dt) const;
  bool IsSelected(const MenuSceneState &state, SettingId id) const;
  bool IsAnimating(const MenuSceneState &state) const;
  bool CanCloseWithToggle(const MenuSceneState &state) const;
  void BeginClose(MenuSceneState &state, const MenuSceneAnimationConfig &config) const;
  void BeginOpen(MenuSceneState &state, const MenuSceneAnimationConfig &config) const;
  void SnapClosed(MenuSceneState &state) const;
  void HandleInput(const MenuSceneInputContext &context) const;
  void Draw(const MenuSceneRenderContext &context) const;

private:
  SettingsRuntimeLayout MakeLayout(const MenuSceneLayoutMetrics &metrics) const;
};
