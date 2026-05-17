#include "rgds_dual_screen_runtime.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace rgds {

namespace {
std::string SdlErrorText(const char *prefix) {
  std::ostringstream out;
  out << prefix << ": " << SDL_GetError();
  return out.str();
}
}  // namespace

DualScreenRuntime::~DualScreenRuntime() {
  Shutdown();
}

DualScreenInitResult DualScreenRuntime::Initialize(const char *top_title, const char *bottom_title) {
  Shutdown();

  DualScreenInitResult result;
  result.display_count = SDL_GetNumVideoDisplays();
  if (result.display_count < 2) {
    result.error = "RGDS requires two SDL displays; detected " + std::to_string(result.display_count);
    return result;
  }

  const char *mode = std::getenv("ROCREADER_RGDS_LAYOUT");
  const bool use_spanning = mode && std::string(mode) == "spanning";
  if (use_spanning) {
    if (!CreateSpanningSurface(top_title, result.error)) {
      Shutdown();
      return result;
    }
    spanning_ = true;
  } else {
    if (!CreateSurface(top_, 0, top_title, 0, result.error)) {
      Shutdown();
      return result;
    }
    if (!CreateSurface(bottom_, 1, bottom_title, kScreenW, result.error)) {
      Shutdown();
      return result;
    }
  }

  ready_ = true;
  result.ok = true;
  return result;
}

void DualScreenRuntime::Shutdown() {
  if (reader_canvas_) SDL_DestroyTexture(reader_canvas_);
  reader_canvas_ = nullptr;
  if (top_buffer_) SDL_DestroyTexture(top_buffer_);
  if (bottom_buffer_) SDL_DestroyTexture(bottom_buffer_);
  top_buffer_ = nullptr;
  bottom_buffer_ = nullptr;

  if (shared_renderer_) SDL_DestroyRenderer(shared_renderer_);
  if (shared_window_) SDL_DestroyWindow(shared_window_);
  shared_renderer_ = nullptr;
  shared_window_ = nullptr;

  if (top_.renderer) SDL_DestroyRenderer(top_.renderer);
  if (bottom_.renderer) SDL_DestroyRenderer(bottom_.renderer);
  if (top_.window) SDL_DestroyWindow(top_.window);
  if (bottom_.window) SDL_DestroyWindow(bottom_.window);
  top_ = ScreenSurface{};
  bottom_ = ScreenSurface{};
  spanning_ = false;
  ready_ = false;
}

bool DualScreenRuntime::IsReady() const {
  if (!ready_) return false;
  if (spanning_) return shared_renderer_ != nullptr && shared_window_ != nullptr;
  return top_.renderer && bottom_.renderer;
}

ScreenSurface &DualScreenRuntime::Top() { return top_; }
ScreenSurface &DualScreenRuntime::Bottom() { return bottom_; }
const ScreenSurface &DualScreenRuntime::Top() const { return top_; }
const ScreenSurface &DualScreenRuntime::Bottom() const { return bottom_; }

SDL_Texture *DualScreenRuntime::EnsureReaderCanvas() {
  if (reader_canvas_) return reader_canvas_;
  SDL_Renderer *renderer = spanning_ ? shared_renderer_ : top_.renderer;
  if (!renderer) return nullptr;
  reader_canvas_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                     kVirtualReaderW, kVirtualReaderH);
  if (reader_canvas_) {
    SDL_SetTextureBlendMode(reader_canvas_, SDL_BLENDMODE_BLEND);
  }
  return reader_canvas_;
}

SDL_Texture *DualScreenRuntime::ReaderCanvas() const {
  return reader_canvas_;
}

void DualScreenRuntime::Clear(SDL_Renderer *renderer, SDL_Color color) const {
  if (!renderer) return;
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_RenderClear(renderer);
}

