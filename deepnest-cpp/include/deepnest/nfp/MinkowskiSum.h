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
         * Now uses int64_t coordinates throughout - coordinates are already scaled
         * at I/O boundaries by inputScale, so no additional scaling is needed.
         *
         * Based on minkowski.cc from the original DeepNest
         */
        class MinkowskiSum {
        public:
            /**
             * @brief Calculate No-Fit Polygon using Minkowski sum
             *
             * Computes the NFP of polygon B sliding around polygon A.
             * Point coordinates are already int64_t (scaled by inputScale at I/O boundaries),
             * so no additional scaling is performed.
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
             * @return Vector of NFP results (one per B polygon)
             */
            static std::vector<std::vector<Polygon>> calculateNFPBatch(
                const Polygon& A,
                const std::vector<Polygon>& Blist);

        private:

            /**
             * @brief Convert polygon to Boost integer polygon
             *
             * Direct conversion from Point (int64_t) to BoostPoint (int64_t).
             * No scaling is performed as coordinates are already properly scaled.
             *
             * @param poly Input polygon
             * @param scale DEPRECATED - ignored, kept for backward compatibility
             * @return Boost polygon with int64_t coordinates
             */
            static BoostPolygonWithHoles toBoostIntPolygon(
                const Polygon& poly,
                double scale
            );

            /**
             * @brief Convert Boost integer polygon back to our Polygon
             *
             * Direct conversion from BoostPoint (int64_t) to Point (int64_t).
             * No descaling is performed.
             *
             * @param boostPoly Boost polygon with int64_t coordinates
             * @param scale DEPRECATED - ignored, kept for backward compatibility
             * @return Our Polygon with int64_t coordinates
             */
            static Polygon fromBoostIntPolygon(
                const BoostPolygonWithHoles& boostPoly,
                double scale
            );

            /**
             * @brief Extract polygons from Boost polygon set
             *
             * @param polySet Boost polygon set with int64_t coordinates
             * @param scale DEPRECATED - ignored, kept for backward compatibility
             * @return Vector of our Polygons
             */
            static std::vector<Polygon> fromBoostPolygonSet(
                const BoostPolygonSet& polySet,
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
            * Now uses int64_t coordinates throughout - coordinates are already scaled
            * at I/O boundaries by inputScale, so no additional scaling is needed.
            *
            * Based on minkowski.cc from the original DeepNest
            */
        class MinkowskiSum {
        public:
            /**
                * @brief Calculate No-Fit Polygon using Minkowski sum
                *
                * Computes the NFP of polygon B sliding around polygon A.
                * Point coordinates are already int64_t (scaled by inputScale at I/O boundaries),
                * so no additional scaling is performed.
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
                * @return Vector of NFP results (one per B polygon)
                */
            static std::vector<std::vector<Polygon>> calculateNFPBatch(
                const Polygon& A,
                const std::vector<Polygon>& Blist);

        private:

            /**
                * @brief Convert polygon to Boost integer polygon
                *
                * Direct conversion from Point (int64_t) to BoostPoint (int64_t).
                * No scaling is performed as coordinates are already properly scaled.
                *
                * @param poly Input polygon
                * @return Boost polygon with int64_t coordinates
                */
            static BoostPolygonWithHoles toBoostIntPolygon(
                const Polygon& poly
            );

            /**
                * @brief Convert Boost integer polygon back to our Polygon
                *
                * Direct conversion from BoostPoint (int64_t) to Point (int64_t).
                * No descaling is performed.
                *
                * @param boostPoly Boost polygon with int64_t coordinates
                * @return Our Polygon with int64_t coordinates
                */
            static Polygon fromBoostIntPolygon(
                const BoostPolygonWithHoles& boostPoly
            );

            /**
                * @brief Extract polygons from Boost polygon set
                *
                * @param polySet Boost polygon set with int64_t coordinates
                * @return Vector of our Polygons
                */
            static std::vector<Polygon> fromBoostPolygonSet(
                const BoostPolygonSet& polySet
            );
        };
    };


} // namespace deepnest

#endif // DEEPNEST_MINKOWSKISUM_H
