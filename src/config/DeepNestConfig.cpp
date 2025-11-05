#include "deepnest/DeepNestConfig.h"

#include "deepnest/ClipperBridge.h"
#include "deepnest/GeometryUtils.h"

#include <algorithm>
#include <cmath>

namespace deepnest {

namespace {

double ClampPositive(double value, double fallback) {
  if (std::isnan(value) || value <= 0.0) {
    return fallback;
  }
  return value;
}

double ClampNonNegative(double value, double fallback = 0.0) {
  if (std::isnan(value) || value < 0.0) {
    return fallback;
  }
  return value;
}

template <typename T>
T ClampRange(T value, T min_value, T max_value) {
  return std::min(std::max(value, min_value), max_value);
}

template <typename Setter>
void ApplyIfPresent(const QVariantMap& map, const char* key, Setter setter) {
  const auto it = map.constFind(QString::fromUtf8(key));
  if (it != map.constEnd()) {
    setter(*it);
  }
}

}  // namespace

DeepNestConfig::DeepNestConfig()
    : population_size_(10),
      generations_(20),
      threads_(4),
      spacing_(0.0),
      rotations_(4),
      rotation_step_(90.0),
      mutation_rate_(10),
      curve_tolerance_(0.3),
      endpoint_tolerance_(2.0),
      clipper_scale_(kClipperScale),
      placement_type_(QStringLiteral("gravity")),
      merge_lines_(true),
      simplify_(false),
      use_holes_(false),
      explore_concave_(false),
      time_ratio_(0.5),
      scale_(72.0) {}

void DeepNestConfig::set_population_size(int value) {
  population_size_ = std::max(1, value);
}

void DeepNestConfig::set_generations(int value) {
  generations_ = std::max(1, value);
}

void DeepNestConfig::set_threads(int value) {
  threads_ = ClampRange(value, 1, 8);
}

void DeepNestConfig::set_spacing(double value) {
  spacing_ = ClampNonNegative(value, 0.0);
}

void DeepNestConfig::set_rotations(int value) {
  rotations_ = std::max(1, value);
  rotation_step_ = 360.0 / static_cast<double>(rotations_);
}

void DeepNestConfig::set_rotation_step(double value) {
  if (value <= 0.0 || std::isnan(value)) {
    return;
  }
  rotation_step_ = value;
  rotations_ = std::max(1, static_cast<int>(std::round(360.0 / value)));
}

void DeepNestConfig::set_mutation_rate(int value) {
  mutation_rate_ = std::max(0, value);
}

void DeepNestConfig::set_curve_tolerance(double value) {
  curve_tolerance_ = ClampPositive(value, curve_tolerance_);
}

void DeepNestConfig::set_endpoint_tolerance(double value) {
  endpoint_tolerance_ = ClampNonNegative(value, endpoint_tolerance_);
}

void DeepNestConfig::set_clipper_scale(double value) {
  clipper_scale_ = ClampPositive(value, clipper_scale_);
}

void DeepNestConfig::set_placement_type(const QString& value) {
  if (value.isEmpty()) {
    return;
  }
  placement_type_ = value;
}

void DeepNestConfig::set_merge_lines(bool value) {
  merge_lines_ = value;
}

void DeepNestConfig::set_simplify(bool value) {
  simplify_ = value;
}

void DeepNestConfig::set_use_holes(bool value) {
  use_holes_ = value;
}

void DeepNestConfig::set_explore_concave(bool value) {
  explore_concave_ = value;
}

void DeepNestConfig::set_time_ratio(double value) {
  time_ratio_ = ClampRange(value, 0.0, 1.0);
}

void DeepNestConfig::set_scale(double value) {
  scale_ = ClampPositive(value, scale_);
}

QVariantMap DeepNestConfig::ToVariantMap() const {
  QVariantMap map;
  map.insert(QStringLiteral("populationSize"), population_size_);
  map.insert(QStringLiteral("generations"), generations_);
  map.insert(QStringLiteral("threads"), threads_);
  map.insert(QStringLiteral("spacing"), spacing_);
  map.insert(QStringLiteral("rotations"), rotations_);
  map.insert(QStringLiteral("rotationStep"), rotation_step_);
  map.insert(QStringLiteral("mutationRate"), mutation_rate_);
  map.insert(QStringLiteral("curveTolerance"), curve_tolerance_);
  map.insert(QStringLiteral("endpointTolerance"), endpoint_tolerance_);
  map.insert(QStringLiteral("clipperScale"), clipper_scale_);
  map.insert(QStringLiteral("placementType"), placement_type_);
  map.insert(QStringLiteral("mergeLines"), merge_lines_);
  map.insert(QStringLiteral("simplify"), simplify_);
  map.insert(QStringLiteral("useHoles"), use_holes_);
  map.insert(QStringLiteral("exploreConcave"), explore_concave_);
  map.insert(QStringLiteral("timeRatio"), time_ratio_);
  map.insert(QStringLiteral("scale"), scale_);
  return map;
}

void DeepNestConfig::FromVariantMap(const QVariantMap& values) {
  ApplyIfPresent(values, "population", [this](const QVariant& value) {
    set_population_size(value.toInt());
  });
  ApplyIfPresent(values, "populationSize", [this](const QVariant& value) {
    set_population_size(value.toInt());
  });
  ApplyIfPresent(values, "generations", [this](const QVariant& value) {
    set_generations(value.toInt());
  });
  ApplyIfPresent(values, "threads", [this](const QVariant& value) {
    set_threads(value.toInt());
  });
  ApplyIfPresent(values, "spacing", [this](const QVariant& value) {
    set_spacing(value.toDouble());
  });
  ApplyIfPresent(values, "rotations", [this](const QVariant& value) {
    set_rotations(value.toInt());
  });
  ApplyIfPresent(values, "rotationStep", [this](const QVariant& value) {
    set_rotation_step(value.toDouble());
  });
  ApplyIfPresent(values, "mutationRate", [this](const QVariant& value) {
    set_mutation_rate(value.toInt());
  });
  ApplyIfPresent(values, "curveTolerance", [this](const QVariant& value) {
    set_curve_tolerance(value.toDouble());
  });
  ApplyIfPresent(values, "endpointTolerance", [this](const QVariant& value) {
    set_endpoint_tolerance(value.toDouble());
  });
  ApplyIfPresent(values, "clipperScale", [this](const QVariant& value) {
    set_clipper_scale(value.toDouble());
  });
  ApplyIfPresent(values, "placementType", [this](const QVariant& value) {
    set_placement_type(value.toString());
  });
  ApplyIfPresent(values, "mergeLines", [this](const QVariant& value) {
    set_merge_lines(value.toBool());
  });
  ApplyIfPresent(values, "simplify", [this](const QVariant& value) {
    set_simplify(value.toBool());
  });
  ApplyIfPresent(values, "useHoles", [this](const QVariant& value) {
    set_use_holes(value.toBool());
  });
  ApplyIfPresent(values, "exploreConcave", [this](const QVariant& value) {
    set_explore_concave(value.toBool());
  });
  ApplyIfPresent(values, "timeRatio", [this](const QVariant& value) {
    set_time_ratio(value.toDouble());
  });
  ApplyIfPresent(values, "scale", [this](const QVariant& value) {
    set_scale(value.toDouble());
  });
}

bool DeepNestConfig::operator==(const DeepNestConfig& other) const {
  return population_size_ == other.population_size_ &&
         generations_ == other.generations_ && threads_ == other.threads_ &&
         AlmostEqual(spacing_, other.spacing_) &&
         rotations_ == other.rotations_ &&
         AlmostEqual(rotation_step_, other.rotation_step_) &&
         mutation_rate_ == other.mutation_rate_ &&
         AlmostEqual(curve_tolerance_, other.curve_tolerance_) &&
         AlmostEqual(endpoint_tolerance_, other.endpoint_tolerance_) &&
         AlmostEqual(clipper_scale_, other.clipper_scale_) &&
         placement_type_ == other.placement_type_ &&
         merge_lines_ == other.merge_lines_ &&
         simplify_ == other.simplify_ && use_holes_ == other.use_holes_ &&
         explore_concave_ == other.explore_concave_ &&
         AlmostEqual(time_ratio_, other.time_ratio_) &&
         AlmostEqual(scale_, other.scale_);
}

}  // namespace deepnest
