#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kTargetW = 640;
constexpr int kTargetH = 480;

struct DumbBuffer {
  uint32_t handle = 0;
  uint32_t fb = 0;
  uint32_t pitch = 0;
  uint64_t size = 0;
  void *map = nullptr;
  int w = kTargetW;
  int h = kTargetH;
};

struct ConnectorPick {
  uint32_t connector_id = 0;
  uint32_t encoder_id = 0;
  uint32_t crtc_id = 0;
  int crtc_index = -1;
  drmModeModeInfo mode{};
  drmModeCrtc *old_crtc = nullptr;
};

int ReadEnvInt(const char *name, int fallback_value, int min_value, int max_value) {
  const char *raw = std::getenv(name);
  if (!raw || !*raw) return fallback_value;
  try {
    return std::clamp(std::stoi(raw), min_value, max_value);
  } catch (...) {
    return fallback_value;
  }
}

std::string ErrnoText(const char *prefix) {
  return std::string(prefix) + ": errno=" + std::to_string(errno) + " " + std::strerror(errno);
}

bool CreateDumbBuffer(int fd, DumbBuffer &buffer, uint32_t color) {
  drm_mode_create_dumb create{};
  create.width = buffer.w;
  create.height = buffer.h;
  create.bpp = 32;
  if (ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) != 0) {
    std::cout << "[rgds_drm_probe] " << ErrnoText("CREATE_DUMB failed") << "\n";
    return false;
  }
  buffer.handle = create.handle;
  buffer.pitch = create.pitch;
  buffer.size = create.size;

  if (drmModeAddFB(fd, buffer.w, buffer.h, 24, 32, buffer.pitch, buffer.handle, &buffer.fb) != 0) {
    std::cout << "[rgds_drm_probe] " << ErrnoText("drmModeAddFB failed") << "\n";
    return false;
  }

  drm_mode_map_dumb map_req{};
  map_req.handle = buffer.handle;
  if (ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req) != 0) {
    std::cout << "[rgds_drm_probe] " << ErrnoText("MAP_DUMB failed") << "\n";
    return false;
  }

  buffer.map = mmap(nullptr, buffer.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map_req.offset);
  if (buffer.map == MAP_FAILED) {
    buffer.map = nullptr;
    std::cout << "[rgds_drm_probe] " << ErrnoText("mmap failed") << "\n";
    return false;
  }

  auto *pixels = static_cast<uint32_t *>(buffer.map);
  const int stride_pixels = static_cast<int>(buffer.pitch / 4);
  for (int y = 0; y < buffer.h; ++y) {
    for (int x = 0; x < buffer.w; ++x) {
      const bool border = x < 12 || y < 12 || x >= buffer.w - 12 || y >= buffer.h - 12;
      const bool cross = (x > buffer.w / 2 - 3 && x < buffer.w / 2 + 3) ||
                         (y > buffer.h / 2 - 3 && y < buffer.h / 2 + 3);
      pixels[y * stride_pixels + x] = border || cross ? 0x00ffffffu : color;
    }
  }
  return true;
}

void FillDumbBuffer(DumbBuffer &buffer, uint32_t color, int frame) {
  if (!buffer.map) return;
  auto *pixels = static_cast<uint32_t *>(buffer.map);
  const int stride_pixels = static_cast<int>(buffer.pitch / 4);
  for (int y = 0; y < buffer.h; ++y) {
    for (int x = 0; x < buffer.w; ++x) {
      const bool border = x < 12 || y < 12 || x >= buffer.w - 12 || y >= buffer.h - 12;
      const bool cross = (x > buffer.w / 2 - 3 && x < buffer.w / 2 + 3) ||
                         (y > buffer.h / 2 - 3 && y < buffer.h / 2 + 3);
      const bool marker = x >= 24 && x < 24 + (frame % 40) * 8 && y >= 24 && y < 42;
      pixels[y * stride_pixels + x] = border || cross || marker ? 0x00ffffffu : color;
    }
  }
}

void DestroyDumbBuffer(int fd, DumbBuffer &buffer) {
  if (buffer.map) {
    munmap(buffer.map, buffer.size);
    buffer.map = nullptr;
  }
  if (buffer.fb) {
    drmModeRmFB(fd, buffer.fb);
    buffer.fb = 0;
  }
  if (buffer.handle) {
    drm_mode_destroy_dumb destroy{};
    destroy.handle = buffer.handle;
    ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    buffer.handle = 0;
  }
}

bool ModeLooksUsable(const drmModeModeInfo &mode) {
  return mode.hdisplay == kTargetW && mode.vdisplay == kTargetH;
}

