#pragma once

#include "deepnest/GeometryTypes.h"

namespace deepnest {

class MinkowskiConvolution {
 public:
  PolygonCollection ComputeAll(const PolygonWithHoles& a,
                               const PolygonWithHoles& b) const;

  PolygonWithHoles Compute(const PolygonWithHoles& a,
                           const PolygonWithHoles& b) const;
};

}  // namespace deepnest
