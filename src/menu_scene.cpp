#include "menu_scene.h"

#include "app_layout.h"

#include <algorithm>

MenuSceneLayoutMetrics MakeMenuSceneLayoutMetrics(const LayoutMetrics &layout) {
  return MenuSceneLayoutMetrics{
      layout.screen_w,
      layout.screen_h,
      layout.top_bar_y,
      layout.top_bar_h,
      layout.bottom_bar_y,
      layout.bottom_bar_h,
      layout.settings_sidebar_w,
      layout.settings_y_offset,
      layout.settings_content_offset_y,
      layout.ui_scale,
  };
}

MenuSceneInputServices MakeMenuSceneInputServices(
    SystemSettingsCallbacks system_settings_callbacks,
    TxtSettingsCallbacks txt_settings_callbacks,
    VersionUpdateCallbacks version_update_callbacks,
    SettingsRuntimeInputActions actions) {
  return MenuSceneInputServices{
      std::move(system_settings_callbacks),
      std::move(txt_settings_callbacks),
      std::move(version_update_callbacks),
      std::move(actions),
  };
}

SettingsRuntimeRenderServices MakeMenuSceneRenderServices(MenuSceneRenderServiceCallbacks callbacks) {
  return SettingsRuntimeRenderServices{
      std::move(callbacks.draw_rect),
      std::move(callbacks.get_texture_size),
      std::move(callbacks.get_text_texture),
      std::move(callbacks.get_title_text_texture),
      std::move(callbacks.get_reader_text_texture),
      std::move(callbacks.utf8_ellipsize),
      std::move(callbacks.draw_volume_overlay),
  };
}

void MenuScene::Tick(MenuSceneState &state, float dt) const {
  state.toggle_guard = std::max(0.0f, state.toggle_guard - dt);
}

bool MenuScene::IsSelected(const MenuSceneState &state, SettingId id) const {
  if (state.items.empty()) return false;
  const int selected = std::clamp(state.selected, 0, static_cast<int>(state.items.size()) - 1);
  return state.items[selected] == id;
}

bool MenuScene::IsAnimating(const MenuSceneState &state) const {
  return state.anim.IsAnimating();
}

bool MenuScene::CanCloseWithToggle(const MenuSceneState &state) const {
  return state.close_armed && state.toggle_guard <= 0.0f && !state.closing;
}

void MenuScene::BeginClose(MenuSceneState &state, const MenuSceneAnimationConfig &config) const {
  if (config.animations_enabled) state.anim.AnimateTo(0.0f, config.close_duration_sec, animation::Ease::InOutCubic);
  else state.anim.Snap(0.0f);
  state.closing = true;
}

void MenuScene::BeginOpen(MenuSceneState &state, const MenuSceneAnimationConfig &config) const {
  state.anim.Snap(0.0f);
  if (config.animations_enabled) state.anim.AnimateTo(1.0f, config.open_duration_sec, animation::Ease::OutCubic);
  else state.anim.Snap(1.0f);
  state.toggle_guard = config.toggle_guard_sec;
  state.close_armed = false;
  state.closing = false;
}

void MenuScene::SnapClosed(MenuSceneState &state) const {
  state.anim.Snap(0.0f);
}

void MenuScene::HandleInput(const MenuSceneInputContext &context) const {
  SettingsRuntimeInputDeps deps{
      context.input,
      context.ui_cfg,
      context.dt,
      SettingsRuntimeMenuState{
          context.menu_state.closing,
          context.menu_state.close_armed,
          context.menu_state.toggle_guard,
          context.menu_state.selected,
          context.menu_state.items,
          context.menu_state.anim,
      },
      context.system_settings_state,
      context.services.system_settings_callbacks,
      context.txt_settings_state,
      context.services.txt_settings_callbacks,
      context.contributor_avatar_state,
      context.contributor_avatar_count,
      context.version_update_state,
      context.services.version_update_callbacks,
      context.services.actions,
  };
  HandleSettingsInput(deps);
}

void MenuScene::Draw(const MenuSceneRenderContext &context) const {
  SettingsRuntimeRenderDeps deps{
      context.renderer,
      context.ui_assets,
      context.cfg,
      context.input_profile,
      context.menu_state.items,
      context.menu_state.selected,
      context.menu_state.anim,
      context.sidebar_mask_max_alpha,
      context.txt_transcode_job,
      context.system_settings_state,
      context.txt_settings_state,
      context.contributor_avatar_entries,
      context.contributor_avatar_state,
      context.version_update_state,
      MakeLayout(context.layout),
      context.services,
  };
  DrawSettingsRuntime(deps);
}

SettingsRuntimeLayout MenuScene::MakeLayout(const MenuSceneLayoutMetrics &metrics) const {
  return SettingsRuntimeLayout{
      metrics.screen_w,
      metrics.screen_h,
      metrics.top_bar_y,
      metrics.top_bar_h,
      metrics.bottom_bar_y,
      metrics.bottom_bar_h,
      metrics.settings_sidebar_w,
      metrics.settings_y_offset,
      metrics.settings_content_offset_y,
      metrics.ui_scale,
  };
}
