#include "deepnest/DeepNestConfig.h"

#include <algorithm>

namespace deepnest {

DeepNestConfig::DeepNestConfig()
    : population_size_(10),
      generations_(20),
      threads_(1),
      spacing_(0.0),
      rotation_step_(90.0) {}

void DeepNestConfig::set_population_size(int value) {
  population_size_ = std::max(1, value);
}

void DeepNestConfig::set_generations(int value) {
  generations_ = std::max(1, value);
}

void DeepNestConfig::set_threads(int value) {
  threads_ = std::max(1, std::min(8, value));
}

void DeepNestConfig::set_spacing(double value) {
  spacing_ = std::max(0.0, value);
}

void DeepNestConfig::set_rotation_step(double value) {
  rotation_step_ = value;
}

QVariantMap DeepNestConfig::ToVariantMap() const {
  QVariantMap map;
  map.insert("population", population_size_);
  map.insert("generations", generations_);
  map.insert("threads", threads_);
  map.insert("spacing", spacing_);
  map.insert("rotationStep", rotation_step_);
  return map;
}

void DeepNestConfig::FromVariantMap(const QVariantMap& values) {
  if (values.contains("population")) {
    set_population_size(values.value("population").toInt());
  }
  if (values.contains("generations")) {
    set_generations(values.value("generations").toInt());
  }
  if (values.contains("threads")) {
    set_threads(values.value("threads").toInt());
  }
  if (values.contains("spacing")) {
    set_spacing(values.value("spacing").toDouble());
  }
  if (values.contains("rotationStep")) {
    set_rotation_step(values.value("rotationStep").toDouble());
  }
}

}  // namespace deepnest
