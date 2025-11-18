#include "../../include/deepnest/algorithm/GeneticAlgorithm.h"
#include <stdexcept>
#include <algorithm>

namespace deepnest {

GeneticAlgorithm::GeneticAlgorithm(const std::vector<Polygon*>& adam, const DeepNestConfig& config)
    : population_(config)
    , config_(config)
    , parts_(adam)
    , currentGeneration_(0) {

    if (adam.empty()) {
        throw std::invalid_argument("Parts list (adam) cannot be empty");
    }

    // Initialize population with adam as first individual
    // JavaScript: this.population = [{placement: adam, rotation: angles}];
    //             while(this.population.length < config.populationSize) {
    //                 var mutant = this.mutate(this.population[0]);
    //                 this.population.push(mutant);
    //             }
    population_.initialize(adam);
}

void GeneticAlgorithm::generation() {
    if (!isGenerationComplete()) {
        throw std::runtime_error("Cannot create next generation: current generation not complete");
    }

    // Create next generation using Population's nextGeneration method
    // JavaScript: GeneticAlgorithm.prototype.generation = function() {
    //   this.population.sort(function(a, b){ return a.fitness - b.fitness; });
    //   var newpopulation = [this.population[0]]; // elitism
    //   while(newpopulation.length < this.population.length) {
    //     var male = this.randomWeightedIndividual();
    //     var female = this.randomWeightedIndividual(male);
    //     var children = this.mate(male, female);
    //     newpopulation.push(this.mutate(children[0]));
    //     if(newpopulation.length < this.population.length) {
    //       newpopulation.push(this.mutate(children[1]));
    //     }
    //   }
    //   this.population = newpopulation;
    // }
    population_.nextGeneration();
    currentGeneration_++;
}

const Individual& GeneticAlgorithm::getBestIndividual() const {
    if (population_.empty()) {
        throw std::runtime_error("Cannot get best individual from empty population");
    }

    // Population's getBest() assumes population is sorted by fitness
    // We need to sort first if not already sorted
    // For safety, let's find the minimum manually
    const auto& individuals = population_.getIndividuals();

    auto minIt = std::min_element(individuals.begin(), individuals.end(),
                                  [](const Individual& a, const Individual& b) {
                                      return a.fitness < b.fitness;
                                  });

    if (minIt == individuals.end()) {
        throw std::runtime_error("No individuals in population");
    }

    return *minIt;
}

bool GeneticAlgorithm::isGenerationComplete() const {
    // Check if all individuals have been evaluated
    // JavaScript: var finished = true;
    //   for(i=0; i<GA.population.length; i++) {
    //     if(!GA.population[i].fitness) {
    //       finished = false;
    //       break;
    //     }
    //   }
    return population_.isGenerationComplete();
}

int GeneticAlgorithm::getCurrentGeneration() const {
    return currentGeneration_;
}

size_t GeneticAlgorithm::getPopulationSize() const {
    return population_.size();
}

Individual& GeneticAlgorithm::getIndividual(size_t index) {
    return population_[index];
}

const Individual& GeneticAlgorithm::getIndividual(size_t index) const {
    return population_[index];
}

std::vector<Individual>& GeneticAlgorithm::getPopulation() {
    return population_.getIndividuals();
}

const std::vector<Individual>& GeneticAlgorithm::getPopulation() const {
    return population_.getIndividuals();
}

size_t GeneticAlgorithm::getProcessingCount() const {
    // Count individuals currently being processed
    // JavaScript: var running = GA.population.filter(function(p){ return !!p.processing; }).length;
    return population_.getProcessingCount();
}

const DeepNestConfig& GeneticAlgorithm::getConfig() const {
    return config_;
}

const std::vector<Polygon*>& GeneticAlgorithm::getParts() const {
    return parts_;
}

void GeneticAlgorithm::reset() {
    population_.clear();
    currentGeneration_ = 0;
}

void GeneticAlgorithm::reinitialize(const std::vector<Polygon*>& adam) {
    if (adam.empty()) {
        throw std::invalid_argument("Parts list (adam) cannot be empty");
    }

    reset();
    parts_ = adam;
    population_.initialize(adam);
}

std::tuple<int, size_t, size_t, bool> GeneticAlgorithm::getStatistics() const {
    int generation = currentGeneration_;
    size_t popSize = population_.size();
    size_t processing = population_.getProcessingCount();
    bool complete = population_.isGenerationComplete();

    return std::make_tuple(generation, popSize, processing, complete);
}

} // namespace deepnest
