#include "deepnest/MinkowskiConvolution.h"

#include <boost/polygon/polygon.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace deepnest {

namespace {

using IntCoordinate = std::int32_t;
using IntPoint = boost::polygon::point_data<IntCoordinate>;
using IntPolygon = boost::polygon::polygon_with_holes_data<IntCoordinate>;
using IntPolygonSet = boost::polygon::polygon_set_data<IntCoordinate>;
using IntEdge = std::pair<IntPoint, IntPoint>;

struct Bounds {
  Coordinate min_x {0.0};
  Coordinate max_x {0.0};
  Coordinate min_y {0.0};
  Coordinate max_y {0.0};
  bool initialized {false};
};

void UpdateBounds(const PolygonWithHoles& polygon, Bounds* bounds) {
  if (polygon.outer().size() < 1) {
    return;
  }
  auto update = [bounds](const Point& pt) {
    if (!bounds->initialized) {
      bounds->min_x = bounds->max_x = pt.x();
      bounds->min_y = bounds->max_y = pt.y();
      bounds->initialized = true;
      return;
    }
    bounds->min_x = std::min(bounds->min_x, pt.x());
    bounds->max_x = std::max(bounds->max_x, pt.x());
    bounds->min_y = std::min(bounds->min_y, pt.y());
    bounds->max_y = std::max(bounds->max_y, pt.y());
  };

  for (const auto& pt : polygon.outer()) {
    update(pt);
  }
  for (const auto& hole : polygon.inners()) {
    for (const auto& pt : hole) {
      update(pt);
    }
  }
}

IntPoint ScalePoint(const Point& pt, double scale, bool negate) {
  const double factor = negate ? -scale : scale;
  const Coordinate x = pt.x() * factor;
  const Coordinate y = pt.y() * factor;
  return IntPoint(static_cast<IntCoordinate>(std::llround(x)),
                  static_cast<IntCoordinate>(std::llround(y)));
}

void InsertPolygon(const PolygonWithHoles& polygon, double scale, bool negate,
                   IntPolygonSet* target) {
  if (polygon.outer().size() < 3) {
    return;
  }

  using namespace boost::polygon::operators;

  std::vector<IntPoint> pts;
  pts.reserve(polygon.outer().size());
  for (const auto& pt : polygon.outer()) {
    pts.push_back(ScalePoint(pt, scale, negate));
  }

  IntPolygon poly;
  boost::polygon::set_points(poly, pts.begin(), pts.end());
  (*target) += poly;

  for (const auto& hole : polygon.inners()) {
    if (hole.size() < 3) {
      continue;
    }
    pts.clear();
    pts.reserve(hole.size());
    for (const auto& pt : hole) {
      pts.push_back(ScalePoint(pt, scale, negate));
    }
    boost::polygon::set_points(poly, pts.begin(), pts.end());
    (*target) -= poly;
  }
}

void ConvolveTwoSegments(std::vector<IntPoint>& figure, const IntEdge& a,
                         const IntEdge& b) {
  using namespace boost::polygon;
  figure.clear();
  figure.push_back(IntPoint(a.first));
  figure.push_back(IntPoint(a.first));
  figure.push_back(IntPoint(a.second));
  figure.push_back(IntPoint(a.second));
  convolve(figure[0], b.second);
  convolve(figure[1], b.first);
  convolve(figure[2], b.first);
  convolve(figure[3], b.second);
}

template <typename IteratorA, typename IteratorB>
void ConvolveTwoPointSequences(IntPolygonSet& result, IteratorA ab, IteratorA ae,
                               IteratorB bb, IteratorB be) {
  using namespace boost::polygon;
  if (ab == ae || bb == be) {
    return;
  }
  IntPoint prev_a = *ab;
  std::vector<IntPoint> vec;
  IntPolygon poly;
  ++ab;
  for (; ab != ae; ++ab) {
    IntPoint prev_b = *bb;
    IteratorB tmpb = bb;
    ++tmpb;
    for (; tmpb != be; ++tmpb) {
      ConvolveTwoSegments(vec, std::make_pair(prev_b, *tmpb),
                          std::make_pair(prev_a, *ab));
      boost::polygon::set_points(poly, vec.begin(), vec.end());
      result.insert(poly);
      prev_b = *tmpb;
    }
    prev_a = *ab;
  }
}

template <typename Iterator>
void ConvolvePointSequenceWithPolygons(IntPolygonSet& result, Iterator begin,
                                       Iterator end,
                                       const std::vector<IntPolygon>& polygons) {
  using namespace boost::polygon;
  for (const auto& polygon : polygons) {
    ConvolveTwoPointSequences(result, begin, end, begin_points(polygon),
                              end_points(polygon));
    for (auto hole = begin_holes(polygon); hole != end_holes(polygon); ++hole) {
      ConvolveTwoPointSequences(result, begin, end, begin_points(*hole),
                                end_points(*hole));
    }
  }
}

void ConvolveTwoPolygonSets(IntPolygonSet& result, const IntPolygonSet& a,
                            const IntPolygonSet& b) {
  using namespace boost::polygon;
  result.clear();
  std::vector<IntPolygon> a_polygons;
  std::vector<IntPolygon> b_polygons;
  a.get(a_polygons);
  b.get(b_polygons);
  for (const auto& poly_a : a_polygons) {
    ConvolvePointSequenceWithPolygons(result, begin_points(poly_a),
                                      end_points(poly_a), b_polygons);
    for (auto hole = begin_holes(poly_a); hole != end_holes(poly_a); ++hole) {
      ConvolvePointSequenceWithPolygons(result, begin_points(*hole),
                                        end_points(*hole), b_polygons);
    }
    for (const auto& poly_b : b_polygons) {
      IntPolygon tmp_poly = poly_a;
      result.insert(convolve(tmp_poly, *(begin_points(poly_b))));
      tmp_poly = poly_b;
      result.insert(convolve(tmp_poly, *(begin_points(poly_a))));
    }
  }
}

PolygonWithHoles ConvertToPolygon(const IntPolygon& poly, double scale,
                                  Coordinate xshift, Coordinate yshift) {
  Polygon outer;
  std::vector<Point> outer_points;
  for (auto it = boost::polygon::begin_points(poly);
       it != boost::polygon::end_points(poly); ++it) {
    const Coordinate x = static_cast<Coordinate>(it->get(boost::polygon::HORIZONTAL)) /
                         scale + xshift;
    const Coordinate y = static_cast<Coordinate>(it->get(boost::polygon::VERTICAL)) /
                         scale + yshift;
    outer_points.emplace_back(x, y);
  }
  if (outer_points.size() < 3) {
    return PolygonWithHoles();
  }
  boost::polygon::set_points(outer, outer_points.begin(), outer_points.end());

  std::vector<Polygon> holes;
  for (auto hole = boost::polygon::begin_holes(poly);
       hole != boost::polygon::end_holes(poly); ++hole) {
    std::vector<Point> hole_points;
    for (auto it = boost::polygon::begin_points(*hole);
         it != boost::polygon::end_points(*hole); ++it) {
      const Coordinate x = static_cast<Coordinate>(it->get(boost::polygon::HORIZONTAL)) /
                           scale + xshift;
      const Coordinate y = static_cast<Coordinate>(it->get(boost::polygon::VERTICAL)) /
                           scale + yshift;
      hole_points.emplace_back(x, y);
    }
    if (hole_points.size() < 3) {
      continue;
    }
    Polygon hole_poly;
    boost::polygon::set_points(hole_poly, hole_points.begin(), hole_points.end());
    holes.push_back(hole_poly);
  }

  PolygonWithHoles result;
  boost::polygon::set_outer(result, outer);
  boost::polygon::set_holes(result, holes.begin(), holes.end());
  return result;
}

}  // namespace

