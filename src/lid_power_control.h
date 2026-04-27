#pragma once

#include "filesystem_compat.h"
#include "input_manager.h"
#include <string>

class LidPowerController {
public:
  explicit LidPowerController(std::filesystem::path power_script_path);

  bool Enabled() const;
  void SetEnabled(bool enabled);
  bool ScriptAvailable() const;
  bool TriggerAutoIfEnabled() const;
  bool TriggerPowerKeyScreenOff(InputProfile input_profile) const;
  std::string PowerScriptPath() const;

private:
  std::filesystem::path power_script_path_;
  bool enabled_ = true;
};
