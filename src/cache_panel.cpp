#include "cache_panel.h"

bool HandleCachePanelConfirm(SettingsRuntimeInputDeps &deps) {
  if (deps.on_clean_cache) deps.on_clean_cache();
  return true;
}