drmModeModeInfo PickMode(drmModeConnector *connector) {
  for (int i = 0; i < connector->count_modes; ++i) {
    if (ModeLooksUsable(connector->modes[i])) return connector->modes[i];
  }
  if (connector->count_modes > 0) return connector->modes[0];
  drmModeModeInfo fallback{};
  fallback.hdisplay = kTargetW;
  fallback.vdisplay = kTargetH;
  fallback.vrefresh = 60;
  std::snprintf(fallback.name, sizeof(fallback.name), "640x480");
  return fallback;
}

bool CrtcIndexAvailable(int index, const std::vector<ConnectorPick> &picks) {
  for (const auto &pick : picks) {
    if (pick.crtc_index == index) return false;
  }
  return true;
}

bool PickCrtcForConnector(int fd,
                          drmModeRes *res,
                          drmModeConnector *connector,
                          const std::vector<ConnectorPick> &existing,
                          ConnectorPick &out) {
  std::vector<drmModeEncoder *> encoders;
  if (connector->encoder_id) {
    drmModeEncoder *encoder = drmModeGetEncoder(fd, connector->encoder_id);
    if (encoder) encoders.push_back(encoder);
  }
  for (int i = 0; i < connector->count_encoders; ++i) {
    if (connector->encoders[i] == connector->encoder_id) continue;
    drmModeEncoder *encoder = drmModeGetEncoder(fd, connector->encoders[i]);
    if (encoder) encoders.push_back(encoder);
  }

  for (drmModeEncoder *encoder : encoders) {
    for (int crtc_index = 0; crtc_index < res->count_crtcs; ++crtc_index) {
      if ((encoder->possible_crtcs & (1 << crtc_index)) == 0) continue;
      if (!CrtcIndexAvailable(crtc_index, existing)) continue;
      out.connector_id = connector->connector_id;
      out.encoder_id = encoder->encoder_id;
      out.crtc_id = res->crtcs[crtc_index];
      out.crtc_index = crtc_index;
      out.mode = PickMode(connector);
      for (drmModeEncoder *owned : encoders) drmModeFreeEncoder(owned);
      return true;
    }
  }

  for (drmModeEncoder *owned : encoders) drmModeFreeEncoder(owned);
  return false;
}

std::vector<ConnectorPick> PickConnectedConnectors(int fd, drmModeRes *res) {
  std::vector<ConnectorPick> picks;
  for (int i = 0; i < res->count_connectors; ++i) {
    drmModeConnector *connector = drmModeGetConnector(fd, res->connectors[i]);
    if (!connector) continue;
    std::cout << "[rgds_drm_probe] connector id=" << connector->connector_id
              << " type=" << connector->connector_type
              << " status=" << connector->connection
              << " modes=" << connector->count_modes
              << " encoders=" << connector->count_encoders << "\n";
    if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
      ConnectorPick pick;
      if (PickCrtcForConnector(fd, res, connector, picks, pick)) {
        std::cout << "[rgds_drm_probe] picked connector=" << pick.connector_id
                  << " crtc=" << pick.crtc_id
                  << " mode=" << pick.mode.hdisplay << "x" << pick.mode.vdisplay
                  << "@" << pick.mode.vrefresh << "\n";
        picks.push_back(pick);
      } else {
        std::cout << "[rgds_drm_probe] no free crtc for connector=" << connector->connector_id << "\n";
      }
    }
    drmModeFreeConnector(connector);
    if (picks.size() >= 2) break;
  }
  return picks;
}

