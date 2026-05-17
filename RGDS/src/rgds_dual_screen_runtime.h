#pragma once

#include <SDL.h>

#include <cstdint>
#include <string>

namespace rgds {

constexpr int kScreenW = 640;
constexpr int kScreenH = 480;
constexpr int kVirtualReaderW = 640;
constexpr int kVirtualReaderH = 960;

enum class SurfaceId {
  Top,
  Bottom,
};

struct ScreenSurface {
  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
  int display_index = 0;
  int x = 0;
  int y = 0;
  int w = kScreenW;
  int h = kScreenH;
};

struct DualScreenInitResult {
  bool ok = false;
  std::string error;
  int display_count = 0;
};

class DualScreenRuntime {
public:
  DualScreenRuntime() = default;
  ~DualScreenRuntime();

  DualScreenRuntime(const DualScreenRuntime &) = delete;
  DualScreenRuntime &operator=(const DualScreenRuntime &) = delete;

  DualScreenInitResult Initialize(const char *top_title, const char *bottom_title);
  void Shutdown();

  bool IsReady() const;
  ScreenSurface &Top();
  ScreenSurface &Bottom();
  const ScreenSurface &Top() const;
  const ScreenSurface &Bottom() const;

  SDL_Texture *EnsureReaderCanvas();
  SDL_Texture *ReaderCanvas() const;

  void Clear(SDL_Renderer *renderer, SDL_Color color) const;
  void ClearSurface(SurfaceId surface, SDL_Color color) const;
  SDL_Renderer *BeginSurface(SurfaceId surface);
  void EndSurface();
  void PresentBoth() const;
  void DrawFocusFrame(SurfaceId surface, float alpha) const;
  void PresentReaderCanvasSplit() const;

private:
  bool CreateSurface(ScreenSurface &surface, int display_index, const char *title, int fallback_x, std::string &error);
  bool CreateSpanningSurface(const char *title, std::string &error);
  SDL_Texture *SurfaceBuffer(SurfaceId surface) const;

  ScreenSurface top_{};
  ScreenSurface bottom_{};
  SDL_Window *shared_window_ = nullptr;
  SDL_Renderer *shared_renderer_ = nullptr;
  SDL_Texture *top_buffer_ = nullptr;
  SDL_Texture *bottom_buffer_ = nullptr;
  SDL_Texture *reader_canvas_ = nullptr;
  bool spanning_ = false;
  bool ready_ = false;
};

}  // namespace rgds
