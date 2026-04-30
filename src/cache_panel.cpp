#include "cache_panel.h"

bool HandleCachePanelConfirm(SettingsRuntimeInputDeps &deps) {
  if (deps.actions.on_clean_cache) deps.actions.on_clean_cache();
  return true;
}
