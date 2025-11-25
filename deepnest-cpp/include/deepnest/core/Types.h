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

// ============================================================================
// INTEGER COORDINATE SYSTEM
// ============================================================================
// All internal coordinates use int64_t for precision without overflow.
// Conversion from/to double happens ONLY at I/O boundaries (SVG, QPainterPath)
// using the inputScale parameter from DeepNestConfig.
//
// Benefits:
// - Exact arithmetic (no floating point errors)
// - Clipper2 native support (no scaling needed)
// - Faster comparisons and operations
// - Predictable behavior
//
// With int64_t max = 9,223,372,036,854,775,807:
// - inputScale=10000: max coordinate ≈ 922 million units (safe for <10km shapes)
// - inputScale=1000: max coordinate ≈ 9.2 billion units (safe for huge shapes)
// ============================================================================

// Base coordinate type - int64_t for precision without overflow
using CoordType = int64_t;

// Boost.Polygon types using integer coordinates
using BoostPoint = boost::polygon::point_data<CoordType>;
using BoostPolygon = boost::polygon::polygon_data<CoordType>;
using BoostPolygonWithHoles = boost::polygon::polygon_with_holes_data<CoordType>;
using BoostPolygonSet = boost::polygon::polygon_set_data<CoordType>;
using BoostRing = std::vector<BoostPoint>;

// Integer tolerance (1 = minimum distinguishable distance in integer space)
// With inputScale=10000, TOL=1 represents 0.0001 units in user space
constexpr CoordType TOL = 1;

// Common typedefs
using PolygonPtr = std::shared_ptr<Polygon>;
using PolygonList = std::vector<Polygon>;
using PolygonPtrList = std::vector<PolygonPtr>;

} // namespace deepnest

#endif // DEEPNEST_TYPES_H