void DualScreenRuntime::ClearSurface(SurfaceId surface, SDL_Color color) const {
  if (spanning_) {
    SDL_Texture *buffer = SurfaceBuffer(surface);
    if (!buffer || !shared_renderer_) return;
    SDL_SetRenderTarget(shared_renderer_, buffer);
    SDL_SetRenderDrawColor(shared_renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderClear(shared_renderer_);
    SDL_SetRenderTarget(shared_renderer_, nullptr);
    return;
  }
  Clear(surface == SurfaceId::Top ? top_.renderer : bottom_.renderer, color);
}

SDL_Renderer *DualScreenRuntime::BeginSurface(SurfaceId surface) {
  if (!spanning_) {
    return surface == SurfaceId::Top ? top_.renderer : bottom_.renderer;
  }
  if (!shared_renderer_) return nullptr;
  SDL_Texture *buffer = SurfaceBuffer(surface);
  if (!buffer) return nullptr;
  SDL_SetRenderTarget(shared_renderer_, buffer);
  return shared_renderer_;
}

void DualScreenRuntime::EndSurface() {
  if (spanning_ && shared_renderer_) {
    SDL_SetRenderTarget(shared_renderer_, nullptr);
  }
}

void DualScreenRuntime::PresentBoth() const {
  if (spanning_) {
    if (!shared_renderer_) return;
    SDL_RenderClear(shared_renderer_);
    SDL_Rect top_dst{0, 0, kScreenW, kScreenH};
    SDL_Rect bottom_dst{kScreenW, 0, kScreenW, kScreenH};
    SDL_RenderCopy(shared_renderer_, top_buffer_, nullptr, &top_dst);
    SDL_RenderCopy(shared_renderer_, bottom_buffer_, nullptr, &bottom_dst);
    SDL_RenderPresent(shared_renderer_);
    return;
  }
  if (top_.renderer) SDL_RenderPresent(top_.renderer);
  if (bottom_.renderer) SDL_RenderPresent(bottom_.renderer);
}

void DualScreenRuntime::DrawFocusFrame(SurfaceId surface, float alpha) const {
  SDL_Renderer *renderer = nullptr;
  if (spanning_) {
    renderer = shared_renderer_;
    if (!renderer) return;
    SDL_Texture *buffer = SurfaceBuffer(surface);
    if (!buffer) return;
    SDL_SetRenderTarget(renderer, buffer);
  } else {
    const ScreenSurface &target = (surface == SurfaceId::Top) ? top_ : bottom_;
    renderer = target.renderer;
  }
  if (!renderer || alpha <= 0.001f) return;
  const int w = rgds::kScreenW;
  const int h = rgds::kScreenH;
  const Uint8 line_alpha = static_cast<Uint8>(std::clamp(alpha * 255.0f, 0.0f, 255.0f));
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 120, 210, 255, line_alpha);
  for (int i = 0; i < 3; ++i) {
    SDL_Rect border{i, i, std::max(1, w - i * 2), std::max(1, h - i * 2)};
    SDL_RenderDrawRect(renderer, &border);
  }
  if (spanning_ && shared_renderer_) {
    SDL_SetRenderTarget(shared_renderer_, nullptr);
  }
}

void DualScreenRuntime::PresentReaderCanvasSplit() const {
  if (!reader_canvas_) return;
  if (spanning_) {
    if (!shared_renderer_) return;
    SDL_Texture *top_buf = SurfaceBuffer(SurfaceId::Top);
    SDL_Texture *bottom_buf = SurfaceBuffer(SurfaceId::Bottom);
    if (!top_buf || !bottom_buf) return;
    SDL_SetRenderTarget(shared_renderer_, top_buf);
    SDL_Rect top_src{0, 0, kScreenW, kScreenH};
    SDL_Rect dst{0, 0, kScreenW, kScreenH};
    SDL_RenderCopy(shared_renderer_, reader_canvas_, &top_src, &dst);
    SDL_SetRenderTarget(shared_renderer_, bottom_buf);
    SDL_Rect bottom_src{0, kScreenH, kScreenW, kScreenH};
    SDL_RenderCopy(shared_renderer_, reader_canvas_, &bottom_src, &dst);
    SDL_SetRenderTarget(shared_renderer_, nullptr);
    return;
  }
  if (!top_.renderer || !bottom_.renderer) return;
  SDL_Rect top_src{0, 0, kScreenW, kScreenH};
  SDL_Rect bottom_src{0, kScreenH, kScreenW, kScreenH};
  SDL_Rect dst{0, 0, kScreenW, kScreenH};
  SDL_RenderCopy(top_.renderer, reader_canvas_, &top_src, &dst);
  SDL_RenderCopy(bottom_.renderer, reader_canvas_, &bottom_src, &dst);
}

