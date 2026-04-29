#pragma once

#include "app_stores.h"

struct AppConfigBridge {
  ConfigStore *config = nullptr;

  bool IsDirty() const { return config && config->IsDirty(); }
  void Save() {
    if (config) config->Save();
  }
};
