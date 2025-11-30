#include "../../include/deepnest/core/Polygon.h"
#include <boost/polygon/polygon.hpp>
#include <vector>
#include <limits>
#include <cmath>
#include <algorithm>

namespace deepnest {

namespace newmnk  {

    // Local typedefs for integer-based Boost.Polygon operations
    // mirroring the logic in node-calculateNFP/src/minkowski.cc
    typedef boost::polygon::point_data<int> point;
    typedef boost::polygon::polygon_set_data<int> polygon_set;
    typedef boost::polygon::polygon_with_holes_data<int> polygon;
    typedef std::pair<point, point> edge;
    using namespace boost::polygon::operators;

    void convolve_two_segments(std::vector<point>& figure, const edge& a, const edge& b) {
        using namespace boost::polygon;
        figure.clear();
        figure.push_back(point(a.first));
        figure.push_back(point(a.first));
        figure.push_back(point(a.second));
        figure.push_back(point(a.second));
        convolve(figure[0], b.second);
        convolve(figure[1], b.first);
        convolve(figure[2], b.first);
        convolve(figure[3], b.second);
    }

    template <typename itrT1, typename itrT2>
    void convolve_two_point_sequences(polygon_set& result, itrT1 ab, itrT1 ae, itrT2 bb, itrT2 be) {
        using namespace boost::polygon;
        if (ab == ae || bb == be)
            return;
        point first_a = *ab;
        point prev_a = *ab;
        std::vector<point> vec;
        polygon poly;
        ++ab;
        for (; ab != ae; ++ab) {
            point first_b = *bb;
            point prev_b = *bb;
            itrT2 tmpb = bb;
            ++tmpb;
            for (; tmpb != be; ++tmpb) {
                convolve_two_segments(vec, std::make_pair(prev_b, *tmpb), std::make_pair(prev_a, *ab));
                set_points(poly, vec.begin(), vec.end());
                result.insert(poly);
                prev_b = *tmpb;
            }
            prev_a = *ab;
        }
    }

    template <typename itrT>
    void convolve_point_sequence_with_polygons(polygon_set& result, itrT b, itrT e, const std::vector<polygon>& polygons) {
        using namespace boost::polygon;
        for (std::size_t i = 0; i < polygons.size(); ++i) {
            convolve_two_point_sequences(result, b, e, begin_points(polygons[i]), end_points(polygons[i]));
            for (polygon_with_holes_traits<polygon>::iterator_holes_type itrh = begin_holes(polygons[i]);
                 itrh != end_holes(polygons[i]); ++itrh) {
                convolve_two_point_sequences(result, b, e, begin_points(*itrh), end_points(*itrh));
            }
        }
    }

    void convolve_two_polygon_sets(polygon_set& result, const polygon_set& a, const polygon_set& b) {
        using namespace boost::polygon;
        result.clear();
        std::vector<polygon> a_polygons;
        std::vector<polygon> b_polygons;
        a.get(a_polygons);
        b.get(b_polygons);
        for (std::size_t ai = 0; ai < a_polygons.size(); ++ai) {
            convolve_point_sequence_with_polygons(result, begin_points(a_polygons[ai]),
                                                  end_points(a_polygons[ai]), b_polygons);
            for (polygon_with_holes_traits<polygon>::iterator_holes_type itrh = begin_holes(a_polygons[ai]);
                 itrh != end_holes(a_polygons[ai]); ++itrh) {
                convolve_point_sequence_with_polygons(result, begin_points(*itrh),
                                                      end_points(*itrh), b_polygons);
            }
            for (std::size_t bi = 0; bi < b_polygons.size(); ++bi) {
                polygon tmp_poly = a_polygons[ai];
                result.insert(convolve(tmp_poly, *(begin_points(b_polygons[bi]))));
                tmp_poly = b_polygons[bi];
                result.insert(convolve(tmp_poly, *(begin_points(a_polygons[ai]))));
            }
        }
    }



    // Static method implementation
    // Note: This function is intended to be part of a class or a standalone utility.
    // Based on the user request, it seems they want a standalone function or static method.
    // The user asked for: static std::vector<Polygon> calculateNFP(const Polygon& A, const Polygon& B);
    // I will wrap it in a class or namespace if needed, but for now I'll provide the function body.
    // Since the user asked for "converti la funzione calculateNFP per essere chiamata dal c++ con questo prototipo...",
    // I will assume it might be added to a class or just a free function.
    // However, the filename is `calculatenfp.cpp`. I'll put it in `deepnest` namespace.