bool DualScreenRuntime::CreateSurface(ScreenSurface &surface, int display_index, const char *title,
                                      int fallback_x, std::string &error) {
  SDL_Rect bounds{fallback_x, 0, kScreenW, kScreenH};
  SDL_GetDisplayBounds(display_index, &bounds);

  surface.display_index = display_index;
  surface.x = bounds.x;
  surface.y = bounds.y;
  surface.w = bounds.w > 0 ? bounds.w : kScreenW;
  surface.h = bounds.h > 0 ? bounds.h : kScreenH;

  surface.window = SDL_CreateWindow(title, bounds.x + 16, bounds.y + 16, kScreenW, kScreenH,
                                    SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);
  if (!surface.window) {
    error = SdlErrorText("SDL_CreateWindow failed");
    return false;
  }

  const bool force_vsync = std::getenv("ROCREADER_RGDS_FORCE_VSYNC") != nullptr;
  if (force_vsync) {
    surface.renderer = SDL_CreateRenderer(surface.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  }
  if (!surface.renderer) {
    surface.renderer = SDL_CreateRenderer(surface.window, -1, SDL_RENDERER_ACCELERATED);
  }
  if (!surface.renderer) {
    surface.renderer = SDL_CreateRenderer(surface.window, -1, SDL_RENDERER_SOFTWARE);
  }
  if (!surface.renderer) {
    error = SdlErrorText("SDL_CreateRenderer failed");
    return false;
  }
  SDL_RendererInfo info{};
  if (SDL_GetRendererInfo(surface.renderer, &info) == 0) {
    std::cerr << "[rgds_runtime] display=" << display_index
              << " renderer=" << (info.name ? info.name : "unknown")
              << " accelerated=" << ((info.flags & SDL_RENDERER_ACCELERATED) != 0)
              << " target_texture=" << ((info.flags & SDL_RENDERER_TARGETTEXTURE) != 0)
              << " vsync=" << ((info.flags & SDL_RENDERER_PRESENTVSYNC) != 0)
              << "\n";
  }
  return true;
}

bool DualScreenRuntime::CreateSpanningSurface(const char *title, std::string &error) {
  SDL_Rect bounds0{};
  SDL_Rect bounds1{};
  if (SDL_GetDisplayBounds(0, &bounds0) != 0 || SDL_GetDisplayBounds(1, &bounds1) != 0) {
    error = SdlErrorText("SDL_GetDisplayBounds failed");
    return false;
  }

  shared_window_ = SDL_CreateWindow(title, bounds0.x, bounds0.y, kScreenW * 2, kScreenH,
                                    SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);
  if (!shared_window_) {
    error = SdlErrorText("SDL_CreateWindow(spanning) failed");
    return false;
  }

  const bool force_vsync = std::getenv("ROCREADER_RGDS_FORCE_VSYNC") != nullptr;
  if (force_vsync) {
    shared_renderer_ = SDL_CreateRenderer(shared_window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  }
  if (!shared_renderer_) {
    shared_renderer_ = SDL_CreateRenderer(shared_window_, -1, SDL_RENDERER_ACCELERATED);
  }
  if (!shared_renderer_) {
    shared_renderer_ = SDL_CreateRenderer(shared_window_, -1, SDL_RENDERER_SOFTWARE);
  }
  if (!shared_renderer_) {
    error = SdlErrorText("SDL_CreateRenderer(spanning) failed");
    return false;
  }

  top_buffer_ = SDL_CreateTexture(shared_renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                  kScreenW, kScreenH);
  bottom_buffer_ = SDL_CreateTexture(shared_renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                     kScreenW, kScreenH);
  if (!top_buffer_ || !bottom_buffer_) {
    error = SdlErrorText("SDL_CreateTexture buffer failed");
    return false;
  }
  SDL_SetTextureBlendMode(top_buffer_, SDL_BLENDMODE_NONE);
  SDL_SetTextureBlendMode(bottom_buffer_, SDL_BLENDMODE_NONE);

  SDL_RendererInfo info{};
  if (SDL_GetRendererInfo(shared_renderer_, &info) == 0) {
    std::cerr << "[rgds_runtime] spanning renderer="
              << (info.name ? info.name : "unknown")
              << " accelerated=" << ((info.flags & SDL_RENDERER_ACCELERATED) != 0)
              << " target_texture=" << ((info.flags & SDL_RENDERER_TARGETTEXTURE) != 0)
              << " vsync=" << ((info.flags & SDL_RENDERER_PRESENTVSYNC) != 0)
              << "\n";
  }
  return true;
}

SDL_Texture *DualScreenRuntime::SurfaceBuffer(SurfaceId surface) const {
  return surface == SurfaceId::Top ? top_buffer_ : bottom_buffer_;
}

}  // namespace rgds
