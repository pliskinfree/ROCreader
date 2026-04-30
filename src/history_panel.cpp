#include "history_panel.h"

bool HandleHistoryPanelConfirm(SettingsRuntimeInputDeps &deps) {
  if (deps.actions.on_clear_history) deps.actions.on_clear_history();
  return true;
}
