#pragma once

#include "deepnest/GeometryTypes.h"

#include <QPointF>
#include <QString>

namespace deepnest {

class NestPolygon {
 public:
  NestPolygon();
  explicit NestPolygon(const PolygonWithHoles& polygon);

  const PolygonWithHoles& geometry() const { return geometry_; }
  void set_geometry(const PolygonWithHoles& polygon);

  const QString& id() const { return id_; }
  void set_id(const QString& id) { id_ = id; }

  double rotation() const { return rotation_; }
  void set_rotation(double rotation) { rotation_ = rotation; }

  QPointF position() const { return position_; }
  void set_position(const QPointF& pos) { position_ = pos; }

 private:
  QString id_;
  PolygonWithHoles geometry_;
  double rotation_;
  QPointF position_;
};

}  // namespace deepnest
