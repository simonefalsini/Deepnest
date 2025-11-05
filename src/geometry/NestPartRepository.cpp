#include "deepnest/NestPartRepository.h"

namespace deepnest {

NestPartRepository::NestPartRepository(const DeepNestConfig& config)
    : config_(config) {}

void NestPartRepository::LoadParts(const QHash<QString, QPainterPath>& parts) {
  parts_.clear();
  parts_.reserve(parts.size());
  for (auto it = parts.constBegin(); it != parts.constEnd(); ++it) {
    NestPolygon poly(QPainterPathToPolygonWithHoles(it.value()));
    poly.set_id(it.key());
    parts_.push_back(poly);
  }
}

}  // namespace deepnest
