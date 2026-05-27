#pragma once

#include "app_environment.h"
#include "app_runtime.h"
#include "app_stores.h"
#include "progress_store.h"
#include "rgds_runtime.h"
#include "lid_power_control.h"
#include "menu_scene.h"
#include "online_shelf_controller.h"
#include "system_controls.h"
#include "system_status.h"
#include "system_settings_runtime.h"
#include "txt_settings_runtime.h"

#include <SDL.h>

#include <functional>
#include <filesystem>
#include <string>
#include <vector>

struct MenuSceneState;

namespace rgds {
struct InteractionState;
}

struct AppInputDevices {
  std::vector<SDL_GameController *> opened_controllers;
  std::vector<SDL_Joystick *> opened_joysticks;
};

struct AppRuntimePaths {
  std::filesystem::path exe_path;
  std::filesystem::path ui_path;

  std::filesystem::path ResolveRuntimeFile(const std::string &name) const;
};

struct AppConfigPaths {
  std::filesystem::path keymap_path;
  std::filesystem::path config_path;
  std::filesystem::path progress_path;
  std::filesystem::path favorites_path;
  std::filesystem::path history_path;
  std::filesystem::path power_script_path;
};

struct AppRuntimeStores {
  ConfigStore config;
  ProgressStore progress;
  RecentPathStore favorites_store;
  RecentPathStore history_store;
};

struct AppSystemServices {
  VolumeController volume_controller;
  SystemStatusMonitor system_status;
  SystemControlService system_control_service;
  LidPowerController lid_power_controller;
};

struct AppSettingsStates {
  SystemSettingsState system_settings_state;
  TxtSettingsState txt_settings_state;
};

struct AppStoragePaths {
  std::vector<std::string> books_roots;
  std::vector<std::string> cover_roots;
  std::filesystem::path txt_layout_cache_dir;
  std::filesystem::path removable_txt_layout_cache_dir;
  std::filesystem::path cover_thumb_cache_dir;
  std::filesystem::path removable_cover_thumb_cache_dir;
};

struct OnlineShelfInputTickHandlers {
  std::function<void()> reset_to_category_root;
  std::function<void()> clear_cover_cache;
  std::function<void()> rebuild_shelf_items;
  std::function<void()> reset_shelf_cover_stream_preload;
  std::function<void()> enter_shelf;
};

struct OnlineShelfPresentTickHandlers {
  std::function<void()> clear_cover_cache;
  std::function<void()> rebuild_shelf_items;
  std::function<void()> reset_shelf_cover_stream_preload;
};

struct OnlineShelfDeferredDisconnectHandlers {
  std::function<void()> reset_nav_to_first;
  std::function<void()> reset_to_category_root;
  std::function<void()> clear_cover_cache;
  std::function<void()> rebuild_shelf_items;
  std::function<void()> reset_shelf_cover_stream_preload;
  std::function<void()> enter_shelf;
};

struct ContributorAvatarState;
struct MenuSceneInputContext;

AppPlatformEnv ResolveAppPlatformEnv(const std::string &device_model_token,
                                     const ScreenProfile &screen_profile,
                                     const rgds::Runtime &rgds_runtime);
AppRuntimePaths ResolveAppRuntimePaths();
AppConfigPaths ResolveAppConfigPaths(const AppRuntimePaths &runtime_paths);
AppRuntimeStores InitializeAppRuntimeStores(const AppConfigPaths &app_config_paths,
                                            const std::function<std::string(const std::string &)> &normalize_path_key);
AppSystemServices InitializeAppSystemServices(bool use_h700_defaults,
                                               const std::filesystem::path &power_script_path);
AppSettingsStates InitializeAppSettingsStates(ConfigStore &config,
                                              SystemControlService &system_control_service,
                                              bool use_h700_defaults,
                                              InputProfile input_profile);
void InitializeLidCloseScreenOffState(const ConfigStore &config,
                                      LidPowerController &lid_power_controller,
                                      SystemSettingsState &system_settings_state);
void SnapRgdsMenuOpenState(MenuSceneState &menu_state, bool close_armed);
void InitializeRgdsStartupState(bool is_rgds_runtime,
                                MenuSceneState &menu_state,
                                rgds::InteractionState &rgds_interaction,
                                bool close_armed);
void InitializeRgdsReaderState(bool is_rgds_runtime,
                               MenuSceneState &menu_state,
                               rgds::InteractionState &rgds_interaction);
void ApplyOnlineShelfInputTickResult(const OnlineShelfControllerTickResult &result,
                                     const OnlineShelfInputTickHandlers &handlers);
void ApplyOnlineShelfPresentTickResult(const OnlineShelfControllerTickResult &result,
                                       const OnlineShelfPresentTickHandlers &handlers);
bool HandleOnlineShelfDeferredConnect(OnlineShelfController &online_shelf_controller,
                                      ShelfRuntimeState &shelf_runtime,
                                      const OnlineShelfInputTickHandlers &handlers);
void HandleOnlineShelfDeferredDisconnect(OnlineShelfController &online_shelf_controller,
                                         std::vector<std::string> &books_roots,
                                         std::vector<std::string> &cover_roots,
                                         const OnlineShelfDeferredDisconnectHandlers &handlers);
MenuSceneInputContext MakeMenuSceneInputContext(const InputManager &input,
                                                const NativeConfig &ui_cfg,
                                                float dt,
                                                MenuSceneState &menu_state,
                                                SystemSettingsState &system_settings_state,
                                                TxtSettingsState &txt_settings_state,
                                                ContributorAvatarState &contributor_avatar_state,
                                                size_t contributor_avatar_count,
                                                VersionUpdateState &version_update_state,
                                                OnlineSourceState &online_source_state,
                                                MenuSceneInputServices services);
void PrepareMenuSceneInputState(SystemSettingsState &system_settings_state,
                                bool refresh_system_controls,
                                SystemControlService &system_control_service,
                                ContributorAvatarState &contributor_avatar_state,
                                size_t contributor_avatar_count);
AppInputDevices OpenAppInputDevices(bool verbose_log);
void CloseAppInputDevices(AppInputDevices &devices);
void RefreshAppInputDevices(AppInputDevices &devices, bool verbose_log);
AppStoragePaths InitializeAppStoragePaths(bool verbose_log);
