#include "../../include/deepnest/algorithm/Population.h"
#include <algorithm>
#include <stdexcept>

namespace deepnest {

Population::Population(const DeepNestConfig& config)
    : config_(config)
    , rng_(std::random_device{}()) {
}

Population::Population(const DeepNestConfig& config, unsigned int seed)
    : config_(config)
    , rng_(seed) {
}

void Population::initialize(const std::vector<Polygon*>& parts) {
    individuals_.clear();

    if (parts.empty()) {
        throw std::invalid_argument("Parts list cannot be empty");
    }

    // Create first individual "adam" with parts in given order and random rotations
    // JavaScript: this.population = [{placement: adam, rotation: angles}];
    Individual adam(parts, config_, rng_());
    individuals_.push_back(adam);

    // Fill rest of population with mutated versions of adam
    // JavaScript: while(this.population.length < config.populationSize)
    while (individuals_.size() < static_cast<size_t>(config_.populationSize)) {
        Individual mutant = adam.clone();
        mutant.mutate(config_.mutationRate, config_.rotations, rng_());
        individuals_.push_back(mutant);
    }
}

std::pair<Individual, Individual> Population::crossover(
    const Individual& parent1,
    const Individual& parent2) {

    if (parent1.placement.empty() || parent2.placement.empty()) {
        throw std::invalid_argument("Parents cannot have empty placement sequences");
    }

    // Single point crossover
    // JavaScript: var cutpoint = Math.round(Math.min(Math.max(Math.random(), 0.1), 0.9)*(male.placement.length-1));
    std::uniform_real_distribution<double> dist(0.1, 0.9);
    double randomVal = dist(rng_);
    size_t cutpoint = static_cast<size_t>(std::round(randomVal * (parent1.placement.size() - 1)));

    // Create children
    Individual child1, child2;

    // Child 1: Take first part from parent1 up to cutpoint
    // JavaScript: var gene1 = male.placement.slice(0,cutpoint);
    child1.placement.insert(child1.placement.end(),
                           parent1.placement.begin(),
                           parent1.placement.begin() + cutpoint);
    child1.rotation.insert(child1.rotation.end(),
                          parent1.rotation.begin(),
                          parent1.rotation.begin() + cutpoint);

    // Child 2: Take first part from parent2 up to cutpoint
    child2.placement.insert(child2.placement.end(),
                           parent2.placement.begin(),
                           parent2.placement.begin() + cutpoint);
    child2.rotation.insert(child2.rotation.end(),
                          parent2.rotation.begin(),
                          parent2.rotation.begin() + cutpoint);

    // Fill child1 with remaining parts from parent2 (avoiding duplicates)
    // JavaScript: for(i=0; i<female.placement.length; i++)
    //   if(!contains(gene1, female.placement[i].id)) { gene1.push(female.placement[i]); }
    for (size_t i = 0; i < parent2.placement.size(); ++i) {
        if (!containsPolygon(child1.placement, parent2.placement[i]->id)) {
            child1.placement.push_back(parent2.placement[i]);
            child1.rotation.push_back(parent2.rotation[i]);
        }
    }

    // Fill child2 with remaining parts from parent1 (avoiding duplicates)
    for (size_t i = 0; i < parent1.placement.size(); ++i) {
        if (!containsPolygon(child2.placement, parent1.placement[i]->id)) {
            child2.placement.push_back(parent1.placement[i]);
            child2.rotation.push_back(parent1.rotation[i]);
        }
    }

    return std::make_pair(child1, child2);
}

Individual Population::selectWeightedRandom(const Individual* exclude) {
    if (individuals_.empty()) {
        throw std::runtime_error("Cannot select from empty population");
    }

    // Create working copy of population
    std::vector<Individual> pop = individuals_;

    // Remove excluded individual if specified
    // JavaScript: if(exclude && pop.indexOf(exclude) >= 0) { pop.splice(pop.indexOf(exclude),1); }
    if (exclude != nullptr) {
        int excludeIdx = findIndividualIndex(exclude);
        if (excludeIdx >= 0) {
            pop.erase(pop.begin() + excludeIdx);
        }
    }

    if (pop.empty()) {
        throw std::runtime_error("Population empty after exclusion");
    }

    // Weighted random selection (favor lower fitness = better individuals)
    // JavaScript: var weight = 1/pop.length; var upper = weight;
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double rand = dist(rng_);

    double lower = 0.0;
    double weight = 1.0 / pop.size();
    double upper = weight;

    // JavaScript: for(var i=0; i<pop.length; i++)
    //   if(rand > lower && rand < upper) { return pop[i]; }
    //   upper += 2*weight * ((pop.length-i)/pop.length);
    for (size_t i = 0; i < pop.size(); ++i) {
        if (rand > lower && rand < upper) {
            return pop[i];
        }
        lower = upper;
        upper += 2.0 * weight * (static_cast<double>(pop.size() - i) / pop.size());
    }

    // Fallback: return first individual
    return pop[0];
}

void Population::nextGeneration() {
    if (individuals_.empty()) {
        throw std::runtime_error("Cannot create next generation from empty population");
    }

    // Sort by fitness (lower is better)
    // JavaScript: this.population.sort(function(a, b){ return a.fitness - b.fitness; });
    sortByFitness();

    // Create new population starting with best individual (elitism)
    // JavaScript: var newpopulation = [this.population[0]];
    std::vector<Individual> newPopulation;
    newPopulation.push_back(individuals_[0]);

    // Fill rest of population with children from crossover + mutation
    // JavaScript: while(newpopulation.length < this.population.length)
    size_t targetSize = individuals_.size();
    while (newPopulation.size() < targetSize) {
        // Select two parents using weighted random selection
        Individual male = selectWeightedRandom(nullptr);
        Individual female = selectWeightedRandom(&male);

        // Crossover to produce two children
        // JavaScript: var children = this.mate(male, female);
        auto children = crossover(male, female);

        // Mutate and add first child
        // JavaScript: newpopulation.push(this.mutate(children[0]));
        children.first.mutate(config_.mutationRate, config_.rotations, rng_());
        newPopulation.push_back(children.first);

        // Mutate and add second child if there's room
        // JavaScript: if(newpopulation.length < this.population.length)
        if (newPopulation.size() < targetSize) {
            children.second.mutate(config_.mutationRate, config_.rotations, rng_());
            newPopulation.push_back(children.second);
        }
    }

    // Replace old population with new one
    individuals_ = newPopulation;
}

size_t Population::size() const {
    return individuals_.size();
}

bool Population::empty() const {
    return individuals_.empty();
}

Individual& Population::operator[](size_t index) {
    return individuals_.at(index);
}

const Individual& Population::operator[](size_t index) const {
    return individuals_.at(index);
}

std::vector<Individual>& Population::getIndividuals() {
    return individuals_;
}

const std::vector<Individual>& Population::getIndividuals() const {
    return individuals_;
}

const Individual& Population::getBest() const {
    if (individuals_.empty()) {
        throw std::runtime_error("Cannot get best from empty population");
    }

    // Assume population is sorted by fitness
    return individuals_[0];
}

void Population::sortByFitness() {
    std::sort(individuals_.begin(), individuals_.end(),
              [](const Individual& a, const Individual& b) {
                  return a.fitness < b.fitness;
              });
}

bool Population::isGenerationComplete() const {
    // Check if all individuals have been evaluated (have valid fitness)
    // JavaScript: for(i=0; i<GA.population.length; i++) { if(!GA.population[i].fitness) return false; }
    for (const auto& individual : individuals_) {
        if (!individual.hasValidFitness()) {
            return false;
        }
    }
    return true;
}

size_t Population::getProcessingCount() const {
    // Count individuals currently being processed
    // JavaScript: var running = GA.population.filter(function(p){ return !!p.processing; }).length;
    size_t count = 0;
    for (const auto& individual : individuals_) {
        if (individual.processing) {
            ++count;
        }
    }
    return count;
}

void Population::clear() {
    individuals_.clear();
}

bool Population::containsPolygon(const std::vector<Polygon*>& placement, int polygonId) {
    // JavaScript: function contains(gene, id) { for(var i=0; i<gene.length; i++)
    //   if(gene[i].id == id) { return true; } }
    for (const auto* poly : placement) {
        if (poly != nullptr && poly->id == polygonId) {
            return true;
        }
    }
    return false;
}

int Population::findIndividualIndex(const Individual* individual) const {
    if (individual == nullptr) {
        return -1;
    }

    for (size_t i = 0; i < individuals_.size(); ++i) {
        if (&individuals_[i] == individual) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace deepnest
