#ifndef DEEPNEST_INDIVIDUAL_H
#define DEEPNEST_INDIVIDUAL_H

#include "../core/Polygon.h"
#include "../config/DeepNestConfig.h"
#include "../placement/PlacementWorker.h"
#include <vector>
#include <limits>
#include <random>

namespace deepnest {

/**
 * @brief Represents an individual solution in the genetic algorithm
 *
 * An individual encodes a potential nesting solution by specifying:
 * - The order in which parts are placed (placement sequence)
 * - The rotation angle for each part
 * - A fitness value representing the quality of the solution (lower is better)
 *
 * References:
 * - deepnest.js: GeneticAlgorithm population structure (line 1340)
 * - deepnest.js: mutate() function (line 1349-1371)
 */
class Individual {
public:
    /**
     * @brief Placement sequence - order of part insertion
     *
     * Each element is a pointer to a Polygon that will be placed.
     * The order in this array determines the placement sequence.
     */
    std::vector<Polygon*> placement;

    /**
     * @brief Rotation angles for each part (in degrees)
     *
     * rotation[i] is the rotation angle for placement[i].
     * Angles are typically multiples of (360 / config.rotations).
     */
    std::vector<double> rotation;

    /**
     * @brief Fitness value (lower is better)
     *
     * Represents the quality of this solution. Lower fitness values
     * indicate better solutions (e.g., less wasted material, smaller bounding box).
     * Initialized to max() to indicate uncomputed fitness.
     */
    double fitness;

    /**
     * @brief Total sheet area used
     *
     * Sum of areas of all sheets used in this placement.
     * Populated when individual is evaluated.
     */
    double area;

    /**
     * @brief Total length of merged/aligned lines
     *
     * Used for cutting optimization - represents lines that can be merged
     * for more efficient cutting paths. Populated when individual is evaluated.
     */
    double mergedLength;

    /**
     * @brief Placements organized by sheet
     *
     * Contains the actual placement results - position and rotation of each part
     * on each sheet. Populated when individual is evaluated.
     */
    std::vector<std::vector<PlacementWorker::Placement>> placements;

    /**
     * @brief Processing flag
     *
     * True when this individual is currently being evaluated by a worker thread.
     * Used to prevent duplicate evaluations.
     */
    bool processing;

    /**
     * @brief Default constructor
     *
     * Creates an individual with maximum fitness (uncomputed) and not processing.
     */
    Individual();

    /**
     * @brief Construct individual from parts and random rotations
     *
     * @param parts List of polygons to be nested
     * @param config Configuration containing rotation settings
     * @param seed Random seed for rotation generation (optional)
     *
     * Initializes placement with the given parts in order, and assigns
     * random rotation angles based on config.rotations.
     */
    Individual(const std::vector<Polygon*>& parts,
               const DeepNestConfig& config,
               unsigned int seed = std::random_device{}());

    /**
     * @brief Clone this individual
     *
     * Creates a deep copy of this individual with the same placement order,
     * rotations, and fitness value. The processing flag is reset to false.
     *
     * @return A new Individual that is a copy of this one
     *
     * Corresponds to JavaScript clone in mutate() (line 1350)
     */
    Individual clone() const;

    /**
     * @brief Mutate this individual
     *
     * Applies genetic mutations to this individual:
     * 1. With probability mutationRate%, swap adjacent parts in placement sequence
     * 2. With probability mutationRate%, assign a new random rotation
     *
     * @param mutationRate Mutation rate as percentage (0-100)
     * @param numRotations Number of allowed rotations (e.g., 4 for 90° increments)
     * @param seed Random seed (optional)
     *
     * Corresponds to JavaScript mutate() (line 1349-1371)
     */
    void mutate(double mutationRate, int numRotations, unsigned int seed = std::random_device{}());

    /**
     * @brief Check if this individual has been evaluated
     *
     * @return True if fitness has been computed (fitness < max), false otherwise
     */
    bool hasValidFitness() const;

    /**
     * @brief Reset fitness to uncomputed state
     *
     * Sets fitness to maximum value, indicating it needs to be recomputed.
     */
    void resetFitness();

    /**
     * @brief Get the number of parts in this individual
     *
     * @return Number of parts (placement.size())
     */
    size_t size() const;

    /**
     * @brief Comparison operator for sorting by fitness
     *
     * @param other Individual to compare with
     * @return True if this individual has lower (better) fitness
     */
    bool operator<(const Individual& other) const;

private:
    /**
     * @brief Generate random rotation angle
     *
     * @param numRotations Number of allowed rotations
     * @param rng Random number generator
     * @return Rotation angle in degrees
     */
    static double generateRandomRotation(int numRotations, std::mt19937& rng);
};

} // namespace deepnest

#endif // DEEPNEST_INDIVIDUAL_H
