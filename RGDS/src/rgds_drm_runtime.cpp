#include "rgds_drm_runtime.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sstream>

namespace rgds {

namespace {

std::string ErrnoText(const char *prefix) {
  std::ostringstream out;
  out << prefix << ": errno=" << errno << " " << std::strerror(errno);
  return out.str();
}

drmModeModeInfo PickMode(drmModeConnector *connector) {
  for (int i = 0; i < connector->count_modes; ++i) {
    if (connector->modes[i].hdisplay == kScreenW && connector->modes[i].vdisplay == kScreenH) {
      return connector->modes[i];
    }
  }
  return connector->modes[0];
}

bool CrtcAvailable(int crtc_index, const std::vector<ConnectorBinding> &bindings) {
  for (const auto &binding : bindings) {
    if (binding.crtc_index == crtc_index) return false;
  }
  return true;
}

bool PickCrtc(int fd,
              drmModeRes *resources,
              drmModeConnector *connector,
              const std::vector<ConnectorBinding> &bindings,
              ConnectorBinding &out) {
  std::vector<drmModeEncoder *> encoders;
  if (connector->encoder_id) {
    if (drmModeEncoder *encoder = drmModeGetEncoder(fd, connector->encoder_id)) encoders.push_back(encoder);
  }
  for (int i = 0; i < connector->count_encoders; ++i) {
    if (connector->encoders[i] == connector->encoder_id) continue;
    if (drmModeEncoder *encoder = drmModeGetEncoder(fd, connector->encoders[i])) encoders.push_back(encoder);
  }

  for (drmModeEncoder *encoder : encoders) {
    for (int crtc_index = 0; crtc_index < resources->count_crtcs; ++crtc_index) {
      if ((encoder->possible_crtcs & (1 << crtc_index)) == 0) continue;
      if (!CrtcAvailable(crtc_index, bindings)) continue;
      out.connector_id = connector->connector_id;
      out.crtc_id = resources->crtcs[crtc_index];
      out.crtc_index = crtc_index;
      out.mode = PickMode(connector);
      for (drmModeEncoder *owned : encoders) drmModeFreeEncoder(owned);
      return true;
    }
  }

  for (drmModeEncoder *owned : encoders) drmModeFreeEncoder(owned);
  return false;
}

}  // namespace

DrmRuntime::~DrmRuntime() {
  Shutdown(true);
}

DrmInitResult DrmRuntime::Initialize() {
  Shutdown(false);

  DrmInitResult result;
  std::string error;
  for (const char *path : {"/dev/dri/card0", "/dev/dri/card1", "/dev/dri/card2", "/dev/dri/card3"}) {
    if (TryOpenCard(path, error)) {
      result.ok = true;
      result.card_path = card_path_;
      return result;
    }
  }
  result.error = error.empty() ? "no DRM card succeeded" : error;
  return result;
}

void DrmRuntime::Shutdown(bool restore_crtcs) {
  if (restore_crtcs && fd_ >= 0) {
    for (auto &binding : bindings_) {
      if (!binding.old_crtc) continue;
      drmModeSetCrtc(fd_, binding.old_crtc->crtc_id,
                     binding.old_crtc->buffer_id,
                     binding.old_crtc->x,
                     binding.old_crtc->y,
                     &binding.connector_id,
                     1,
                     &binding.old_crtc->mode);
      drmModeFreeCrtc(binding.old_crtc);
      binding.old_crtc = nullptr;
    }
  } else {
    for (auto &binding : bindings_) {
      if (binding.old_crtc) {
        drmModeFreeCrtc(binding.old_crtc);
        binding.old_crtc = nullptr;
      }
    }
  }

  DestroyFramebuffer(top_);
  DestroyFramebuffer(bottom_);
  DestroyFramebuffer(top_back_);
  DestroyFramebuffer(bottom_back_);
  bindings_.clear();
  back_active_ = false;

  if (resources_) {
    drmModeFreeResources(resources_);
    resources_ = nullptr;
  }
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
  }
  card_path_.clear();
}

