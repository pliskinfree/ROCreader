#include "lid_power_control.h"

#include "runtime_log.h"

#include <cstdlib>
#include <initializer_list>
#include <fstream>
#include <sstream>
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

bool RunPowerScriptArg(const std::filesystem::path &script, const std::string &arg) {
  if (script.empty() || arg.empty()) return false;
  const std::string command = EscapeShellArg(script) + " " + EscapeShellArg(arg);
  const bool ok = RunCommandQuiet(command);
  runtime_log::Line(std::string("lid_power: script arg=") + arg +
                    " rc=" + (ok ? "0" : "nonzero") +
                    " path=" + script.string());
  return ok;
}

bool RunPowerScriptArgList(const std::filesystem::path &script,
                           std::initializer_list<const char *> args) {
  for (const char *arg : args) {
    if (arg && *arg && RunPowerScriptArg(script, arg)) return true;
  }
  return false;
}

bool RunPowerScriptArgListReverse(const std::filesystem::path &script,
                                  std::initializer_list<const char *> args) {
  for (const char *arg : args) {
    if (arg && *arg && RunPowerScriptArg(script, arg)) return true;
  }
  return false;
}

std::string TrimAscii(std::string text) {
  auto is_space = [](unsigned char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f' || ch == '\v';
  };
  size_t start = 0;
  while (start < text.size() && is_space(static_cast<unsigned char>(text[start]))) ++start;
  size_t end = text.size();
  while (end > start && is_space(static_cast<unsigned char>(text[end - 1]))) --end;
  return text.substr(start, end - start);
}

std::filesystem::path RgdsPowerStatePath(const std::filesystem::path &script) {
  if (const char *env = std::getenv("ROCREADER_RGDS_POWER_STATE_FILE"); env && *env) {
    return std::filesystem::path(env);
  }
  std::filesystem::path state = script;
  state.replace_filename("rgds_power_state.txt");
  return state;
}

bool TryReadTextFile(const std::filesystem::path &path, std::string &out) {
  std::ifstream in(path);
  if (!in) return false;
  std::ostringstream oss;
  oss << in.rdbuf();
  out = TrimAscii(oss.str());
  return !out.empty();
}

bool RgdsPowerStateMatches(const std::filesystem::path &script, const std::string &expected, bool &state_available) {
  state_available = false;
  const std::filesystem::path state_path = RgdsPowerStatePath(script);
  std::string state;
  if (!TryReadTextFile(state_path, state)) return false;
  state_available = true;
  const bool match = state == expected;
  if (!match) {
    runtime_log::Line("lid_power: rgds power state mismatch expected=" + expected +
                      " got=" + state + " path=" + state_path.string());
  }
  return match;
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
  if (const char *command = std::getenv("ROCREADER_RGDS_AUTO_SLEEP_COMMAND"); command && *command) {
    if (RunCommandQuiet(command)) {
      bool state_available = false;
      return !RgdsPowerStateMatches(std::filesystem::path(command), "off", state_available) ? !state_available : true;
    }
  }
  const bool script_ok =
      RunPowerScriptArgList(power_script_path_, {"auto", "powerkey", "manual", "off"}) ||
      RunPowerScriptArgListReverse(power_script_path_, {"off", "manual", "powerkey", "auto"});
  if (!script_ok) return false;
  bool state_available = false;
  return RgdsPowerStateMatches(power_script_path_, "off", state_available) || !state_available;
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
    return RunPowerScriptArg(power_script_path_, arg);
  }
  return RunPowerScriptArgList(power_script_path_, {"powerkey", "manual", "off"});
}

bool LidPowerController::TriggerScreenOn(InputProfile input_profile) const {
  if (input_profile == InputProfile::TrimuiBrick) {
    if (const char *command = std::getenv("ROCREADER_TRIMUI_SCREEN_ON_COMMAND"); command && *command) {
      return RunCommandQuiet(command);
    }
    const std::string shmvar_path =
        EnvOrDefault("ROCREADER_TRIMUI_SHMVAR_PATH", "/usr/trimui/bin/shmvar");
    if (RunCommandQuiet(EscapeShellArg(shmvar_path) + " lcd 1")) return true;
    return RunCommandQuiet("sh -c 'echo 0 > /sys/class/graphics/fb0/blank'");
  }

  if (!ScriptAvailable()) return false;
  if (const char *arg = std::getenv("ROCREADER_SCREEN_ON_ARG"); arg && *arg) {
    if (!RunPowerScriptArg(power_script_path_, arg)) return false;
    bool state_available = false;
    return RgdsPowerStateMatches(power_script_path_, "on", state_available) || !state_available;
  }
  if (const char *command = std::getenv("ROCREADER_RGDS_SCREEN_ON_COMMAND"); command && *command) {
    if (RunCommandQuiet(command)) {
      bool state_available = false;
      return RgdsPowerStateMatches(power_script_path_, "on", state_available) || !state_available;
    }
  }
  if (!(RunPowerScriptArgList(power_script_path_, {"on", "wake", "resume"}))) return false;
  bool state_available = false;
  return RgdsPowerStateMatches(power_script_path_, "on", state_available) || !state_available;
}

std::string LidPowerController::PowerScriptPath() const { return power_script_path_.string(); }
