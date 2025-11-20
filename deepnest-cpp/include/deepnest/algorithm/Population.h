#ifndef DEEPNEST_POPULATION_H
#define DEEPNEST_POPULATION_H

#include "Individual.h"
#include "../config/DeepNestConfig.h"
#include <vector>
#include <random>
#include <algorithm>

namespace deepnest {

/**
 * @brief Population manager for genetic algorithm
 *
 * Manages a population of individuals and provides genetic operations
 * such as crossover, selection, and generation evolution.
 *
 * References:
 * - deepnest.js: GeneticAlgorithm methods (lines 1330-1463)
 */
class Population {
private:
    /**
     * @brief Current population of individuals
     */
    std::vector<Individual> individuals_;

    /**
     * @brief Configuration for genetic algorithm
     */
    const DeepNestConfig& config_;

    /**
     * @brief Random number generator
     */
    std::mt19937 rng_;

public:
    /**
     * @brief Constructor with config
     */
    explicit Population(const DeepNestConfig& config);

    /**
     * @brief Constructor with config and seed for random number generator
     */
    Population(const DeepNestConfig& config, unsigned int seed);

    /**
     * @brief Initialize population from parts list
     *
     * Creates initial population:
     * 1. First individual ("adam") has parts in given order with random rotations
     * 2. Remaining individuals are mutations of adam
     *
     * PHASE 2: Now accepts shared_ptr instead of raw pointers for thread safety.
     *
     * @param parts List of polygon shared pointers to be nested
     *
     * Corresponds to JavaScript GeneticAlgorithm constructor (line 1331-1345)
     */
    void initialize(const std::vector<std::shared_ptr<Polygon>>& parts);

    /**
     * @brief Perform single-point crossover between two parents
     *
     * Creates two children by:
     * 1. Selecting a random cut point (10-90% of sequence length)
     * 2. Child1 gets parent1[0:cut] + remaining from parent2 (no duplicates)
     * 3. Child2 gets parent2[0:cut] + remaining from parent1 (no duplicates)
     *
     * @param parent1 First parent individual
     * @param parent2 Second parent individual
     * @return Pair of two children individuals
     *
     * Corresponds to JavaScript mate() function (line 1374-1409)
     */
    std::pair<Individual, Individual> crossover(
        const Individual& parent1,
        const Individual& parent2
    );

    /**
     * @brief Select individual using weighted random selection
     *
     * Selection is weighted to favor individuals with better (lower) fitness.
     * The population should be sorted by fitness before calling this method.
     *
     * @param exclude Optional individual to exclude from selection
     * @return Selected individual
     *
     * Corresponds to JavaScript randomWeightedIndividual() (line 1440-1463)
     */
    Individual selectWeightedRandom(const Individual* exclude = nullptr);

    /**
     * @brief Create next generation using genetic operations
     *
     * Process:
     * 1. Sort population by fitness (elitism)
     * 2. Keep best individual (elitism)
     * 3. Fill rest of population with children from crossover + mutation
     *
     * Corresponds to JavaScript generation() function (line 1411-1437)
     */
    void nextGeneration();

    /**
     * @brief Get current population size
     */
    size_t size() const;

    /**
     * @brief Check if population is empty
     */
    bool empty() const;

    /**
     * @brief Get individual at index
     */
    Individual& operator[](size_t index);
    const Individual& operator[](size_t index) const;

    /**
     * @brief Get all individuals
     */
    std::vector<Individual>& getIndividuals();
    const std::vector<Individual>& getIndividuals() const;

    /**
     * @brief Get best individual (lowest fitness)
     *
     * Population should be sorted first
     */
    const Individual& getBest() const;

    /**
     * @brief Sort population by fitness (ascending)
     */
    void sortByFitness();

    /**
     * @brief Check if all individuals have been evaluated
     */
    bool isGenerationComplete() const;

    /**
     * @brief Get number of individuals currently being processed
     */
    size_t getProcessingCount() const;

    /**
     * @brief Clear population
     */
    void clear();

private:
    /**
     * @brief Check if a placement sequence contains a polygon by ID
     */
    static bool containsPolygon(const std::vector<Polygon*>& placement, int polygonId);

    /**
     * @brief Find index of individual in population (by pointer comparison)
     */
    int findIndividualIndex(const Individual* individual) const;
};

} // namespace deepnest

#endif // DEEPNEST_POPULATION_H