PolygonCollection MinkowskiConvolution::ComputeAll(const PolygonWithHoles& a,
                                                   const PolygonWithHoles& b) const {
  Bounds bounds_a;
  Bounds bounds_b;
  UpdateBounds(a, &bounds_a);
  UpdateBounds(b, &bounds_b);

  const Coordinate cmaxx = bounds_a.max_x + bounds_b.max_x;
  const Coordinate cminx = bounds_a.min_x + bounds_b.min_x;
  const Coordinate cmaxy = bounds_a.max_y + bounds_b.max_y;
  const Coordinate cminy = bounds_a.min_y + bounds_b.min_y;

  Coordinate max_abs = std::max(std::max(std::fabs(cmaxx), std::fabs(cminx)),
                                std::max(std::fabs(cmaxy), std::fabs(cminy)));
  if (max_abs < 1.0) {
    max_abs = 1.0;
  }

  const double max_int = 0.1 * static_cast<double>(std::numeric_limits<IntCoordinate>::max());
  const double scale = max_int / max_abs;

  IntPolygonSet a_set;
  IntPolygonSet b_set;
  InsertPolygon(a, scale, /*negate=*/false, &a_set);
  InsertPolygon(b, scale, /*negate=*/true, &b_set);

  IntPolygonSet result_set;
  ConvolveTwoPolygonSets(result_set, a_set, b_set);

  std::vector<IntPolygon> int_polygons;
  result_set.get(int_polygons);

  PolygonCollection polygons;
  polygons.reserve(int_polygons.size());

  Coordinate xshift = 0.0;
  Coordinate yshift = 0.0;
  if (!b.outer().empty()) {
    xshift = b.outer().front().x();
    yshift = b.outer().front().y();
  }

  for (const auto& int_poly : int_polygons) {
    PolygonWithHoles poly = ConvertToPolygon(int_poly, scale, xshift, yshift);
    if (poly.outer().size() < 3) {
      continue;
    }
    polygons.push_back(poly);
  }

  return polygons;
}

PolygonWithHoles MinkowskiConvolution::Compute(const PolygonWithHoles& a,
                                               const PolygonWithHoles& b) const {
  PolygonCollection polygons = ComputeAll(a, b);
  if (polygons.empty()) {
    return PolygonWithHoles();
  }
  return polygons.front();
}

}  // namespace deepnest
