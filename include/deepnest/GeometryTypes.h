#pragma once

#include <boost/polygon/point_data.hpp>
#include <boost/polygon/polygon.hpp>
#include <boost/polygon/polygon_set_data.hpp>
#include <boost/polygon/polygon_with_holes_data.hpp>
#include <boost/polygon/voronoi.hpp>

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace deepnest {

using Coordinate = double;
using Point = boost::polygon::point_data<Coordinate>;
using Segment = boost::polygon::segment_data<Coordinate>;
using Polygon = boost::polygon::polygon_data<Coordinate>;
using PolygonWithHoles = boost::polygon::polygon_with_holes_data<Coordinate>;
using PolygonSet = boost::polygon::polygon_set_data<Coordinate>;
using PolygonCollection = std::vector<PolygonWithHoles>;
using Loop = std::vector<Point>;
using LoopCollection = std::vector<Loop>;

constexpr Coordinate kDefaultScale = 1000.0;
constexpr Coordinate kTolerance = 1e-9;
constexpr Coordinate kPi = 3.141592653589793238462643383279502884L;

inline Coordinate DegreesToRadians(Coordinate degrees) {
  return degrees * (kPi / 180.0);
}

inline Coordinate RadiansToDegrees(Coordinate radians) {
  return radians * (180.0 / kPi);
}

inline bool AlmostEqual(Coordinate lhs, Coordinate rhs,
                        Coordinate tolerance = kTolerance) {
  return std::abs(lhs - rhs) <= tolerance;
}

enum class Orientation {
  kClockwise,
  kCounterClockwise,
  kCollinear,
};

struct BoundingBox {
  Coordinate min_x {std::numeric_limits<Coordinate>::infinity()};
  Coordinate min_y {std::numeric_limits<Coordinate>::infinity()};
  Coordinate max_x {-std::numeric_limits<Coordinate>::infinity()};
  Coordinate max_y {-std::numeric_limits<Coordinate>::infinity()};

  static BoundingBox Empty() { return {}; }

  bool IsEmpty() const {
    return min_x > max_x || min_y > max_y;
  }

  Coordinate Width() const {
    return IsEmpty() ? 0 : (max_x - min_x);
  }

  Coordinate Height() const {
    return IsEmpty() ? 0 : (max_y - min_y);
  }

  Point Center() const {
    if (IsEmpty()) {
      return Point(0, 0);
    }
    return Point((min_x + max_x) / 2.0, (min_y + max_y) / 2.0);
  }

  void Expand(const Point& point) {
    if (IsEmpty()) {
      min_x = max_x = point.x();
      min_y = max_y = point.y();
      return;
    }
    min_x = std::min(min_x, point.x());
    max_x = std::max(max_x, point.x());
    min_y = std::min(min_y, point.y());
    max_y = std::max(max_y, point.y());
  }

  void Expand(const BoundingBox& other) {
    if (other.IsEmpty()) {
      return;
    }
    if (IsEmpty()) {
      *this = other;
      return;
    }
    min_x = std::min(min_x, other.min_x);
    max_x = std::max(max_x, other.max_x);
    min_y = std::min(min_y, other.min_y);
    max_y = std::max(max_y, other.max_y);
  }
};

inline Orientation OrientationFromSignedArea(Coordinate area) {
  if (area > kTolerance) {
    return Orientation::kCounterClockwise;
  }
  if (area < -kTolerance) {
    return Orientation::kClockwise;
  }
  return Orientation::kCollinear;
}

inline Coordinate SignedArea(const Loop& loop) {
  const std::size_t size = loop.size();
  if (size < 3) {
    return 0.0;
  }

  Coordinate area = 0.0;
  for (std::size_t i = 0; i < size; ++i) {
    const auto& current = loop[i];
    const auto& next = loop[(i + 1) % size];
    area += (current.x() * next.y()) - (next.x() * current.y());
  }
  return area / 2.0;
}

inline Orientation OrientationOfLoop(const Loop& loop) {
  return OrientationFromSignedArea(SignedArea(loop));
}

inline Orientation OrientationOfPolygon(const Polygon& polygon) {
  return OrientationFromSignedArea(boost::polygon::area(polygon));
}

namespace detail {

template <typename T, typename = void>
struct has_outer_method : std::false_type {};

template <typename T>
struct has_outer_method<T, std::void_t<decltype(std::declval<T&>().outer())>>
    : std::true_type {};

template <typename T, typename = void>
struct has_polygon_method : std::false_type {};

template <typename T>
struct has_polygon_method<T, std::void_t<decltype(std::declval<T&>().polygon())>>
    : std::true_type {};

template <typename T, typename = void>
struct has_inners_method : std::false_type {};

template <typename T>
struct has_inners_method<T, std::void_t<decltype(std::declval<T&>().inners())>>
    : std::true_type {};

template <typename T, typename = void>
struct has_holes_method : std::false_type {};

template <typename T>
struct has_holes_method<T, std::void_t<decltype(std::declval<T&>().holes())>>
    : std::true_type {};

template <typename Poly>
decltype(auto) AccessOuter(Poly& polygon) {
  if constexpr (has_outer_method<Poly>::value) {
    return (polygon.outer());
  } else {
    static_assert(has_polygon_method<Poly>::value,
                  "polygon_with_holes_data requires outer() or polygon() accessor");
    return (polygon.polygon());
  }
}

template <typename Poly>
decltype(auto) AccessHoles(Poly& polygon) {
  if constexpr (has_inners_method<Poly>::value) {
    return (polygon.inners());
  } else {
    static_assert(has_holes_method<Poly>::value,
                  "polygon_with_holes_data requires inners() or holes() accessor");
    return (polygon.holes());
  }
}

template <typename Poly, typename = void>
struct has_set_outer_function : std::false_type {};

template <typename Poly>
struct has_set_outer_function<
    Poly, std::void_t<decltype(boost::polygon::set_outer(
              std::declval<Poly&>(), std::declval<const Polygon&>()))>>
    : std::true_type {};

template <typename Poly, typename = void>
struct has_add_hole_function : std::false_type {};

template <typename Poly>
struct has_add_hole_function<
    Poly, std::void_t<decltype(boost::polygon::add_hole(
              std::declval<Poly&>(), std::declval<const Polygon&>()))>>
    : std::true_type {};

}  // namespace detail

inline Polygon& Outer(PolygonWithHoles& polygon) {
  return detail::AccessOuter(polygon);
}

inline const Polygon& Outer(const PolygonWithHoles& polygon) {
  return detail::AccessOuter(polygon);
}

inline std::vector<Polygon>& Holes(PolygonWithHoles& polygon) {
  return detail::AccessHoles(polygon);
}

inline const std::vector<Polygon>& Holes(const PolygonWithHoles& polygon) {
  return detail::AccessHoles(polygon);
}

inline void SetOuter(PolygonWithHoles& polygon, const Polygon& outer) {
  if constexpr (detail::has_set_outer_function<PolygonWithHoles>::value) {
    boost::polygon::set_outer(polygon, outer);
  } else {
    Outer(polygon) = outer;
  }
}

inline void ClearHoles(PolygonWithHoles& polygon) {
  Holes(polygon).clear();
}

inline void AddHole(PolygonWithHoles& polygon, const Polygon& hole) {
  if constexpr (detail::has_add_hole_function<PolygonWithHoles>::value) {
    boost::polygon::add_hole(polygon, hole);
  } else {
    Holes(polygon).push_back(hole);
  }
}

template <typename Iterator>
inline void SetHoles(PolygonWithHoles& polygon, Iterator begin, Iterator end) {
  auto& container = Holes(polygon);
  container.assign(begin, end);
}

inline Orientation OrientationOfPolygonWithHoles(
    const PolygonWithHoles& polygon) {
  return OrientationOfPolygon(Outer(polygon));
}

}  // namespace deepnest
