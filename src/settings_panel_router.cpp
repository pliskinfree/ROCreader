#include "settings_panel_router.h"

#include "app_language.h"
#include "avatar_panel.h"
#include "cache_panel.h"
#include "contact_panel.h"
#include "exit_panel.h"
#include "history_panel.h"
#include "key_guide_panel.h"
#include "online_source_panel.h"
#include "system_controls_panel.h"
#include "txt_settings_panel.h"
#include "update_panel.h"

#include <algorithm>

bool HandleSelectedSettingsPanelInput(SettingId id, SettingsRuntimeInputDeps &deps) {
  if (id == SettingId::SystemControls && HandleSystemControlsPanelInput(deps)) return true;
  if (id == SettingId::TxtToUtf8 && HandleTxtSettingsPanelInput(deps)) return true;
  if (id == SettingId::ContributorAvatars && HandleAvatarPanelInput(deps)) return true;
  if (id == SettingId::VersionUpdate && HandleUpdatePanelInput(deps)) return true;
  if (id == SettingId::UrlEntry && HandleOnlineSourcePanelInput(deps)) return true;
  return false;
}

bool HandleSelectedSettingsPanelConfirm(SettingId id, SettingsRuntimeInputDeps &deps) {
  if (id == SettingId::ExitApp) return HandleExitPanelConfirm(deps);
  if (id == SettingId::ClearHistory) return HandleHistoryPanelConfirm(deps);
  if (id == SettingId::CleanCache) return HandleCachePanelConfirm(deps);
  return false;
}

void DrawSelectedSettingsPanel(SettingId selected, SettingsRuntimeRenderDeps &deps,
                               SDL_Rect preview_rect, int language_index,
                               int first_menu_item_y, int sidebar_item_pitch,
                               int sidebar_item_h, float scale) {
  if (selected == SettingId::SystemControls) {
    DrawSystemControlsPanel(deps, preview_rect, first_menu_item_y, sidebar_item_pitch, sidebar_item_h, scale);
  }
  if (selected == SettingId::KeyGuide) {
    DrawKeyGuidePanel(deps, preview_rect, language_index, first_menu_item_y);
  }
  if (selected == SettingId::ContactMe) {
    DrawContactPanel(deps, preview_rect, language_index);
  }
  if (selected == SettingId::TxtToUtf8) {
    DrawTxtSettingsPanel(deps, preview_rect, language_index, first_menu_item_y, sidebar_item_pitch,
                         sidebar_item_h, scale);
  }
  if (selected == SettingId::ContributorAvatars) {
    DrawAvatarPanel(deps, preview_rect, language_index, scale);
  }
  if (selected == SettingId::VersionUpdate) {
    DrawUpdatePanel(deps, preview_rect, language_index, scale);
  }
  if (selected == SettingId::UrlEntry) {
    DrawOnlineSourcePanel(deps, preview_rect, language_index, first_menu_item_y,
                          sidebar_item_pitch, sidebar_item_h, scale);
  }
  if (selected == SettingId::ExitApp) {
    DrawExitPanel(deps, preview_rect, language_index);
  }
}
