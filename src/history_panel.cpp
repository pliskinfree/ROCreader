#include "history_panel.h"

bool HandleHistoryPanelConfirm(SettingsRuntimeInputDeps &deps) {
  if (deps.on_clear_history) deps.on_clear_history();
  return true;
}
