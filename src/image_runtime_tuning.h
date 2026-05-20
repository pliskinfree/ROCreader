#pragma once

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace image_runtime_tuning {

inline bool EqualsToken(const char *value, const char *token) {
  return value && token && std::strcmp(value, token) == 0;
}

inline bool EnvEnabled(const char *name) {
  const char *value = std::getenv(name);
  return value && *value && !EqualsToken(value, "0");
}

inline bool RgdsFastMode() {
  static const bool enabled = [] {
    if (EnvEnabled("ROCREADER_IMAGE_FAST_MODE")) return true;
    const char *device = std::getenv("ROCREADER_DEVICE_MODEL");
    return EqualsToken(device, "rgds");
  }();
  return enabled;
}

inline int ReadEnvInt(const char *name, int fallback, int min_value, int max_value) {
  const char *value = std::getenv(name);
  if (!value || !*value) return fallback;
  char *end = nullptr;
  errno = 0;
  const long parsed = std::strtol(value, &end, 10);
  if (errno != 0 || end == value) return fallback;
  const long clamped = std::clamp(parsed, static_cast<long>(min_value), static_cast<long>(max_value));
  return static_cast<int>(clamped);
}

inline int TextureCacheSlots(int default_slots, int max_slots) {
  static const int slots =
      ReadEnvInt("ROCREADER_IMAGE_TEXTURE_CACHE", RgdsFastMode() ? 8 : default_slots, 1, 10);
  return std::clamp(slots, 1, max_slots);
}

inline int PrefetchViewportScreens(int default_screens) {
  static const int screens =
      ReadEnvInt("ROCREADER_IMAGE_PREFETCH_SCREENS", RgdsFastMode() ? 4 : default_screens, 1, 6);
  return screens;
}

inline int PrefetchAheadPages(int default_pages) {
  static const int pages =
      ReadEnvInt("ROCREADER_IMAGE_PREFETCH_AHEAD", RgdsFastMode() ? 4 : default_pages, 1, 6);
  return pages;
}

inline unsigned int IdlePrefetchDelayMs(unsigned int default_ms) {
  static const int ms = ReadEnvInt("ROCREADER_IMAGE_IDLE_PREFETCH_MS",
                                   RgdsFastMode() ? 0 : static_cast<int>(default_ms), 0, 1000);
  return static_cast<unsigned int>(ms);
}

inline unsigned int VisualRenderThrottleMs(unsigned int default_ms) {
  static const int ms = ReadEnvInt("ROCREADER_IMAGE_VISUAL_THROTTLE_MS",
                                   RgdsFastMode() ? 0 : static_cast<int>(default_ms), 0, 250);
  return static_cast<unsigned int>(ms);
}

inline bool BidirectionalPrefetchEnabled() {
  static const bool enabled =
      ReadEnvInt("ROCREADER_IMAGE_PREFETCH_BIDIRECTIONAL", RgdsFastMode() ? 1 : 0, 0, 1) != 0;
  return enabled;
}

inline bool NormalRenderThreadPriority() {
  static const bool normal = [] {
    const char *value = std::getenv("ROCREADER_IMAGE_RENDER_THREAD_PRIORITY");
    if (value && *value) {
      return EqualsToken(value, "normal") || EqualsToken(value, "NORMAL") || EqualsToken(value, "1");
    }
    return RgdsFastMode();
  }();
  return normal;
}

inline bool DedicatedPrefetchThreadEnabled() {
  static const bool enabled =
      ReadEnvInt("ROCREADER_IMAGE_PREFETCH_DEDICATED_THREAD", RgdsFastMode() ? 1 : 0, 0, 1) != 0;
  return enabled;
}

}  // namespace image_runtime_tuning
