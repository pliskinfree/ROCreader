#pragma once

#include <algorithm>
#include <cmath>

namespace gkd_menu {

inline int ScalePx(float scale, int value) {
  return std::max(1, static_cast<int>(std::round(static_cast<float>(value) * std::max(0.1f, scale))));
}

inline int ControlH(float scale) { return ScalePx(scale, 42); }
inline int StepButtonW(float scale) { return ScalePx(scale, 42); }
inline int LongButtonW(float scale) { return ScalePx(scale, 138); }
inline int WideButtonW(float scale) { return ScalePx(scale, 176); }
inline int RightPadding(float scale) { return ScalePx(scale, 28); }
inline int ButtonGap(float scale) { return ScalePx(scale, 14); }

}  // namespace gkd_menu
