#pragma once

#include "deepnest/MinkowskiConvolution.h"
#include "deepnest/NfpCache.h"

namespace deepnest {

class NfpGenerator {
 public:
  explicit NfpGenerator(NfpCache* cache);

  PolygonWithHoles Compute(const NestPolygon& a, const NestPolygon& b, bool inside);

 private:
  NfpCache* cache_;
  MinkowskiConvolution minkowski_;
};

}  // namespace deepnest
