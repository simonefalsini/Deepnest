#ifndef DEEPNEST_CONFIG_H
#define DEEPNEST_CONFIG_H

#include <QString>
#include <string>
#include <memory>

namespace deepnest {

/**
 * @brief Configuration class for DeepNest algorithm
 *
 * This class uses the Singleton pattern to provide global access to
 * configuration parameters used throughout the nesting algorithm.
 * Parameters are extracted from deepnest.js and svgnest.js
 */
class DeepNestConfig {
private:
    // Singleton instance
    static std::unique_ptr<DeepNestConfig> instance;

    // Private constructor for singleton
    DeepNestConfig();

public:
    // Delete copy constructor and assignment operator
    DeepNestConfig(const DeepNestConfig&) = delete;
    DeepNestConfig& operator=(const DeepNestConfig&) = delete;

    // Singleton accessor
    static DeepNestConfig& getInstance();

    // Reset to default values
    void resetToDefaults();

    // Load configuration from JSON file
    void loadFromJson(const QString& path);

    // Save configuration to JSON file
    void saveToJson(const QString& path) const;

    // Configuration parameters (from deepnest.js lines 20-33)

    /**
     * @brief Scaling factor for Clipper library operations
     *
     * Clipper works with integers, so floating point coordinates
     * need to be scaled up for precision
     */
    double clipperScale;

    /**
     * @brief Tolerance for curve approximation
     *
     * Controls how closely curves (Bezier, arcs) are approximated
     * by line segments. Lower values = more segments = higher precision
     */
    double curveTolerance;
    double getCurveTolerance(){return curveTolerance;};
    void setCurveTolerance(double Tolerance){curveTolerance = Tolerance;};
    /**
     * @brief Spacing between parts in the nest (in same units as input)
     */
    double spacing;

    /**
     * @brief Number of rotation angles to try for each part
     *
     * For example: 4 means try 0°, 90°, 180°, 270°
     * Higher values may find better solutions but take longer
     */
    int rotations;

    /**
     * @brief Size of the genetic algorithm population
     *
     * Number of different placement sequences maintained
     * in each generation of the genetic algorithm
     */
    int populationSize;

    /**
     * @brief Mutation rate for genetic algorithm (percentage)
     *
     * Probability that an individual will be mutated.
     * Value should be between 0 and 100
     */
    int mutationRate;

    /**
     * @brief Number of threads to use for parallel processing
     */
    int threads;

    /**
     * @brief Type of placement strategy to use
     *
     * Options: "gravity", "boundingbox", "convexhull"
     */
    std::string placementType;

    /**
     * @brief Whether to detect and optimize merged lines
     *
     * When parts share edges, this can reduce cutting time
     */
    bool mergeLines;

    /**
     * @brief Time ratio for algorithm phases
     *
     * Ratio of time spent on different optimization phases
     */
    double timeRatio;

    /**
     * @brief Default scale factor for SVG import
     *
     * Default DPI value (72 = 1 inch = 72 points)
     */
    double scale;

    /**
     * @brief Whether to simplify polygons before nesting
     *
     * Can reduce complexity but may lose detail
     */
    bool simplify;

    // Additional parameters from svgnest.js

    /**
     * @brief Whether to use holes in parts
     */
    bool useHoles;

    /**
     * @brief Whether to explore concave regions for placement
     */
    bool exploreConcave;

    // Additional runtime parameters

    /**
     * @brief Maximum number of iterations to run
     *
     * 0 = run indefinitely until stopped
     */
    int maxIterations;

    /**
     * @brief Timeout in seconds for the nesting algorithm
     *
     * 0 = no timeout
     */
    int timeoutSeconds;

    /**
     * @brief Whether to use progressive nesting
     *
     * If true, nests parts incrementally; if false, nests all at once
     */
    bool progressive;

    /**
     * @brief Random seed for reproducible results
     *
     * 0 = use system time for random seed
     */
    unsigned int randomSeed;

    // Getter methods with validation

    /**
     * @brief Get clipper scale with validation
     */
    double getClipperScale() const { return clipperScale; }

    /**
     * @brief Set clipper scale with validation
     */
    void setClipperScale(double value);

    /**
     * @brief Get spacing with validation
     */
    double getSpacing() const { return spacing; }

    /**
     * @brief Set spacing with validation (must be >= 0)
     */
    void setSpacing(double value);

    /**
     * @brief Get rotations with validation
     */
    int getRotations() const { return rotations; }

    /**
     * @brief Set rotations with validation (must be > 0)
     */
    void setRotations(int value);

    /**
     * @brief Get population size with validation
     */
    int getPopulationSize() const { return populationSize; }

    /**
     * @brief Set population size with validation (must be > 2)
     */
    void setPopulationSize(int value);

    /**
     * @brief Get mutation rate with validation
     */
    int getMutationRate() const { return mutationRate; }

    /**
     * @brief Set mutation rate with validation (must be 0-100)
     */
    void setMutationRate(int value);

    /**
     * @brief Get threads count with validation
     */
    int getThreads() const { return threads; }

    /**
     * @brief Set threads count with validation (must be > 0)
     */
    void setThreads(int value);
};

} // namespace deepnest

#endif // DEEPNEST_CONFIG_H
