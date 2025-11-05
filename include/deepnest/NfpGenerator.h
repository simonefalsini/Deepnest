#pragma once

#include "deepnest/ClipperBridge.h"
#include "deepnest/DeepNestConfig.h"
#include "deepnest/MinkowskiConvolution.h"
#include "deepnest/NfpCache.h"

namespace deepnest {

class NfpGenerator {
 public:
  NfpGenerator(NfpCache* cache, const DeepNestConfig& config);

  PolygonCollection Compute(const NestPolygon& a, const NestPolygon& b,
                            bool inside);

 private:
  PolygonCollection ComputeOuter(const NestPolygon& a, const NestPolygon& b);
  PolygonCollection ComputeInner(const NestPolygon& a, const NestPolygon& b);
  PolygonCollection CleanResult(const PolygonCollection& polygons) const;

  NfpCache* cache_;
  const DeepNestConfig& config_;
  MinkowskiConvolution minkowski_;
};

}  // namespace deepnest
