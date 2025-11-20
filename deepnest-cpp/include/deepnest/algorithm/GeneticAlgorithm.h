#ifndef DEEPNEST_GENETIC_ALGORITHM_H
#define DEEPNEST_GENETIC_ALGORITHM_H

#include "Population.h"
#include "Individual.h"
#include "../config/DeepNestConfig.h"
#include "../core/Polygon.h"
#include <vector>
#include <memory>

namespace deepnest {

/**
 * @brief Main genetic algorithm class for nesting optimization
 *
 * This class manages the genetic algorithm for optimizing part placement.
 * It wraps the Population class and provides a high-level interface for
 * running the optimization process.
 *
 * The genetic algorithm works by:
 * 1. Creating initial population with random solutions
 * 2. Evaluating fitness of each individual (done externally)
 * 3. Evolving population using selection, crossover, and mutation
 * 4. Repeating until convergence or max generations
 *
 * References:
 * - deepnest.js: GeneticAlgorithm class (lines 1329-1463)
 */
class GeneticAlgorithm {
private:
    /**
     * @brief Population manager
     */
    Population population_;

    /**
     * @brief Algorithm configuration
     */
    const DeepNestConfig& config_;

    /**
     * @brief Parts to be nested (stored as shared_ptr for thread safety)
     *
     * PHASE 2 FIX: Changed from raw pointers to shared_ptr.
     * Shared ownership ensures parts stay alive even if source vector is cleared.
     */
    std::vector<std::shared_ptr<Polygon>> parts_;

    /**
     * @brief Generation counter
     */
    int currentGeneration_;

public:
    /**
     * @brief Constructor
     *
     * Initializes the genetic algorithm with a list of parts (adam) and configuration.
     * Creates initial population with adam as first individual and fills rest with mutations.
     *
     * PHASE 2: Now accepts shared_ptr instead of raw pointers for thread safety.
     *
     * @param adam List of parts to be nested (order preserved in first individual)
     * @param config Algorithm configuration
     *
     * Corresponds to JavaScript GeneticAlgorithm constructor (line 1329-1346)
     */
    GeneticAlgorithm(const std::vector<std::shared_ptr<Polygon>>& adam, const DeepNestConfig& config);

    /**
     * @brief Create next generation
     *
     * Evolves the population to the next generation using:
     * 1. Sort by fitness (elitism)
     * 2. Keep best individual
     * 3. Fill rest with crossover + mutation
     *
     * Should be called only when current generation is complete (all evaluated).
     *
     * Corresponds to JavaScript generation() method (line 1411-1437)
     */
    void generation();

    /**
     * @brief Get best individual from current population
     *
     * Returns the individual with lowest (best) fitness value.
     * Population should be evaluated before calling this.
     *
     * @return Best individual (by reference)
     */
    const Individual& getBestIndividual() const;

    /**
     * @brief Check if current generation is complete
     *
     * Returns true if all individuals in population have been evaluated
     * (have valid fitness values and are not processing).
     *
     * @return True if generation is complete, false otherwise
     *
     * Corresponds to JavaScript check: for(i=0; i<GA.population.length; i++)
     *   if(!GA.population[i].fitness) { finished = false; }
     */
    bool isGenerationComplete() const;

    /**
     * @brief Get current generation number
     */
    int getCurrentGeneration() const;

    /**
     * @brief Get population size
     */
    size_t getPopulationSize() const;

    /**
     * @brief Get individual at index
     *
     * @param index Index in population
     * @return Individual reference
     */
    Individual& getIndividual(size_t index);
    const Individual& getIndividual(size_t index) const;

    /**
     * @brief Get all individuals in population
     */
    std::vector<Individual>& getPopulation();
    const std::vector<Individual>& getPopulation() const;

    /**
     * @brief Get Population object
     *
     * Returns reference to the underlying Population object.
     * This is needed for ParallelProcessor::processPopulation.
     */
    Population& getPopulationObject();
    const Population& getPopulationObject() const;

    /**
     * @brief Get number of individuals currently being processed
     */
    size_t getProcessingCount() const;

    /**
     * @brief Get configuration
     */
    const DeepNestConfig& getConfig() const;

    /**
     * @brief Get parts list
     *
     * PHASE 2: Returns shared_ptr vector instead of raw pointers.
     */
    const std::vector<std::shared_ptr<Polygon>>& getParts() const;

    /**
     * @brief Reset algorithm
     *
     * Clears population and resets generation counter.
     * Useful for restarting optimization.
     */
    void reset();

    /**
     * @brief Reinitialize with new parts
     *
     * Resets and creates new population from given parts.
     *
     * PHASE 2: Now accepts shared_ptr instead of raw pointers.
     *
     * @param adam New parts list
     */
    void reinitialize(const std::vector<std::shared_ptr<Polygon>>& adam);

    /**
     * @brief Get statistics about current state
     *
     * @return Tuple of (generation, population_size, processing_count, has_best)
     */
    std::tuple<int, size_t, size_t, bool> getStatistics() const;
};

} // namespace deepnest

#endif // DEEPNEST_GENETIC_ALGORITHM_H
