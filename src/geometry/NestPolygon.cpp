#include "deepnest/NestPolygon.h"

namespace deepnest {

NestPolygon::NestPolygon() : rotation_(0.0), position_(0.0, 0.0) {}

NestPolygon::NestPolygon(const PolygonWithHoles& polygon)
    : geometry_(polygon), rotation_(0.0), position_(0.0, 0.0) {}

void NestPolygon::set_geometry(const PolygonWithHoles& polygon) {
  geometry_ = polygon;
}

}  // namespace deepnest
