#pragma once

#include "deepnest/GeometryTypes.h"

#include <clipper2/clipper.h>

namespace deepnest {

clipper2::PathsD PolygonToClipper(const PolygonWithHoles& polygon);
PolygonWithHoles ClipperToPolygon(const clipper2::PathsD& paths);
PolygonWithHoles OffsetPolygon(const PolygonWithHoles& polygon, Coordinate delta);

}  // namespace deepnest
