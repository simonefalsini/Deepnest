#include "../../include/deepnest/algorithm/Individual.h"
#include <algorithm>
#include <cmath>

namespace deepnest {

Individual::Individual()
    : fitness(std::numeric_limits<double>::max())
    , area(0.0)
    , mergedLength(0.0)
    , processing(false) {
}

Individual::Individual(const std::vector<std::shared_ptr<Polygon>>& parts,
                       const DeepNestConfig& config,
                       unsigned int seed)
    : placement(parts)  // PHASE 2: Now copies shared_ptr, not raw pointers
    , fitness(std::numeric_limits<double>::max())
    , area(0.0)
    , mergedLength(0.0)
    , processing(false) {

    // Initialize random number generator
    std::mt19937 rng(seed);

    // Generate random rotation for each part
    // JavaScript: angle = Math.floor(Math.random()*this.config.rotations)*(360/this.config.rotations);
    rotation.reserve(parts.size());
    for (size_t i = 0; i < parts.size(); ++i) {
        rotation.push_back(generateRandomRotation(config.rotations, rng));
    }
}

Individual Individual::clone() const {
    Individual copy;
    copy.placement = this->placement;  // Copy pointers
    copy.rotation = this->rotation;    // Deep copy of rotations
    copy.fitness = this->fitness;
    copy.area = this->area;
    copy.mergedLength = this->mergedLength;
    copy.placements = this->placements; // Copy placement results
    copy.processing = false;           // Reset processing flag

    return copy;
}

void Individual::mutate(double mutationRate, int numRotations, unsigned int seed) {
    // Initialize random number generator
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    // Convert mutation rate from percentage (0-100) to probability (0.0-1.0)
    // DeepNestConfig passes int percentage, we need to convert to probability
    double mutationProb = mutationRate * 0.01;

    // GA DEBUG: Track mutations
    int swapCount = 0;
    int rotationChangeCount = 0;

    // Mutate placement order (swap adjacent parts)
    // JavaScript: if(rand < 0.01*this.config.mutationRate) { swap }
    for (size_t i = 0; i < placement.size(); ++i) {
        double rand = dist(rng);
        if (rand < mutationProb) {
            // Swap current part with next part
            size_t j = i + 1;

            if (j < placement.size()) {
                std::swap(placement[i], placement[j]);
                swapCount++;
            }
        }
    }

    // Mutate rotations
    // JavaScript: if(rand < 0.01*this.config.mutationRate) { new rotation }
    for (size_t i = 0; i < rotation.size(); ++i) {
        double rand = dist(rng);
        if (rand < mutationProb) {
            rotation[i] = generateRandomRotation(numRotations, rng);
            rotationChangeCount++;
        }
    }

    // GA DEBUG: Log mutations for every call during initialization and first generation
    static int mutationCount = 0;
    mutationCount++;

    // Log first 15 mutations to see initial diversity, then every 100th
    if (mutationCount <= 15 || mutationCount % 100 == 0) {
        std::cout << "  Mutation #" << mutationCount << ": swaps=" << swapCount
                  << ", rotation_changes=" << rotationChangeCount
                  << ", prob=" << mutationProb << std::endl;
        std::cout.flush();
    }

    // Reset fitness since individual has changed
    resetFitness();
}

bool Individual::hasValidFitness() const {
    return fitness < std::numeric_limits<double>::max();
}

void Individual::resetFitness() {
    fitness = std::numeric_limits<double>::max();
    area = 0.0;
    mergedLength = 0.0;
    placements.clear();  // Clear placement results when fitness is reset
}

size_t Individual::size() const {
    return placement.size();
}

bool Individual::operator<(const Individual& other) const {
    return fitness < other.fitness;
}

double Individual::generateRandomRotation(int numRotations, std::mt19937& rng) {
    // JavaScript: Math.floor(Math.random()*this.config.rotations)*(360/this.config.rotations);
    std::uniform_int_distribution<int> dist(0, numRotations - 1);
    int step = dist(rng);
    double angle = step * (360.0 / numRotations);

    return angle;
}

} // namespace deepnest
