#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <xf86drmMode.h>

namespace rgds {

constexpr int kScreenW = 640;
constexpr int kScreenH = 480;
constexpr int kVirtualReaderW = 640;
constexpr int kVirtualReaderH = 960;

enum class ScreenId {
  Top,
  Bottom,
};

struct Rgba {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 255;
};

struct Framebuffer {
  int w = kScreenW;
  int h = kScreenH;
  uint32_t handle = 0;
  uint32_t fb = 0;
  uint32_t pitch = 0;
  uint64_t size = 0;
  uint32_t *pixels = nullptr;
};

struct ConnectorBinding {
  uint32_t connector_id = 0;
  uint32_t crtc_id = 0;
  int crtc_index = -1;
  drmModeModeInfo mode{};
  drmModeCrtc *old_crtc = nullptr;
};

struct DrmInitResult {
  bool ok = false;
  std::string error;
  std::string card_path;
};

class DrmRuntime {
 public:
  DrmRuntime() = default;
  ~DrmRuntime();

  DrmRuntime(const DrmRuntime &) = delete;
  DrmRuntime &operator=(const DrmRuntime &) = delete;

  DrmInitResult Initialize();
  void Shutdown(bool restore_crtcs);
  bool Present();

  uint32_t *Pixels(ScreenId screen);
  const Framebuffer &Buffer(ScreenId screen) const;
  Framebuffer &DrawBuffer(ScreenId screen);
  std::string DescribeBindings() const;

  static uint32_t Pack(Rgba color);
  static void Clear(Framebuffer &buffer, Rgba color);
  static void FillRect(Framebuffer &buffer, int x, int y, int w, int h, Rgba color);
  static void StrokeRect(Framebuffer &buffer, int x, int y, int w, int h, int thickness, Rgba color);
  static void BlendRect(Framebuffer &buffer, int x, int y, int w, int h, Rgba color);

 private:
  bool TryOpenCard(const char *path, std::string &error);
  bool PickConnectors(std::string &error);
  bool CreateFramebuffer(Framebuffer &buffer, std::string &error);
  void DestroyFramebuffer(Framebuffer &buffer);

  int fd_ = -1;
  std::string card_path_;
  drmModeRes *resources_ = nullptr;
  std::vector<ConnectorBinding> bindings_;
  Framebuffer top_{};
  Framebuffer bottom_{};
  Framebuffer top_back_{};
  Framebuffer bottom_back_{};
  bool back_active_ = false;
};

}  // namespace rgds
