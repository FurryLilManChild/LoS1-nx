/**
 * @file    gpu/render/render_scale.h
 * @brief   Render resolution: host pixels per guest pixel.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace renut::gpu {

// Host pixels per guest pixel of a render target, and of what resolves from
// it, as one of a fixed set of ratios: the render resolutions
// (renut_resolution: 1x, 480p, 1080p, 1440p, 4K, and 540p) and the targets
// that render finer than the rest (graphics_options.h: shadow maps at 4x,
// ambient occlusion at twice the resolution: 6x). LoS1-nx (2026-10-28): code 7
// is 3/4 (540p, 960x540: the Switch's stock clocks), no longer 4/3 (the full
// ambient occlusion of 480p, which the deko3d renderer never draws; on the PC
// it now stays at the scene's scale). Code 0 is 1x, the scale of all
// content that is not rendered (guest textures). Shaders get the code in the
// descriptor index (kResolutionScaleShift) and have the same factors
// (renutTextureScale in shader_common.h).
struct RenderScale {
  static constexpr uint32_t kCount = 8;
  static constexpr uint32_t kNumerators[kCount] = {1, 2, 3, 2, 3, 4, 6, 3};
  static constexpr uint32_t kDenominators[kCount] = {1, 3, 2, 1, 1, 1, 1, 4};

  uint32_t code = 0;

  float Factor() const { return float(kNumerators[code]) / float(kDenominators[code]); }
  bool operator==(const RenderScale &) const = default;

  // A guest pixel edge in host pixels, to the nearest: rects that share an
  // edge in guest pixels share it in host pixels.
  int32_t Coord(int32_t guest) const {
    return int32_t(std::lround(double(guest) * kNumerators[code] / kDenominators[code]));
  }

  // A guest size in host pixels, 1 at least.
  uint32_t Size(uint32_t guest) const {
    return std::max<uint32_t>(uint32_t(Coord(int32_t(guest))), 1u);
  }

  // The scale of ratio numerator / denominator, if it is one of them.
  static bool FromRatio(uint32_t numerator, uint32_t denominator, RenderScale &out) {
    for (uint32_t c = 0; c < kCount; ++c) {
      if (uint64_t(numerator) * kDenominators[c] == uint64_t(kNumerators[c]) * denominator) {
        out.code = c;
        return true;
      }
    }
    return false;
  }

  // This scale times 'multiplier', or this one when that is not among them.
  RenderScale Times(uint32_t multiplier) const {
    RenderScale out = *this;
    FromRatio(kNumerators[code] * multiplier, kDenominators[code], out);
    return out;
  }
};

} // namespace renut::gpu