bool TryCard(const std::string &path, int seconds, int refresh_ms, bool no_restore) {
  std::cout << "[rgds_drm_probe] trying card=" << path << "\n";
  int fd = open(path.c_str(), O_RDWR | O_CLOEXEC);
  if (fd < 0) {
    std::cout << "[rgds_drm_probe] " << ErrnoText("open card failed") << "\n";
    return false;
  }

  uint64_t cap = 0;
  if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &cap) == 0) {
    std::cout << "[rgds_drm_probe] DRM_CAP_DUMB_BUFFER=" << cap << "\n";
  }

  drmModeRes *res = drmModeGetResources(fd);
  if (!res) {
    std::cout << "[rgds_drm_probe] " << ErrnoText("drmModeGetResources failed") << "\n";
    close(fd);
    return false;
  }
  std::cout << "[rgds_drm_probe] resources connectors=" << res->count_connectors
            << " crtcs=" << res->count_crtcs
            << " encoders=" << res->count_encoders << "\n";

  std::vector<ConnectorPick> picks = PickConnectedConnectors(fd, res);
  if (picks.size() < 2) {
    std::cout << "[rgds_drm_probe] need two connected connectors, got=" << picks.size() << "\n";
    drmModeFreeResources(res);
    close(fd);
    return false;
  }

  DumbBuffer top;
  DumbBuffer bottom;
  if (!CreateDumbBuffer(fd, top, 0x000035a8u) || !CreateDumbBuffer(fd, bottom, 0x00a83a35u)) {
    DestroyDumbBuffer(fd, top);
    DestroyDumbBuffer(fd, bottom);
    drmModeFreeResources(res);
    close(fd);
    return false;
  }

  bool set_ok = true;
  for (size_t i = 0; i < 2; ++i) {
    picks[i].old_crtc = drmModeGetCrtc(fd, picks[i].crtc_id);
    DumbBuffer &buffer = (i == 0) ? top : bottom;
    uint32_t connector_id = picks[i].connector_id;
    if (drmModeSetCrtc(fd, picks[i].crtc_id, buffer.fb, 0, 0, &connector_id, 1, &picks[i].mode) != 0) {
      std::cout << "[rgds_drm_probe] " << ErrnoText("drmModeSetCrtc failed")
                << " connector=" << connector_id
                << " crtc=" << picks[i].crtc_id << "\n";
      set_ok = false;
      break;
    }
    std::cout << "[rgds_drm_probe] set connector=" << connector_id
              << " crtc=" << picks[i].crtc_id
              << " fb=" << buffer.fb << "\n";
  }

  if (set_ok) {
    std::cout << "[rgds_drm_probe] holding image seconds=" << seconds
              << " refresh_ms=" << refresh_ms
              << " no_restore=" << no_restore << "\n";
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    int frame = 0;
    while (std::chrono::steady_clock::now() < end) {
      FillDumbBuffer(top, 0x000035a8u, frame);
      FillDumbBuffer(bottom, 0x00a83a35u, frame);
      for (size_t i = 0; i < 2; ++i) {
        DumbBuffer &buffer = (i == 0) ? top : bottom;
        uint32_t connector_id = picks[i].connector_id;
        if (drmModeSetCrtc(fd, picks[i].crtc_id, buffer.fb, 0, 0, &connector_id, 1, &picks[i].mode) != 0) {
          std::cout << "[rgds_drm_probe] refresh set failed frame=" << frame
                    << " index=" << i << " " << ErrnoText("drmModeSetCrtc") << "\n";
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(refresh_ms));
      ++frame;
    }
    std::cout << "[rgds_drm_probe] held frames=" << frame << "\n";
  }

  if (!no_restore) {
    for (auto &pick : picks) {
      if (pick.old_crtc) {
        drmModeSetCrtc(fd, pick.old_crtc->crtc_id, pick.old_crtc->buffer_id,
                       pick.old_crtc->x, pick.old_crtc->y,
                       &pick.connector_id, 1, &pick.old_crtc->mode);
        drmModeFreeCrtc(pick.old_crtc);
        pick.old_crtc = nullptr;
      }
    }
  } else {
    for (auto &pick : picks) {
      if (pick.old_crtc) {
        drmModeFreeCrtc(pick.old_crtc);
        pick.old_crtc = nullptr;
      }
    }
  }

  DestroyDumbBuffer(fd, top);
  DestroyDumbBuffer(fd, bottom);
  drmModeFreeResources(res);
  close(fd);
  return set_ok;
}

}  // namespace

int main() {
  const int seconds = ReadEnvInt("ROCREADER_RGDS_DRM_SECONDS", 12, 1, 120);
  const int refresh_ms = ReadEnvInt("ROCREADER_RGDS_DRM_REFRESH_MS", 250, 16, 5000);
  const bool no_restore = ReadEnvInt("ROCREADER_RGDS_DRM_NO_RESTORE", 0, 0, 1) == 1;
  std::cout << "[rgds_drm_probe] begin seconds=" << seconds
            << " refresh_ms=" << refresh_ms
            << " no_restore=" << no_restore << "\n";
  for (const char *card : {"/dev/dri/card0", "/dev/dri/card1", "/dev/dri/card2", "/dev/dri/card3"}) {
    if (TryCard(card, seconds, refresh_ms, no_restore)) {
      std::cout << "[rgds_drm_probe] success card=" << card << "\n";
      return 0;
    }
  }
  std::cout << "[rgds_drm_probe] no card succeeded\n";
  return 2;
}
