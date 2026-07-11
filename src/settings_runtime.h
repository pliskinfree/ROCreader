#pragma once

#include "app_stores.h"
#include "animation.h"
#include "contributor_avatar_runtime.h"
#include "input_manager.h"
#include "key_calibration_runtime.h"
#include "online_source_runtime.h"
#include "reader_session_state.h"
#include "system_settings_runtime.h"
#include "txt_settings_runtime.h"
#include "ui_assets.h"
#include "ui_text_cache.h"
#include "version_update_runtime.h"

#include <SDL.h>

#include <functional>
#include <string>
#include <vector>

enum class SettingId {
  SystemControls,
  KeyGuide,
  KeyCalibration,
  ClearHistory,
  CleanCache,
  TxtToUtf8,
  ContributorAvatars,
  ContactMe,
  VersionUpdate,
  UrlEntry,
  ExitApp
};

struct SettingsRuntimeMenuState {
  bool &closing;
  bool &close_armed;
  float &toggle_guard;
  int &selected;
  const std::vector<SettingId> &items;
  animation::TweenFloat &anim;
};

struct SettingsRuntimeInputActions {
  bool menu_toggle_request = false;
  std::function<void()> on_close;
  std::function<void()> on_exit_app;
  std::function<void()> on_clear_history;
  std::function<void()> on_clean_cache;
  std::function<void()> on_txt_to_utf8;
  std::function<void(int)> on_contributor_avatar_confirm;
};

struct SettingsRuntimeInputDeps {
  const InputManager &input;
  const NativeConfig &ui_cfg;
  InputProfile input_profile = InputProfile::DesktopDefault;
  float dt = 0.0f;
  SettingsRuntimeMenuState menu;
  SystemSettingsState &system_settings_state;
  SystemSettingsCallbacks system_settings_callbacks;
  TxtSettingsState &txt_settings_state;
  TxtSettingsCallbacks txt_settings_callbacks;
  ContributorAvatarState &contributor_avatar_state;
  size_t contributor_avatar_count = 0;
  KeyCalibrationState &key_calibration_state;
  KeyCalibrationCallbacks key_calibration_callbacks;
  VersionUpdateState &version_update_state;
  OnlineSourceState &online_source_state;
  VersionUpdateCallbacks version_update_callbacks;
  SettingsRuntimeInputActions actions;
};

struct SettingsRuntimeLayout {
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

struct SettingsRuntimeRenderServices {
  std::function<void(int, int, int, int, SDL_Color, bool)> draw_rect;
  std::function<void(SDL_Texture *, int &, int &)> get_texture_size;
  std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_text_texture;
  std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_title_text_texture;
  std::function<TextCacheEntry *(const std::string &, SDL_Color)> get_reader_text_texture;
  std::function<std::string(const std::string &, size_t)> utf8_ellipsize;
  std::function<void()> draw_volume_overlay;
};

struct SettingsRuntimeRenderDeps {
  SDL_Renderer *renderer = nullptr;
  UiAssets &ui_assets;
  const NativeConfig &cfg;
  InputProfile input_profile = InputProfile::DesktopDefault;
  const std::vector<SettingId> &menu_items;
  int menu_selected = 0;
  animation::TweenFloat &menu_anim;
  int sidebar_mask_max_alpha = 0;
  const TxtTranscodeJob &txt_transcode_job;
  const SystemSettingsState &system_settings_state;
  const TxtSettingsState &txt_settings_state;
  const std::vector<ContributorAvatarEntry> &contributor_avatar_entries;
  const ContributorAvatarState &contributor_avatar_state;
  const KeyCalibrationState &key_calibration_state;
  bool has_calibrated_keymap = false;
  const VersionUpdateState &version_update_state;
  const OnlineSourceState &online_source_state;
  SettingsRuntimeLayout layout;
  SettingsRuntimeRenderServices services;
  bool show_chrome = true;
};

void HandleSettingsInput(SettingsRuntimeInputDeps &deps);
void DrawSettingsRuntime(SettingsRuntimeRenderDeps &deps);
