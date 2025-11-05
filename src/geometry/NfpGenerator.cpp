#include "deepnest/NfpGenerator.h"

namespace deepnest {

NfpGenerator::NfpGenerator(NfpCache* cache) : cache_(cache) {}

PolygonWithHoles NfpGenerator::Compute(const NestPolygon& a, const NestPolygon& b,
                                       bool inside) {
  NfpKey key;
  key.a = a.id();
  key.b = b.id();
  key.rotation_a = a.rotation();
  key.rotation_b = b.rotation();
  key.inside = inside;

  PolygonWithHoles cached;
  if (cache_ && cache_->TryGet(key, &cached)) {
    return cached;
  }

  PolygonWithHoles result = minkowski_.Compute(a.geometry(), b.geometry());
  if (cache_) {
    cache_->Store(key, result);
  }
  return result;
}

}  // namespace deepnest
