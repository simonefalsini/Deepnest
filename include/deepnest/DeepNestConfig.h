#pragma once

#include <QVariantMap>

namespace deepnest {

class DeepNestConfig {
 public:
  DeepNestConfig();

  int population_size() const { return population_size_; }
  void set_population_size(int value);

  int generations() const { return generations_; }
  void set_generations(int value);

  int threads() const { return threads_; }
  void set_threads(int value);

  double spacing() const { return spacing_; }
  void set_spacing(double value);

  double rotation_step() const { return rotation_step_; }
  void set_rotation_step(double value);

  QVariantMap ToVariantMap() const;
  void FromVariantMap(const QVariantMap& values);

 private:
  int population_size_;
  int generations_;
  int threads_;
  double spacing_;
  double rotation_step_;
};

}  // namespace deepnest
