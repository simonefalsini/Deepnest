#pragma once

#include "deepnest/GeometryTypes.h"

#include <clipper2/clipper.h>

#include <vector>

namespace deepnest {

namespace clipper2 = Clipper2Lib;

// Default scale factor that mirrors the JavaScript implementation.
constexpr double kClipperScale = 10'000'000.0;

// Conversion helpers -------------------------------------------------------

clipper2::PathD LoopToClipper(const Loop& loop);
Loop ClipperToLoop(const clipper2::PathD& path);

clipper2::PathsD PolygonToClipper(const PolygonWithHoles& polygon);
clipper2::PathsD PolygonCollectionToClipper(const PolygonCollection& polygons);

PolygonWithHoles ClipperToPolygon(const clipper2::PathsD& paths);
PolygonCollection ClipperToPolygonCollection(const clipper2::PathsD& paths);

clipper2::Paths64 ScaleUpPaths(const clipper2::PathsD& paths,
                               double scale = kClipperScale);
clipper2::PathsD ScaleDownPaths(const clipper2::Paths64& paths,
                                double scale = kClipperScale);

clipper2::PathsD NormalizeClipperOrientation(const clipper2::PathsD& paths,
                                             bool outer_ccw = true);

// Geometric utilities ------------------------------------------------------

clipper2::PathsD CleanPaths(const clipper2::PathsD& paths,
                            Coordinate tolerance);
PolygonCollection CleanPolygons(const PolygonCollection& polygons,
                                Coordinate tolerance);

PolygonCollection OffsetPolygon(
    const PolygonWithHoles& polygon, Coordinate delta,
    clipper2::JoinType join_type = clipper2::JoinType::Round,
    clipper2::EndType end_type = clipper2::EndType::Polygon,
    double miter_limit = 2.0, Coordinate arc_tolerance = 0.0);

PolygonCollection BooleanDifference(const PolygonCollection& subject,
                                    const PolygonCollection& clips,
                                    clipper2::FillRule fill_rule =
                                        clipper2::FillRule::NonZero);

PolygonCollection BooleanDifference(const PolygonWithHoles& subject,
                                    const PolygonCollection& clips,
                                    clipper2::FillRule fill_rule =
                                        clipper2::FillRule::NonZero);

}  // namespace deepnest