bool DrmRuntime::Present() {
  if (fd_ < 0 || bindings_.size() < 2) return false;
  Framebuffer *buffers[2] = {back_active_ ? &top_back_ : &top_,
                             back_active_ ? &bottom_back_ : &bottom_};
  bool ok = true;
  for (int i = 0; i < 2; ++i) {
    uint32_t connector_id = bindings_[i].connector_id;
    if (drmModeSetCrtc(fd_, bindings_[i].crtc_id, buffers[i]->fb, 0, 0,
                       &connector_id, 1, &bindings_[i].mode) != 0) {
      ok = false;
    }
  }
  if (ok) back_active_ = !back_active_;
  return ok;
}

uint32_t *DrmRuntime::Pixels(ScreenId screen) {
  return DrawBuffer(screen).pixels;
}

const Framebuffer &DrmRuntime::Buffer(ScreenId screen) const {
  if (back_active_) return screen == ScreenId::Top ? top_back_ : bottom_back_;
  return screen == ScreenId::Top ? top_ : bottom_;
}

Framebuffer &DrmRuntime::DrawBuffer(ScreenId screen) {
  if (back_active_) return screen == ScreenId::Top ? top_ : bottom_;
  return screen == ScreenId::Top ? top_back_ : bottom_back_;
}

std::string DrmRuntime::DescribeBindings() const {
  std::ostringstream out;
  out << "card=" << card_path_;
  for (size_t i = 0; i < bindings_.size(); ++i) {
    out << " screen" << i
        << " connector=" << bindings_[i].connector_id
        << " crtc=" << bindings_[i].crtc_id
        << " mode=" << bindings_[i].mode.hdisplay << "x" << bindings_[i].mode.vdisplay
        << "@" << bindings_[i].mode.vrefresh;
  }
  return out.str();
}

uint32_t DrmRuntime::Pack(Rgba color) {
  return (static_cast<uint32_t>(color.r) << 16) |
         (static_cast<uint32_t>(color.g) << 8) |
         static_cast<uint32_t>(color.b);
}

void DrmRuntime::Clear(Framebuffer &buffer, Rgba color) {
  if (!buffer.pixels) return;
  const uint32_t packed = Pack(color);
  const int stride = static_cast<int>(buffer.pitch / 4);
  for (int y = 0; y < buffer.h; ++y) {
    std::fill(buffer.pixels + y * stride, buffer.pixels + y * stride + buffer.w, packed);
  }
}

void DrmRuntime::FillRect(Framebuffer &buffer, int x, int y, int w, int h, Rgba color) {
  if (!buffer.pixels) return;
  x = std::max(0, x);
  y = std::max(0, y);
  w = std::min(w, buffer.w - x);
  h = std::min(h, buffer.h - y);
  if (w <= 0 || h <= 0) return;
  const uint32_t packed = Pack(color);
  const int stride = static_cast<int>(buffer.pitch / 4);
  for (int yy = y; yy < y + h; ++yy) {
    std::fill(buffer.pixels + yy * stride + x, buffer.pixels + yy * stride + x + w, packed);
  }
}

void DrmRuntime::StrokeRect(Framebuffer &buffer, int x, int y, int w, int h, int thickness, Rgba color) {
  for (int i = 0; i < thickness; ++i) {
    FillRect(buffer, x + i, y + i, w - i * 2, 1, color);
    FillRect(buffer, x + i, y + h - 1 - i, w - i * 2, 1, color);
    FillRect(buffer, x + i, y + i, 1, h - i * 2, color);
    FillRect(buffer, x + w - 1 - i, y + i, 1, h - i * 2, color);
  }
}

void DrmRuntime::BlendRect(Framebuffer &buffer, int x, int y, int w, int h, Rgba color) {
  if (!buffer.pixels || color.a == 0) return;
  if (color.a == 255) {
    FillRect(buffer, x, y, w, h, color);
    return;
  }
  x = std::max(0, x);
  y = std::max(0, y);
  w = std::min(w, buffer.w - x);
  h = std::min(h, buffer.h - y);
  const int stride = static_cast<int>(buffer.pitch / 4);
  for (int yy = y; yy < y + h; ++yy) {
    for (int xx = x; xx < x + w; ++xx) {
      uint32_t &dst = buffer.pixels[yy * stride + xx];
      const uint8_t dr = static_cast<uint8_t>((dst >> 16) & 0xff);
      const uint8_t dg = static_cast<uint8_t>((dst >> 8) & 0xff);
      const uint8_t db = static_cast<uint8_t>(dst & 0xff);
      const uint8_t a = color.a;
      const uint8_t r = static_cast<uint8_t>((color.r * a + dr * (255 - a)) / 255);
      const uint8_t g = static_cast<uint8_t>((color.g * a + dg * (255 - a)) / 255);
      const uint8_t b = static_cast<uint8_t>((color.b * a + db * (255 - a)) / 255);
      dst = Pack(Rgba{r, g, b, 255});
    }
  }
}

