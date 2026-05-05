#include "lid_power_control.h"

#include <cstdlib>
#include <string>
#include <utility>

std::string EscapeShellArg(const std::filesystem::path &path) {
  std::string text = path.string();
  std::string escaped = "'";
  for (char ch : text) {
    if (ch == '\'') escaped += "'\\''";
    else escaped += ch;
  }
  escaped += "'";
  return escaped;
}

std::string EscapeShellArg(const std::string &text) {
  std::string escaped = "'";
  for (char ch : text) {
    if (ch == '\'') escaped += "'\\''";
    else escaped += ch;
  }
  escaped += "'";
  return escaped;
}

std::string EnvOrDefault(const char *name, const char *fallback) {
  const char *value = std::getenv(name);
  return (value && *value) ? std::string(value) : std::string(fallback ? fallback : "");
}

bool RunCommandQuiet(const std::string &command) {
  if (command.empty()) return false;
  const int rc = std::system((command + " >/dev/null 2>&1").c_str());
  return rc == 0;
}

LidPowerController::LidPowerController(std::filesystem::path power_script_path)
    : power_script_path_(std::move(power_script_path)) {}

bool LidPowerController::Enabled() const { return enabled_; }

void LidPowerController::SetEnabled(bool enabled) { enabled_ = enabled; }

bool LidPowerController::ScriptAvailable() const {
  std::error_code ec;
  return !power_script_path_.empty() && std::filesystem::exists(power_script_path_, ec) && !ec;
}

bool LidPowerController::TriggerAutoIfEnabled() const {
  if (!enabled_ || !ScriptAvailable()) return false;
  const std::string command = EscapeShellArg(power_script_path_) + " auto >/dev/null 2>&1";
  const int rc = std::system(command.c_str());
  return rc == 0;
}

bool LidPowerController::TriggerPowerKeyScreenOff(InputProfile input_profile) const {
  if (input_profile == InputProfile::TrimuiBrick) {
    if (const char *command = std::getenv("ROCREADER_TRIMUI_POWER_KEY_SCREEN_OFF_COMMAND");
        command && *command) {
      return RunCommandQuiet(command);
    }
    const std::string shmvar_path =
        EnvOrDefault("ROCREADER_TRIMUI_SHMVAR_PATH", "/usr/trimui/bin/shmvar");
    if (RunCommandQuiet(EscapeShellArg(shmvar_path) + " lcd 0")) return true;
    return RunCommandQuiet("sh -c 'echo 1 > /sys/class/graphics/fb0/blank'");
  }

  if (!ScriptAvailable()) return false;
  const char *env_name =
      (input_profile == InputProfile::H700Default || input_profile == InputProfile::H70034xxSp)
          ? "ROCREADER_H700_POWER_KEY_ARG"
          : "ROCREADER_POWER_KEY_ARG";
  if (const char *arg = std::getenv(env_name); arg && *arg) {
    return RunCommandQuiet(EscapeShellArg(power_script_path_) + " " + EscapeShellArg(std::string(arg)));
  }
  if (RunCommandQuiet(EscapeShellArg(power_script_path_) + " powerkey")) return true;
  if (RunCommandQuiet(EscapeShellArg(power_script_path_) + " manual")) return true;
  return RunCommandQuiet(EscapeShellArg(power_script_path_) + " off");
}

std::string LidPowerController::PowerScriptPath() const { return power_script_path_.string(); }
