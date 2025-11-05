#include "deepnest/NestEngine.h"

namespace deepnest {

NestEngine::NestEngine(const DeepNestConfig& config)
    : config_(config),
      cache_(),
      generator_(&cache_),
      repository_(config_),
      worker_(&generator_, config_),
      algorithm_(config_, &worker_) {}

void NestEngine::Initialize(const QPainterPath& sheet,
                            const QHash<QString, QPainterPath>& parts) {
  (void)sheet;
  repository_.LoadParts(parts);
  parts_ = repository_.items();
}

std::vector<NestPolygon> NestEngine::Run() {
  algorithm_.Run(parts_);
  return parts_;
}

}  // namespace deepnest