    std::vector<Polygon> calculateNFP(const Polygon& A, const Polygon& B) {
        polygon_set a, b, c;
        std::vector<polygon> polys;
        std::vector<point> pts;

        // Calculate input scale based on the geometry bounds
        double Amaxx = 0, Aminx = 0, Amaxy = 0, Aminy = 0;
        double Bmaxx = 0, Bminx = 0, Bmaxy = 0, Bminy = 0;

        // Bounds for A
        if (!A.points.empty()) {
            Amaxx = Aminx = A.points[0].x;
            Amaxy = Aminy = A.points[0].y;
            for (const auto& p : A.points) {
                Amaxx = std::max(Amaxx, p.x);
                Aminx = std::min(Aminx, p.x);
                Amaxy = std::max(Amaxy, p.y);
                Aminy = std::min(Aminy, p.y);
            }
        }

        // Bounds for B
        if (!B.points.empty()) {
            Bmaxx = Bminx = B.points[0].x;
            Bmaxy = Bminy = B.points[0].y;
            for (const auto& p : B.points) {
                Bmaxx = std::max(Bmaxx, p.x);
                Bminx = std::min(Bminx, p.x);
                Bmaxy = std::max(Bmaxy, p.y);
                Bminy = std::min(Bminy, p.y);
            }
        }

        double Cmaxx = Amaxx + Bmaxx;
        double Cminx = Aminx + Bminx;
        double Cmaxy = Amaxy + Bmaxy;
        double Cminy = Aminy + Bminy;

        double maxxAbs = std::max(Cmaxx, std::abs(Cminx));
        double maxyAbs = std::max(Cmaxy, std::abs(Cminy));

        double maxda = std::max(maxxAbs, maxyAbs);
        int maxi = std::numeric_limits<int>::max();

        if (maxda < 1) {
            maxda = 1;
        }

        // Calculate scale factor to maximize integer range usage
        double inputscale = (0.1f * (double)(maxi)) / maxda;

        // Store first point of B for shift reference
        double xshift = 0;
        double yshift = 0;
        if (!B.points.empty()) {
            xshift = B.points[0].x;
            yshift = B.points[0].y;
        }

        // Process polygon A
        pts.clear();
        for (const auto& p : A.points) {
            int x = (int)(inputscale * p.x);
            int y = (int)(inputscale * p.y);
            pts.push_back(point(x, y));
        }

        polygon poly;
        boost::polygon::set_points(poly, pts.begin(), pts.end());
        a += poly;

        // Process holes in A
        for (const auto& hole : A.children) {
            pts.clear();
            for (const auto& p : hole.points) {
                int x = (int)(inputscale * p.x);
                int y = (int)(inputscale * p.y);
                pts.push_back(point(x, y));
            }
            boost::polygon::set_points(poly, pts.begin(), pts.end());
            a -= poly;
        }

        // Process polygon B (negated for NFP)
        pts.clear();
        for (const auto& p : B.points) {
            int x = -(int)(inputscale * p.x);
            int y = -(int)(inputscale * p.y);
            pts.push_back(point(x, y));
        }

        boost::polygon::set_points(poly, pts.begin(), pts.end());
        b += poly;

        // Process holes in B
        for (const auto& hole : B.children) {
            pts.clear();
            for (const auto& p : hole.points) {
                int x = -(int)(inputscale * p.x);
                int y = -(int)(inputscale * p.y);
                pts.push_back(point(x, y));
            }
            boost::polygon::set_points(poly, pts.begin(), pts.end());
            b -= poly;
        }

        // Calculate NFP
        convolve_two_polygon_sets(c, a, b);
        polys.clear();
        c.get(polys);

        std::vector<Polygon> result;
        result.reserve(polys.size());

        for (const auto& p : polys) {
            Polygon resPoly;
        
            // Convert outer boundary
            for (auto itr = p.begin(); itr != p.end(); ++itr) {
                double x = ((double)(itr->get(boost::polygon::HORIZONTAL))) / inputscale + xshift;
                double y = ((double)(itr->get(boost::polygon::VERTICAL))) / inputscale + yshift;
                resPoly.points.push_back(Point(x, y));
            }

            // Convert holes
            for (auto itrh = begin_holes(p); itrh != end_holes(p); ++itrh) {
                Polygon holePoly;
                for (auto itr2 = (*itrh).begin(); itr2 != (*itrh).end(); ++itr2) {
                    double x = ((double)(itr2->get(boost::polygon::HORIZONTAL))) / inputscale + xshift;
                    double y = ((double)(itr2->get(boost::polygon::VERTICAL))) / inputscale + yshift;
                    holePoly.points.push_back(Point(x, y));
                }
                if (!holePoly.points.empty()) {
                    resPoly.children.push_back(holePoly);
                }
            }

            if (!resPoly.points.empty()) {
                result.push_back(resPoly);
            }
        }

        return result;
    }
} // newmnk
} // namespace deepnest
