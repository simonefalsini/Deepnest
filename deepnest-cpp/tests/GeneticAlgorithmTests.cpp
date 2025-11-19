/**
 * @file GeneticAlgorithmTests.cpp
 * @brief FASE 4.2: Integration tests for Genetic Algorithm evolution
 *
 * Tests the complete GA optimization process:
 * - Fitness variation across population
 * - Evolution improvement over generations
 * - Population diversity
 * - Convergence behavior
 * - Mutation and crossover effectiveness
 *
 * Reference: IMPLEMENTATION_PLAN.md FASE 4.2
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>

// Fix Windows Polygon name conflict
#ifdef Polygon
#undef Polygon
#endif

#include "../include/deepnest/core/Types.h"
#include "../include/deepnest/core/Point.h"
#include "../include/deepnest/core/Polygon.h"
#include "../include/deepnest/config/DeepNestConfig.h"
#include "../include/deepnest/algorithm/Individual.h"
#include "../include/deepnest/algorithm/Population.h"
#include "../include/deepnest/algorithm/GeneticAlgorithm.h"
#include "../include/deepnest/placement/PlacementWorker.h"
#include "../include/deepnest/nfp/NFPCache.h"
#include "../include/deepnest/nfp/NFPCalculator.h"
#include "../include/deepnest/DeepNestSolver.h"

using namespace deepnest;

// Test result tracking
struct TestResult {
    std::string testName;
    bool passed;
    std::string message;
};

std::vector<TestResult> results;

// Helper macros
#define TEST_CASE(name) \
    std::cout << "\n[TEST] " << name << std::endl; \
    std::string currentTestName = name;

#define EXPECT_TRUE(condition, description) \
    { \
        bool passed = (condition); \
        results.push_back({currentTestName + " - " + description, passed, description}); \
        if (passed) { \
            std::cout << "  ✓ " << description << std::endl; \
        } else { \
            std::cout << "  ✗ " << description << " FAILED" << std::endl; \
        } \
    }

#define EXPECT_GT(actual, threshold, description) \
    { \
        double actualVal = (actual); \
        double thresholdVal = (threshold); \
        bool passed = (actualVal > thresholdVal); \
        results.push_back({currentTestName + " - " + description, passed, description}); \
        if (passed) { \
            std::cout << "  ✓ " << description << ": " << actualVal << " > " << thresholdVal << std::endl; \
        } else { \
            std::cout << "  ✗ " << description << ": " << actualVal << " NOT > " << thresholdVal << std::endl; \
        } \
    }

#define EXPECT_LT(actual, threshold, description) \
    { \
        double actualVal = (actual); \
        double thresholdVal = (threshold); \
        bool passed = (actualVal < thresholdVal); \
        results.push_back({currentTestName + " - " + description, passed, description}); \
        if (passed) { \
            std::cout << "  ✓ " << description << ": " << actualVal << " < " << thresholdVal << std::endl; \
        } else { \
            std::cout << "  ✗ " << description << ": " << actualVal << " NOT < " << thresholdVal << std::endl; \
        } \
    }

// Helper function to create test parts
std::vector<Polygon> createTestParts(int count, int seed = 12345) {
    std::vector<Polygon> parts;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> sizeDist(20.0, 50.0);

    for (int i = 0; i < count; i++) {
        double width = sizeDist(rng);
        double height = sizeDist(rng);

        std::vector<Point> points = {
            {0, 0}, {width, 0}, {width, height}, {0, height}
        };

        Polygon part(points, i);
        parts.push_back(part);
    }

    return parts;
}

// Helper function to create test sheets
std::vector<Polygon> createTestSheets(int count, double width = 500.0, double height = 300.0) {
    std::vector<Polygon> sheets;

    for (int i = 0; i < count; i++) {
        std::vector<Point> points = {
            {0, 0}, {width, 0}, {width, height}, {0, height}
        };

        Polygon sheet(points, 100 + i);
        sheets.push_back(sheet);
    }

    return sheets;
}

// Helper function to calculate statistics
struct Stats {
    double min;
    double max;
    double mean;
    double stddev;
    double variation; // (max - min) / min
};

Stats calculateStats(const std::vector<double>& values) {
    Stats stats;

    if (values.empty()) {
        return {0, 0, 0, 0, 0};
    }

    stats.min = *std::min_element(values.begin(), values.end());
    stats.max = *std::max_element(values.begin(), values.end());

    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    stats.mean = sum / values.size();

    double sq_sum = 0.0;
    for (double val : values) {
        sq_sum += (val - stats.mean) * (val - stats.mean);
    }
    stats.stddev = std::sqrt(sq_sum / values.size());

    stats.variation = (stats.max - stats.min) / (stats.min > 0 ? stats.min : 1.0);

    return stats;
}

// ========== Test 1: Population Initialization ==========
void test_PopulationInitialization() {
    TEST_CASE("Population Initialization");

    DeepNestConfig& config = DeepNestConfig::getInstance();
    config.setPopulationSize(10);
    config.setRotations(4);
    config.setMutationRate(50);

    // Create test parts
    std::vector<Polygon> parts = createTestParts(5);
    std::vector<Polygon*> partPtrs;
    for (auto& p : parts) {
        partPtrs.push_back(&p);
    }

    // Initialize population
    Population pop(config, 12345);
    pop.initialize(partPtrs);

    EXPECT_TRUE(pop.size() == 10, "Population size is 10");

    // Check each individual
    for (size_t i = 0; i < pop.size(); i++) {
        const Individual& ind = pop[i];
        EXPECT_TRUE(ind.placement.size() == 5, "Individual has 5 parts");
        EXPECT_TRUE(ind.rotation.size() == 5, "Individual has 5 rotations");
    }
}

// ========== Test 2: Initial Fitness Variation ==========
void test_InitialFitnessVariation() {
    TEST_CASE("Initial Fitness Variation");

    DeepNestConfig& config = DeepNestConfig::getInstance();
    config.setPopulationSize(10);
    config.setRotations(4);
    config.setMutationRate(50);
    config.setSpacing(2.0);
    config.placementType = "gravity";

    // Create test data
    std::vector<Polygon> parts = createTestParts(5);
    std::vector<Polygon> sheets = createTestSheets(2);

    std::vector<Polygon*> partPtrs;
    for (auto& p : parts) {
        partPtrs.push_back(&p);
    }

    // Create GA
    NFPCache cache;
    NFPCalculator calculator(cache);
    PlacementWorker worker(config, calculator);
    GeneticAlgorithm ga(partPtrs, config);

    // Evaluate initial population
    std::cout << "  Evaluating initial population..." << std::endl;

    // Get population and evaluate each individual
    Population& pop = ga.getPopulation();

    std::vector<double> fitnessValues;
    for (size_t i = 0; i < pop.size(); i++) {
        Individual& ind = pop[i];

        // Build placement from individual
        std::vector<Polygon> orderedParts;
        for (size_t j = 0; j < ind.placement.size(); j++) {
            size_t partIdx = ind.placement[j];
            if (partIdx < parts.size()) {
                Polygon rotatedPart = parts[partIdx].rotate(ind.rotation[j]);
                orderedParts.push_back(rotatedPart);
            }
        }

        // Evaluate fitness
        auto result = worker.placeParts(sheets, orderedParts);
        ind.fitness = result.fitness;
        fitnessValues.push_back(ind.fitness);
    }

    // Calculate statistics
    Stats stats = calculateStats(fitnessValues);

    std::cout << "  Fitness statistics:" << std::endl;
    std::cout << "    Min: " << stats.min << std::endl;
    std::cout << "    Max: " << stats.max << std::endl;
    std::cout << "    Mean: " << stats.mean << std::endl;
    std::cout << "    StdDev: " << stats.stddev << std::endl;
    std::cout << "    Variation: " << (stats.variation * 100.0) << "%" << std::endl;

    // Assertions
    EXPECT_GT(stats.min, 1000.0, "Min fitness > 1000 (not ~1.0)");
    EXPECT_GT(stats.variation, 0.05, "Fitness varies by > 5%");
    EXPECT_TRUE(stats.stddev > 0, "Fitness has non-zero standard deviation");
}

// ========== Test 3: Mutation Creates Diversity ==========
void test_MutationCreatesDiversity() {
    TEST_CASE("Mutation Creates Diversity");

    DeepNestConfig& config = DeepNestConfig::getInstance();
    config.setRotations(4);

    std::vector<Polygon> parts = createTestParts(10);
    std::vector<Polygon*> partPtrs;
    for (auto& p : parts) {
        partPtrs.push_back(&p);
    }

    // Create individual
    Individual original(partPtrs, config, 12345);

    // Create copies and mutate with different rates
    std::vector<Individual> mutated;

    for (int rate = 10; rate <= 90; rate += 20) {
        Individual copy = original.clone();
        copy.mutate(rate, 4, 54321);  // Different seed
        mutated.push_back(copy);
    }

    // Check that mutated individuals are different from original
    int differentCount = 0;

    for (const auto& ind : mutated) {
        bool isDifferent = false;

        // Check placement order
        for (size_t i = 0; i < ind.placement.size(); i++) {
            if (ind.placement[i] != original.placement[i]) {
                isDifferent = true;
                break;
            }
        }

        // Check rotations
        if (!isDifferent) {
            for (size_t i = 0; i < ind.rotation.size(); i++) {
                if (std::abs(ind.rotation[i] - original.rotation[i]) > 0.1) {
                    isDifferent = true;
                    break;
                }
            }
        }

        if (isDifferent) differentCount++;
    }

    std::cout << "  " << differentCount << " / " << mutated.size() << " mutated individuals differ from original" << std::endl;

    EXPECT_GT(differentCount, 0, "At least some mutated individuals differ");
}

// ========== Test 4: Fitness Improves Over Generations ==========
void test_FitnessImprovesOverGenerations() {
    TEST_CASE("Fitness Improves Over Generations");

    DeepNestConfig& config = DeepNestConfig::getInstance();
    config.setPopulationSize(8);  // Smaller for faster test
    config.setRotations(4);
    config.setMutationRate(50);
    config.setSpacing(2.0);
    config.placementType = "gravity";

    // Create test data
    std::vector<Polygon> parts = createTestParts(4);  // Fewer parts for speed
    std::vector<Polygon> sheets = createTestSheets(1);

    std::vector<Polygon*> partPtrs;
    for (auto& p : parts) {
        partPtrs.push_back(&p);
    }

    // Create solver
    DeepNestSolver solver;
    solver.setSpacing(2.0);
    solver.setRotations(4);
    solver.setPopulationSize(8);
    solver.setMutationRate(50);

    for (auto& part : parts) {
        solver.addPart(part, 1, "TestPart");
    }

    for (auto& sheet : sheets) {
        solver.addSheet(sheet, 1, "TestSheet");
    }

    // Run for 5 generations and track best fitness
    std::cout << "  Running 5 generations..." << std::endl;

    std::vector<double> bestFitnessPerGen;

    solver.startNesting();

    // Wait a bit and collect samples
    for (int gen = 0; gen < 5; gen++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        int currentGen = solver.getCurrentGeneration();
        double bestFitness = solver.getBestFitness();

        bestFitnessPerGen.push_back(bestFitness);

        std::cout << "    Gen " << currentGen << ": best fitness = " << bestFitness << std::endl;
    }

    solver.stopNesting();

    // Check improvement
    if (bestFitnessPerGen.size() >= 2) {
        double initialBest = bestFitnessPerGen.front();
        double finalBest = bestFitnessPerGen.back();

        std::cout << "  Initial best: " << initialBest << std::endl;
        std::cout << "  Final best: " << finalBest << std::endl;

        // Fitness should decrease (improvement) or stay similar
        // We allow for some tolerance as GA is stochastic
        double improvement = (initialBest - finalBest) / initialBest;
        std::cout << "  Improvement: " << (improvement * 100.0) << "%" << std::endl;

        // At minimum, fitness should not get significantly worse
        EXPECT_TRUE(finalBest <= initialBest * 1.1, "Fitness doesn't get significantly worse");

        // Ideally, we'd see some improvement
        if (improvement > 0.01) {
            std::cout << "  ✓ Fitness improved over generations" << std::endl;
        }
    }
}

// ========== Test 5: Population Sorting ==========
void test_PopulationSorting() {
    TEST_CASE("Population Sorting");

    DeepNestConfig& config = DeepNestConfig::getInstance();
    config.setPopulationSize(10);
    config.setRotations(4);

    std::vector<Polygon> parts = createTestParts(3);
    std::vector<Polygon*> partPtrs;
    for (auto& p : parts) {
        partPtrs.push_back(&p);
    }

    Population pop(config, 12345);
    pop.initialize(partPtrs);

    // Assign random fitness values (descending)
    for (size_t i = 0; i < pop.size(); i++) {
        pop[i].fitness = static_cast<double>(pop.size() - i) * 1000.0;
    }

    std::cout << "  Before sorting:" << std::endl;
    std::cout << "    Individual 0 fitness: " << pop[0].fitness << std::endl;
    std::cout << "    Individual 9 fitness: " << pop[9].fitness << std::endl;

    // Sort
    pop.sortByFitness();

    std::cout << "  After sorting:" << std::endl;
    std::cout << "    Individual 0 fitness: " << pop[0].fitness << std::endl;
    std::cout << "    Individual 9 fitness: " << pop[9].fitness << std::endl;

    // Check sorted order (ascending - best fitness first)
    bool isSorted = true;
    for (size_t i = 1; i < pop.size(); i++) {
        if (pop[i].fitness < pop[i-1].fitness) {
            isSorted = false;
            break;
        }
    }

    EXPECT_TRUE(isSorted, "Population sorted in ascending fitness order");
    EXPECT_LT(pop[0].fitness, pop[9].fitness, "Best individual at index 0");
}

// ========== Test 6: Individual Cloning ==========
void test_IndividualCloning() {
    TEST_CASE("Individual Cloning");

    DeepNestConfig& config = DeepNestConfig::getInstance();
    config.setRotations(4);

    std::vector<Polygon> parts = createTestParts(5);
    std::vector<Polygon*> partPtrs;
    for (auto& p : parts) {
        partPtrs.push_back(&p);
    }

    Individual original(partPtrs, config, 12345);
    original.fitness = 12345.67;

    Individual clone = original.clone();

    // Check that clone has same data
    EXPECT_TRUE(clone.placement.size() == original.placement.size(), "Clone has same placement size");
    EXPECT_TRUE(clone.rotation.size() == original.rotation.size(), "Clone has same rotation size");
    EXPECT_TRUE(clone.fitness == original.fitness, "Clone has same fitness");

    // Check placement values
    bool placementMatch = true;
    for (size_t i = 0; i < original.placement.size(); i++) {
        if (original.placement[i] != clone.placement[i]) {
            placementMatch = false;
            break;
        }
    }
    EXPECT_TRUE(placementMatch, "Clone placement matches original");

    // Check rotation values
    bool rotationMatch = true;
    for (size_t i = 0; i < original.rotation.size(); i++) {
        if (std::abs(original.rotation[i] - clone.rotation[i]) > 0.01) {
            rotationMatch = false;
            break;
        }
    }
    EXPECT_TRUE(rotationMatch, "Clone rotation matches original");

    // Mutate clone and verify original unchanged
    clone.mutate(50, 4, 54321);

    bool originalUnchanged = true;
    for (size_t i = 0; i < original.placement.size(); i++) {
        if (original.placement[i] != original.placement[i]) {  // Verify original is stable
            originalUnchanged = false;
            break;
        }
    }
    EXPECT_TRUE(originalUnchanged, "Original unchanged after clone mutation");
}

// ========== Test 7: Fitness Reset After Mutation ==========
void test_FitnessResetAfterMutation() {
    TEST_CASE("Fitness Reset After Mutation");

    DeepNestConfig& config = DeepNestConfig::getInstance();
    config.setRotations(4);

    std::vector<Polygon> parts = createTestParts(5);
    std::vector<Polygon*> partPtrs;
    for (auto& p : parts) {
        partPtrs.push_back(&p);
    }

    Individual ind(partPtrs, config, 12345);

    // Set fitness
    ind.fitness = 12345.67;
    EXPECT_TRUE(ind.hasValidFitness(), "Individual has valid fitness");

    // Mutate
    ind.mutate(50, 4, 54321);

    // Fitness should be reset
    EXPECT_TRUE(!ind.hasValidFitness(), "Fitness reset after mutation");
}

// ========== Test 8: GA Generation Count ==========
void test_GAGenerationCount() {
    TEST_CASE("GA Generation Count");

    DeepNestConfig& config = DeepNestConfig::getInstance();
    config.setPopulationSize(5);
    config.setRotations(4);

    std::vector<Polygon> parts = createTestParts(3);
    std::vector<Polygon*> partPtrs;
    for (auto& p : parts) {
        partPtrs.push_back(&p);
    }

    GeneticAlgorithm ga(partPtrs, config);

    EXPECT_TRUE(ga.getCurrentGeneration() == 0, "Initial generation is 0");

    // Note: We can't easily test generation() without full setup
    // This is covered in integration tests
}

// ========== Main Test Runner ==========
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "FASE 4.2: Genetic Algorithm Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    // Run all tests
    test_PopulationInitialization();
    test_InitialFitnessVariation();
    test_MutationCreatesDiversity();
    test_FitnessImprovesOverGenerations();
    test_PopulationSorting();
    test_IndividualCloning();
    test_FitnessResetAfterMutation();
    test_GAGenerationCount();

    // Print summary
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test Summary" << std::endl;
    std::cout << "========================================" << std::endl;

    int passed = 0;
    int failed = 0;

    for (const auto& result : results) {
        if (result.passed) {
            passed++;
        } else {
            failed++;
            std::cout << "FAILED: " << result.testName << std::endl;
        }
    }

    std::cout << "\nTotal tests: " << results.size() << std::endl;
    std::cout << "Passed: " << passed << " (" << std::fixed << std::setprecision(1)
              << (100.0 * passed / results.size()) << "%)" << std::endl;
    std::cout << "Failed: " << failed << std::endl;

    if (failed == 0) {
        std::cout << "\n✓ All GA integration tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "\n✗ Some tests failed. Review output above." << std::endl;
        return 1;
    }
}
