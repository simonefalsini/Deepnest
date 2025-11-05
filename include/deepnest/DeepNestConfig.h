#pragma once

#include <QVariantMap>

#include <QString>

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

  int rotations() const { return rotations_; }
  void set_rotations(int value);

  double rotation_step() const { return rotation_step_; }
  void set_rotation_step(double value);

  int mutation_rate() const { return mutation_rate_; }
  void set_mutation_rate(int value);

  double curve_tolerance() const { return curve_tolerance_; }
  void set_curve_tolerance(double value);

  double endpoint_tolerance() const { return endpoint_tolerance_; }
  void set_endpoint_tolerance(double value);

  double clipper_scale() const { return clipper_scale_; }
  void set_clipper_scale(double value);

  const QString& placement_type() const { return placement_type_; }
  void set_placement_type(const QString& value);

  bool merge_lines() const { return merge_lines_; }
  void set_merge_lines(bool value);

  bool simplify() const { return simplify_; }
  void set_simplify(bool value);

  bool use_holes() const { return use_holes_; }
  void set_use_holes(bool value);

  bool explore_concave() const { return explore_concave_; }
  void set_explore_concave(bool value);

  double time_ratio() const { return time_ratio_; }
  void set_time_ratio(double value);

  double scale() const { return scale_; }
  void set_scale(double value);

  QVariantMap ToVariantMap() const;
  void FromVariantMap(const QVariantMap& values);

  bool operator==(const DeepNestConfig& other) const;
  bool operator!=(const DeepNestConfig& other) const {
    return !(*this == other);
  }

 private:
  int population_size_;
  int generations_;
  int threads_;
  double spacing_;
  int rotations_;
  double rotation_step_;
  int mutation_rate_;
  double curve_tolerance_;
  double endpoint_tolerance_;
  double clipper_scale_;
  QString placement_type_;
  bool merge_lines_;
  bool simplify_;
  bool use_holes_;
  bool explore_concave_;
  double time_ratio_;
  double scale_;
};

}  // namespace deepnest