bool DrmRuntime::TryOpenCard(const char *path, std::string &error) {
  fd_ = open(path, O_RDWR | O_CLOEXEC);
  if (fd_ < 0) {
    error = ErrnoText("open DRM card failed");
    return false;
  }
  card_path_ = path;

  uint64_t cap = 0;
  if (drmGetCap(fd_, DRM_CAP_DUMB_BUFFER, &cap) != 0 || cap != 1) {
    error = "DRM card does not support dumb buffers";
    Shutdown(false);
    return false;
  }

  resources_ = drmModeGetResources(fd_);
  if (!resources_) {
    error = ErrnoText("drmModeGetResources failed");
    Shutdown(false);
    return false;
  }
  if (!PickConnectors(error)) {
    Shutdown(false);
    return false;
  }
  if (!CreateFramebuffer(top_, error) || !CreateFramebuffer(bottom_, error) ||
      !CreateFramebuffer(top_back_, error) || !CreateFramebuffer(bottom_back_, error)) {
    Shutdown(false);
    return false;
  }
  for (auto &binding : bindings_) {
    binding.old_crtc = drmModeGetCrtc(fd_, binding.crtc_id);
  }
  if (!Present()) {
    error = ErrnoText("initial drmModeSetCrtc failed");
    Shutdown(false);
    return false;
  }
  return true;
}

bool DrmRuntime::PickConnectors(std::string &error) {
  for (int i = 0; i < resources_->count_connectors; ++i) {
    drmModeConnector *connector = drmModeGetConnector(fd_, resources_->connectors[i]);
    if (!connector) continue;
    if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
      ConnectorBinding binding;
      if (PickCrtc(fd_, resources_, connector, bindings_, binding)) {
        bindings_.push_back(binding);
      }
    }
    drmModeFreeConnector(connector);
    if (bindings_.size() >= 2) break;
  }
  if (bindings_.size() < 2) {
    error = "RGDS DRM runtime requires two connected DRM connectors";
    return false;
  }
  return true;
}

bool DrmRuntime::CreateFramebuffer(Framebuffer &buffer, std::string &error) {
  drm_mode_create_dumb create{};
  create.width = buffer.w;
  create.height = buffer.h;
  create.bpp = 32;
  if (ioctl(fd_, DRM_IOCTL_MODE_CREATE_DUMB, &create) != 0) {
    error = ErrnoText("CREATE_DUMB failed");
    return false;
  }
  buffer.handle = create.handle;
  buffer.pitch = create.pitch;
  buffer.size = create.size;

  if (drmModeAddFB(fd_, buffer.w, buffer.h, 24, 32, buffer.pitch, buffer.handle, &buffer.fb) != 0) {
    error = ErrnoText("drmModeAddFB failed");
    return false;
  }

  drm_mode_map_dumb map_req{};
  map_req.handle = buffer.handle;
  if (ioctl(fd_, DRM_IOCTL_MODE_MAP_DUMB, &map_req) != 0) {
    error = ErrnoText("MAP_DUMB failed");
    return false;
  }

  void *mapped = mmap(nullptr, buffer.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, map_req.offset);
  if (mapped == MAP_FAILED) {
    error = ErrnoText("mmap failed");
    return false;
  }
  buffer.pixels = static_cast<uint32_t *>(mapped);
  return true;
}

void DrmRuntime::DestroyFramebuffer(Framebuffer &buffer) {
  if (fd_ < 0) return;
  if (buffer.pixels) {
    munmap(buffer.pixels, buffer.size);
    buffer.pixels = nullptr;
  }
  if (buffer.fb) {
    drmModeRmFB(fd_, buffer.fb);
    buffer.fb = 0;
  }
  if (buffer.handle) {
    drm_mode_destroy_dumb destroy{};
    destroy.handle = buffer.handle;
    ioctl(fd_, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    buffer.handle = 0;
  }
}

}  // namespace rgds
