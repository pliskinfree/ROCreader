#pragma once

#include "contributor_avatar_runtime.h"
#include "input_manager.h"
#include "menu_scene.h"
#include "reader_scene.h"
#include "rgds_reader_layout.h"
#include "rgds_interaction.h"
#include "rgds_runtime.h"
#include "scene_manager.h"
#include "settings_runtime.h"
#include "texture_registry.h"
#include "txt_transcode_service.h"
#include "ui_assets.h"
#include "ui_assets_loader.h"
#include "ui_text_cache.h"
#include "version_update_runtime.h"

#include <SDL.h>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace rgds {

struct RenderResources {
  UiAssets bottom_ui_assets;
  UiTextCacheState bottom_text_cache;
  std::vector<ContributorAvatarEntry> bottom_contributor_avatar_entries;
  std::filesystem::path exe_path;
  std::filesystem::path ui_path;
  int body_font_pt = 0;
  int title_font_pt = 0;
  int reader_font_pt = 0;
};

struct RenderResourceLoadDeps {
  Runtime &runtime;
  std::filesystem::path exe_path;
  std::filesystem::path ui_path;
  std::string ui_profile_name;
  std::function<SDL_Texture *(SDL_Renderer *, const std::string &)> load_texture_from_file;
  std::function<SDL_Surface *(const void *, size_t)> load_surface_from_memory;
  std::function<SDL_Texture *(SDL_Renderer *, SDL_Surface *)> create_texture_from_surface;
  std::function<void(SDL_Texture *, int, int)> remember_texture_size;
  std::function<void(SDL_Texture *)> before_destroy_texture;
  int language_index = 0;
  int avatar_texture_size = 96;
  size_t max_text_cache_entries = 0;
  int body_font_pt = 0;
  int title_font_pt = 0;
  int reader_font_pt = 0;
};

void LoadRenderResources(RenderResources &resources, RenderResourceLoadDeps deps);
void DestroyRenderResources(RenderResources &resources, const BeforeDestroyTextureFn &before_destroy);

void DrawFocusFlash(SDL_Renderer *renderer, uint32_t now, const InteractionState &interaction, bool top_screen);
void DrawTopReaderSlice(Runtime &runtime, SDL_Renderer *renderer, const ReaderScene &reader_scene,
                        ReaderSceneRenderDeps reader_render_deps, const ReaderLayout &reader_layout);

struct BottomRenderDeps {
  Runtime &runtime;
  RenderResources &resources;
  const InteractionState &interaction;
  AppScene scene = AppScene::Boot;
  uint32_t now = 0;
  const NativeConfig &cfg;
  InputProfile input_profile = InputProfile::DesktopDefault;
  MenuScene &menu_scene;
  MenuSceneState &menu_state;
  int sidebar_mask_max_alpha = 0;
  const TxtTranscodeJob &txt_transcode_job;
  const SystemSettingsState &system_settings_state;
  const TxtSettingsState &txt_settings_state;
  const std::vector<ContributorAvatarEntry> *contributor_avatar_entries = nullptr;
  const ContributorAvatarState &contributor_avatar_state;
  const KeyCalibrationState &key_calibration_state;
  bool has_calibrated_keymap = false;
  const VersionUpdateState &version_update_state;
  const OnlineSourceState &online_source_state;
  MenuSceneLayoutMetrics menu_layout;
  ReaderLayout reader_layout;
  ReaderScene *reader_scene = nullptr;
  ReaderSceneRenderDeps *reader_render_deps = nullptr;
  bool render_reader_scene_on_bottom = false;
  std::function<void(SDL_Texture *, int &, int &)> get_texture_size;
  std::function<std::string(const std::string &, size_t)> utf8_ellipsize;
};

void DrawBottomScreen(const BottomRenderDeps &deps);

} // namespace rgds
