#pragma once

#include "deepnest/GeometryTypes.h"

#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>

namespace deepnest {

Point QPointFToPoint(const QPointF& point, Coordinate scale = kDefaultScale);
QPointF PointToQPointF(const Point& point, Coordinate scale = kDefaultScale);

Loop QPolygonFToLoop(const QPolygonF& polygon, Coordinate scale = kDefaultScale);
QPolygonF LoopToQPolygonF(const Loop& loop, Coordinate scale = kDefaultScale);

PolygonWithHoles QPainterPathToPolygonWithHoles(const QPainterPath& path,
                                               Coordinate scale = kDefaultScale);
QPainterPath PolygonWithHolesToQPainterPath(const PolygonWithHoles& polygon,
                                            Coordinate scale = kDefaultScale);

}  // namespace deepnest
