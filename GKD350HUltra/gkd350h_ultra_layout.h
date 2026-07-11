#pragma once

#include "../src/app_layout.h"

namespace gkd350h_ultra {

inline const LayoutMetrics &Layout1600x1440() {
  // Based on the 720x720 layout geometry at 2x scale.
  // The extra 160 horizontal pixels are applied by role:
  // - centered shelf/nav content: +80 px
  // - left-edge menu surfaces: no extra offset
  // - right-edge status/nav surfaces: +160 px
  static constexpr LayoutMetrics layout{
      1600, 1440, 40,
      0, 72,
      72, 116,
      208, 1112,
      1320, 120,
      280, 420,
      360, 500,
      66, 76,
      146, 200,
      72, 4, 8, 48,
      480, 0, 84,
      64, 52,
      42, 108,
      1494, 108,
      260, 270, 100,
      36, 32, 68, 68,
      4, 3,
      2.0f,
  };
  return layout;
}

}  // namespace gkd350h_ultra
