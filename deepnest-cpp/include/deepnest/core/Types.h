#ifndef DEEPNEST_TYPES_H
#define DEEPNEST_TYPES_H

// Define M_PI for MSVC
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <boost/polygon/polygon.hpp>
#include <vector>
#include <memory>

namespace deepnest {

// Forward declarations
class Polygon;
struct Point;

// Boost.Polygon types for internal geometry representation
using BoostPoint = boost::polygon::point_data<double>;
using BoostPolygon = boost::polygon::polygon_data<double>;
using BoostPolygonWithHoles = boost::polygon::polygon_with_holes_data<double>;
using BoostPolygonSet = boost::polygon::polygon_set_data<double>;
using BoostRing = std::vector<BoostPoint>;

// Floating point comparison tolerance
constexpr double TOL = 1e-9;  // Same as JavaScript TOL = Math.pow(10, -9)

// Common typedefs
using PolygonPtr = std::shared_ptr<Polygon>;
using PolygonList = std::vector<Polygon>;
using PolygonPtrList = std::vector<PolygonPtr>;

} // namespace deepnest

#endif // DEEPNEST_TYPES_H
