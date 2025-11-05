#pragma once

#include "deepnest/GeometryTypes.h"

#include <QPointF>
#include <QString>

#include <vector>

namespace deepnest {

class NestPolygon {
 public:
  NestPolygon();
  explicit NestPolygon(const PolygonWithHoles& polygon);

  const PolygonWithHoles& geometry() const { return geometry_; }
  void set_geometry(const PolygonWithHoles& polygon);

  const QString& id() const { return id_; }
  void set_id(const QString& id) { id_ = id; }

  const QString& source() const { return source_; }
  void set_source(const QString& source) { source_ = source; }

  double rotation() const { return rotation_; }
  void set_rotation(double rotation) { rotation_ = rotation; }

  QPointF position() const { return position_; }
  void set_position(const QPointF& pos) { position_ = pos; }

  bool exact() const { return exact_; }
  void set_exact(bool exact) { exact_ = exact; }

  const std::vector<bool>& outer_vertex_exact() const { return outer_exact_; }
  void set_outer_vertex_exact(const std::vector<bool>& flags);

  const std::vector<std::vector<bool>>& inner_vertex_exact() const {
    return inner_exact_;
  }
  void set_inner_vertex_exact(std::size_t index,
                              const std::vector<bool>& flags);

  const std::vector<NestPolygon>& children() const { return children_; }
  std::vector<NestPolygon>& mutable_children() { return children_; }
  void AddChild(const NestPolygon& child);
  void ClearChildren();

  NestPolygon Clone() const;

  void NormalizeOrientation();
  void Translate(Coordinate dx, Coordinate dy);
  void Rotate(Coordinate degrees);

 private:
  void UpdateExactMetadata();

  QString id_;
  QString source_;
  PolygonWithHoles geometry_;
  double rotation_;
  QPointF position_;
  bool exact_;
  std::vector<bool> outer_exact_;
  std::vector<std::vector<bool>> inner_exact_;
  std::vector<NestPolygon> children_;
};

}  // namespace deepnest
