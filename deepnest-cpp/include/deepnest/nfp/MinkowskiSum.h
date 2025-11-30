#ifndef DEEPNEST_MINKOWSKISUM_H
#define DEEPNEST_MINKOWSKISUM_H

#include "../core/Polygon.h"
#include <vector>

namespace deepnest {
    namespace scale {
        /**
         * @brief Minkowski Sum calculation for NFP
         *
         * This class provides Minkowski sum calculation using Boost.Polygon's
         * convolve functions. The Minkowski sum is used to compute the No-Fit
         * Polygon (NFP) between two shapes.
         *
         * Based on minkowski.cc from the original DeepNest
         */
        class MinkowskiSum {
        public:
            /**
             * @brief Calculate No-Fit Polygon using Minkowski sum with dynamic scaling
             *
             * Computes the NFP of polygon B sliding around polygon A.
             * Uses dynamic scaling to prevent overflow and maximize precision.
             *
             * @param A First polygon (stationary)
             * @param B Second polygon (moving)
             * @return Vector of NFP polygons
             */
            static std::vector<Polygon> calculateNFP(
                const Polygon& A,
                const Polygon& B
            );

            /**
             * @brief Calculate NFPs for multiple B polygons against one A
             *
             * Batch version for efficiency when computing NFPs of multiple
             * polygons against the same stationary polygon.
             *
             * @param A Stationary polygon
             * @param Blist List of moving polygons
             * @param inner If true, polygons can slide inside; if false, outside
             * @return Vector of NFP results (one per B polygon)
             */
            static std::vector<std::vector<Polygon>> calculateNFPBatch(
                const Polygon& A,
                const std::vector<Polygon>& Blist);

        private:

            /**
             * @brief Convert polygon to Boost integer polygon with scaling
             *
             * Converts double coordinates to integer using the provided scale factor.
             * Dynamic scaling prevents overflow and maximizes precision.
             *
             * @param poly Input polygon
             * @param scale Scaling factor for integer conversion
             * @return Boost polygon with integer coordinates
             */
            static boost::polygon::polygon_with_holes_data<int> toBoostIntPolygon(
                const Polygon& poly,
                double scale
            );

            /**
             * @brief Convert Boost integer polygon back to our Polygon with scaling
             *
             * Converts integer coordinates back to doubles by dividing by scale.
             *
             * @param boostPoly Boost polygon with integer coordinates
             * @param scale Scaling factor to reverse
             * @return Our Polygon with floating point coordinates
             */
            static Polygon fromBoostIntPolygon(
                const boost::polygon::polygon_with_holes_data<int>& boostPoly,
                double scale
            );

            /**
             * @brief Extract polygons from Boost polygon set with scaling
             *
             * @param polySet Boost polygon set
             * @param scale Scaling factor to reverse
             * @return Vector of our Polygons
             */
            static std::vector<Polygon> fromBoostPolygonSet(
                const boost::polygon::polygon_set_data<int>& polySet,
                double scale
            );
        };
    }


    namespace trunk {

        /**
            * @brief Minkowski Sum calculation for NFP
            *
            * This class provides Minkowski sum calculation using Boost.Polygon's
            * convolve functions. The Minkowski sum is used to compute the No-Fit
            * Polygon (NFP) between two shapes.
            *
            * Based on minkowski.cc from the original DeepNest
            */
        class MinkowskiSum {
        public:
            /**
                * @brief Calculate No-Fit Polygon using Minkowski sum
                *
                * Computes the NFP of polygon B sliding around polygon A.
                * The result is a polygon (or set of polygons) that represents
                * all valid positions where B's reference point can be placed
                * such that B touches but does not overlap with A.
                *
                * @param A First polygon (stationary)
                * @param B Second polygon (moving)
                * @param inner If true, B can slide inside A; if false, outside
                * @return Vector of NFP polygons
                */
            static std::vector<Polygon> calculateNFP(
                const Polygon& A,
                const Polygon& B
            );

            /**
                * @brief Calculate NFPs for multiple B polygons against one A
                *
                * Batch version for efficiency when computing NFPs of multiple
                * polygons against the same stationary polygon.
                *
                * @param A Stationary polygon
                * @param Blist List of moving polygons
                * @param inner If true, polygons can slide inside; if false, outside
                * @return Vector of NFP results (one per B polygon)
                */
            static std::vector<std::vector<Polygon>> calculateNFPBatch(
                const Polygon& A,
                const std::vector<Polygon>& Blist);

        private:

            /**
                * @brief Convert polygon to Boost integer polygon
                *
                * Converts double coordinates to integer by direct truncation.
                * Assumes input polygons are in reasonable coordinate ranges.
                *
                * @param poly Input polygon
                * @return Boost polygon with integer coordinates
                */
            static boost::polygon::polygon_with_holes_data<int> toBoostIntPolygon(
                const Polygon& poly
            );

            /**
                * @brief Convert Boost integer polygon back to our Polygon
                *
                * Converts integer coordinates back to doubles.
                *
                * @param boostPoly Boost polygon with integer coordinates
                * @return Our Polygon with floating point coordinates
                */
            static Polygon fromBoostIntPolygon(
                const boost::polygon::polygon_with_holes_data<int>& boostPoly
            );

            /**
                * @brief Extract polygons from Boost polygon set
                *
                * @param polySet Boost polygon set
                * @return Vector of our Polygons
                */
            static std::vector<Polygon> fromBoostPolygonSet(
                const boost::polygon::polygon_set_data<int>& polySet
            );
        };
    };

    namespace newmnk {

        std::vector<Polygon> calculateNFP(
            const Polygon& A,
            const Polygon& B);

    };
} // namespace deepnest

#endif // DEEPNEST_MINKOWSKISUM_H
