# Piano di Test Completo - DeepNest C++

## Sommario Esecutivo

Questo documento definisce un piano di test completo per verificare che l'implementazione C++ di DeepNest sia:
1. **Corretta** - Produce risultati equivalenti al JavaScript
2. **Completa** - Tutte le funzionalità sono state portate
3. **Robusta** - Gestisce edge cases e input non validi
4. **Performante** - Prestazioni accettabili per uso reale

---

## STRUTTURA TEST SUITE

```
deepnest-cpp/tests/
├── unit/                    # Test unitari per componenti individuali
│   ├── GeometryTests.cpp
│   ├── NFPTests.cpp
│   ├── MinkowskiTests.cpp
│   ├── GeneticAlgorithmTests.cpp
│   ├── PlacementTests.cpp
│   ├── FitnessTests.cpp
│   └── LineMergeTests.cpp
├── integration/             # Test di integrazione tra componenti
│   ├── EndToEndTests.cpp
│   ├── PipelineTests.cpp
│   └── CacheTests.cpp
├── comparison/              # Test di confronto con JavaScript
│   ├── JSComparisonTests.cpp
│   ├── FitnessComparisonTests.cpp
│   └── NFPComparisonTests.cpp
├── regression/              # Test di regressione con casi noti
│   ├── BenchmarkTests.cpp
│   └── RegressionSuite.cpp
├── performance/             # Test di performance
│   ├── BenchmarkPerformance.cpp
│   └── MemoryTests.cpp
├── testdata/               # Dati di test
│   ├── polygons/           # Poligoni di test (JSON/SVG)
│   ├── expected/           # Risultati attesi da JavaScript
│   └── benchmarks/         # Dataset benchmark
└── CMakeLists.txt          # Build configuration per test
```

---

## LIVELLO 1: TEST UNITARI COMPONENTI BASE

### 1.1 Geometry Utilities Tests

**File**: `tests/unit/GeometryTests.cpp`

```cpp
#include <gtest/gtest.h>
#include <deepnest/geometry/GeometryUtil.h>

class GeometryTests : public ::testing::Test {
protected:
    const double TOLERANCE = 1e-9;
};

// Test almostEqual
TEST_F(GeometryTests, AlmostEqual) {
    EXPECT_TRUE(GeometryUtil::almostEqual(1.0, 1.0 + 1e-10));
    EXPECT_FALSE(GeometryUtil::almostEqual(1.0, 1.001));
    EXPECT_TRUE(GeometryUtil::almostEqual(0.0, 0.0));
}

// Test pointInPolygon
TEST_F(GeometryTests, PointInPolygon) {
    Polygon square({{0,0}, {100,0}, {100,100}, {0,100}});

    EXPECT_TRUE(GeometryUtil::pointInPolygon({50, 50}, square));
    EXPECT_FALSE(GeometryUtil::pointInPolygon({150, 50}, square));
    EXPECT_TRUE(GeometryUtil::pointInPolygon({0, 0}, square));  // On vertex
    EXPECT_TRUE(GeometryUtil::pointInPolygon({50, 0}, square)); // On edge
}

// Test polygonArea
TEST_F(GeometryTests, PolygonArea) {
    Polygon square({{0,0}, {100,0}, {100,100}, {0,100}});
    EXPECT_NEAR(std::abs(GeometryUtil::polygonArea(square.points)), 10000.0, 0.1);

    Polygon triangle({{0,0}, {100,0}, {50,50}});
    EXPECT_NEAR(std::abs(GeometryUtil::polygonArea(triangle.points)), 2500.0, 0.1);

    // Test with holes
    Polygon outer({{0,0}, {100,0}, {100,100}, {0,100}});
    Polygon hole({{25,25}, {75,25}, {75,75}, {25,75}});
    outer.children.push_back(hole);
    // Area should be 10000 - 2500 = 7500
    double netArea = std::abs(GeometryUtil::polygonArea(outer.points)) -
                     std::abs(GeometryUtil::polygonArea(hole.points));
    EXPECT_NEAR(netArea, 7500.0, 0.1);
}

// Test getPolygonBounds
TEST_F(GeometryTests, GetPolygonBounds) {
    Polygon poly({{10, 20}, {100, 30}, {80, 90}, {20, 70}});
    BoundingBox bounds = GeometryUtil::getPolygonBounds(poly);

    EXPECT_NEAR(bounds.x, 10.0, 0.1);
    EXPECT_NEAR(bounds.y, 20.0, 0.1);
    EXPECT_NEAR(bounds.width, 90.0, 0.1);   // 100 - 10
    EXPECT_NEAR(bounds.height, 70.0, 0.1);  // 90 - 20
}

// Test lineIntersect
TEST_F(GeometryTests, LineIntersect) {
    Point p1{0, 0}, p2{100, 100};
    Point p3{0, 100}, p4{100, 0};

    auto result = GeometryUtil::lineIntersect(p1, p2, p3, p4);
    EXPECT_TRUE(result.intersect);
    EXPECT_NEAR(result.x, 50.0, 0.1);
    EXPECT_NEAR(result.y, 50.0, 0.1);

    // Parallel lines
    Point p5{0, 0}, p6{100, 0};
    Point p7{0, 10}, p8{100, 10};
    auto result2 = GeometryUtil::lineIntersect(p5, p6, p7, p8);
    EXPECT_FALSE(result2.intersect);
}

// Test isRectangle
TEST_F(GeometryTests, IsRectangle) {
    Polygon square({{0,0}, {100,0}, {100,100}, {0,100}});
    EXPECT_TRUE(GeometryUtil::isRectangle(square));

    Polygon rotatedSquare({{0,0}, {70.7,70.7}, {0,141.4}, {-70.7,70.7}});
    EXPECT_FALSE(GeometryUtil::isRectangle(rotatedSquare));  // Rotated

    Polygon pentagon({{0,0}, {100,0}, {150,50}, {75,100}, {-25,50}});
    EXPECT_FALSE(GeometryUtil::isRectangle(pentagon));
}

// Test noFitPolygonRectangle (optimization)
TEST_F(GeometryTests, NoFitPolygonRectangle) {
    // Container: 500x300 rectangle
    Polygon container({{0,0}, {500,0}, {500,300}, {0,300}});

    // Part: 100x50 rectangle
    Polygon part({{0,0}, {100,0}, {100,50}, {0,50}});

    Polygon nfp = GeometryUtil::noFitPolygonRectangle(container, part);

    // NFP should be (500-100) x (300-50) = 400x250
    BoundingBox bounds = GeometryUtil::getPolygonBounds(nfp);
    EXPECT_NEAR(bounds.width, 400.0, 0.1);
    EXPECT_NEAR(bounds.height, 250.0, 0.1);
}

// Test convex hull
TEST_F(GeometryTests, ConvexHull) {
    // Points forming a square with internal point
    std::vector<Point> points = {
        {0, 0}, {100, 0}, {100, 100}, {0, 100}, {50, 50}
    };

    Polygon hull = ConvexHull::computeHull(points);

    // Hull should have 4 vertices (the square)
    EXPECT_EQ(hull.points.size(), 4);

    // Hull area should equal square area
    double area = std::abs(GeometryUtil::polygonArea(hull.points));
    EXPECT_NEAR(area, 10000.0, 0.1);
}
```

### 1.2 Transformation Tests

**File**: `tests/unit/TransformationTests.cpp`

```cpp
TEST(TransformationTests, Rotation) {
    Polygon square({{0,0}, {100,0}, {100,100}, {0,100}});

    // Rotate 90 degrees around origin
    Polygon rotated = square.rotate(M_PI / 2.0);

    EXPECT_NEAR(rotated.points[0].x, 0.0, 0.1);
    EXPECT_NEAR(rotated.points[0].y, 0.0, 0.1);
    EXPECT_NEAR(rotated.points[1].x, 0.0, 0.1);
    EXPECT_NEAR(rotated.points[1].y, 100.0, 0.1);
}

TEST(TransformationTests, Translation) {
    Polygon square({{0,0}, {100,0}, {100,100}, {0,100}});

    Polygon translated = square.translate(50, 75);

    EXPECT_NEAR(translated.points[0].x, 50.0, 0.1);
    EXPECT_NEAR(translated.points[0].y, 75.0, 0.1);
    EXPECT_NEAR(translated.points[2].x, 150.0, 0.1);
    EXPECT_NEAR(translated.points[2].y, 175.0, 0.1);
}
```

---

## LIVELLO 2: TEST MINKOWSKI E NFP

### 2.1 Minkowski Sum Tests

**File**: `tests/unit/MinkowskiTests.cpp`

```cpp
#include <gtest/gtest.h>
#include <deepnest/nfp/MinkowskiSum.h>

class MinkowskiTests : public ::testing::Test {
protected:
    void SetUp() override {
        // Load test data
        squareA = Polygon({{0,0}, {100,0}, {100,100}, {0,100}});
        squareB = Polygon({{0,0}, {50,0}, {50,50}, {0,50}});
    }

    Polygon squareA;
    Polygon squareB;
};

TEST_F(MinkowskiTests, SimpleSquares) {
    Polygon nfp = MinkowskiSum::calculateNFP(squareA, squareB);

    // NFP of two squares should be a larger square
    // Size should be (100+50) x (100+50) = 150x150
    BoundingBox bounds = GeometryUtil::getPolygonBounds(nfp);

    // Actually, NFP size is (A.width - B.width) x (A.height - B.height)
    // For outer NFP: (100-50) x (100-50) = 50x50
    EXPECT_NEAR(bounds.width, 50.0, 1.0);
    EXPECT_NEAR(bounds.height, 50.0, 1.0);
}

TEST_F(MinkowskiTests, WithHoles) {
    // Create polygon A with a hole
    Polygon A({{0,0}, {100,0}, {100,100}, {0,100}});
    Polygon holeA({{25,25}, {75,25}, {75,75}, {25,75}});
    A.children.push_back(holeA);

    Polygon B({{0,0}, {20,0}, {20,20}, {0,20}});

    Polygon nfp = MinkowskiSum::calculateNFP(A, B);

    // NFP should handle holes correctly
    EXPECT_FALSE(nfp.points.empty());
}

TEST_F(MinkowskiTests, ConcavePolygons) {
    // L-shaped polygon
    Polygon L({{0,0}, {100,0}, {100,50}, {50,50}, {50,100}, {0,100}});
    Polygon small({{0,0}, {10,0}, {10,10}, {0,10}});

    Polygon nfp = MinkowskiSum::calculateNFP(L, small);

    EXPECT_FALSE(nfp.points.empty());
    EXPECT_GT(nfp.points.size(), 4);  // Should be more complex than rectangle
}

TEST_F(MinkowskiTests, ReferencePointShift) {
    // Test that NFP is referenced correctly to B's first point
    Polygon A({{0,0}, {100,0}, {100,100}, {0,100}});
    Polygon B({{10,10}, {60,10}, {60,60}, {10,60}});  // First point at (10,10)

    Polygon nfp = MinkowskiSum::calculateNFP(A, B);

    // Check that NFP coordinates are shifted by B's first point
    // (This test verifies fix 3.1 from IMPLEMENTATION_PLAN)
    bool hasExpectedShift = false;
    for (const auto& p : nfp.points) {
        if (std::abs(p.x - 10.0) < 1.0 || std::abs(p.y - 10.0) < 1.0) {
            hasExpectedShift = true;
            break;
        }
    }
    EXPECT_TRUE(hasExpectedShift);
}

TEST_F(MinkowskiTests, BatchProcessing) {
    std::vector<Polygon> Alist = {squareA, squareA, squareA};
    std::vector<Polygon> nfps = MinkowskiSum::calculateNFPBatch(Alist, squareB);

    EXPECT_EQ(nfps.size(), 3);
    for (const auto& nfp : nfps) {
        EXPECT_FALSE(nfp.points.empty());
    }
}
```

### 2.2 NFP Calculator Tests

**File**: `tests/unit/NFPTests.cpp`

```cpp
class NFPTests : public ::testing::Test {
protected:
    NFPCache cache;
    NFPCalculator calculator;
};

TEST_F(NFPTests, OuterNFP) {
    Polygon A({{0,0}, {100,0}, {100,100}, {0,100}});
    Polygon B({{0,0}, {50,0}, {50,50}, {0,50}});

    Polygon nfp = calculator.getOuterNFP(A, B, false);

    EXPECT_FALSE(nfp.points.empty());

    // Verify caching works
    EXPECT_TRUE(cache.has(A.id, B.id, 0.0, 0.0));
    Polygon cachedNFP = cache.find(A.id, B.id, 0.0, 0.0);
    EXPECT_EQ(nfp.points.size(), cachedNFP.points.size());
}

TEST_F(NFPTests, InnerNFP) {
    // Container
    Polygon container({{0,0}, {500,0}, {500,300}, {0,300}});

    // Part to place inside
    Polygon part({{0,0}, {100,0}, {100,50}, {0,50}});

    std::vector<Polygon> innerNFPs = calculator.getInnerNFP(container, part);

    EXPECT_FALSE(innerNFPs.empty());

    // First NFP should represent valid placement area
    Polygon& nfp = innerNFPs[0];
    BoundingBox bounds = GeometryUtil::getPolygonBounds(nfp);

    // Valid area should be (500-100) x (300-50) = 400x250
    EXPECT_NEAR(bounds.width, 400.0, 1.0);
    EXPECT_NEAR(bounds.height, 250.0, 1.0);
}

TEST_F(NFPTests, InnerNFPWithHoles) {
    // Container with a hole
    Polygon container({{0,0}, {500,0}, {500,300}, {0,300}});
    Polygon hole({{200,100}, {300,100}, {300,200}, {200,200}});
    container.children.push_back(hole);

    Polygon part({{0,0}, {50,0}, {50,50}, {0,50}});

    std::vector<Polygon> innerNFPs = calculator.getInnerNFP(container, part);

    // Should return valid placement regions
    EXPECT_FALSE(innerNFPs.empty());

    // Verify hole is excluded (total area should be less)
    double totalArea = 0.0;
    for (const auto& nfp : innerNFPs) {
        totalArea += std::abs(GeometryUtil::polygonArea(nfp.points));
    }

    double containerArea = 500 * 300;
    double holeArea = 100 * 100;
    double partArea = 50 * 50;
    double expectedMaxArea = (containerArea - holeArea - partArea);

    EXPECT_LT(totalArea, expectedMaxArea);
}

TEST_F(NFPTests, RectangleOptimization) {
    // Test that rectangle optimization is used
    Polygon rectContainer({{0,0}, {1000,0}, {1000,500}, {0,500}});
    EXPECT_TRUE(GeometryUtil::isRectangle(rectContainer));

    Polygon part({{0,0}, {100,0}, {100,50}, {0,50}});

    // This should trigger fast rectangle optimization
    std::vector<Polygon> innerNFPs = calculator.getInnerNFP(rectContainer, part);

    EXPECT_EQ(innerNFPs.size(), 1);

    Polygon& nfp = innerNFPs[0];
    BoundingBox bounds = GeometryUtil::getPolygonBounds(nfp);

    EXPECT_NEAR(bounds.width, 900.0, 1.0);
    EXPECT_NEAR(bounds.height, 450.0, 1.0);
}

TEST_F(NFPTests, CacheThreadSafety) {
    // Test concurrent access to NFP cache
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&]() {
            Polygon A({{0,0}, {100,0}, {100,100}, {0,100}});
            Polygon B({{0,0}, {50,0}, {50,50}, {0,50}});
            A.id = i;
            B.id = i + 100;

            Polygon nfp = calculator.getOuterNFP(A, B, false);
            EXPECT_FALSE(nfp.points.empty());
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // No crashes = success
}
```

---

## LIVELLO 3: TEST FITNESS E ALGORITMO GENETICO

### 3.1 Fitness Calculation Tests

**File**: `tests/unit/FitnessTests.cpp`

```cpp
class FitnessTests : public ::testing::Test {
protected:
    DeepNestConfig config;
    PlacementWorker worker;

    void SetUp() override {
        config = DeepNestConfig::getInstance();
        config.placementType = "gravity";
        config.mergeLines = true;
        config.timeRatio = 0.5;
    }
};

TEST_F(FitnessTests, SheetAreaPenaltyCorrect) {
    // Verify fix 1.1 from IMPLEMENTATION_PLAN
    Polygon sheet({{0,0}, {500,0}, {500,300}, {0,300}});
    double sheetArea = std::abs(GeometryUtil::polygonArea(sheet.points));

    EXPECT_NEAR(sheetArea, 150000.0, 1.0);

    // Create placement scenario with 2 sheets
    std::vector<Polygon> sheets = {sheet, sheet};
    std::vector<Polygon> parts = {};  // No parts (simplest case)

    auto result = worker.placeParts(sheets, parts);

    // Fitness should include 2 * sheetArea = 300,000
    EXPECT_GT(result.fitness, 250000.0);
    EXPECT_LT(result.fitness, 350000.0);
}

TEST_F(FitnessTests, UnplacedPartsPenaltyMassive) {
    // Verify fix 1.2 from IMPLEMENTATION_PLAN
    Polygon sheet({{0,0}, {500,0}, {500,300}, {0,300}});
    double sheetArea = 150000.0;

    // Part that won't fit
    Polygon hugePart({{0,0}, {600,0}, {600,400}, {0,400}});
    double partArea = 240000.0;

    std::vector<Polygon> sheets = {sheet};
    std::vector<Polygon> parts = {hugePart};

    auto result = worker.placeParts(sheets, parts);

    // Penalty should be 100,000,000 * (240000/150000) = 160,000,000
    EXPECT_GT(result.fitness, 100000000.0);
}

TEST_F(FitnessTests, MinariaComponentPresent) {
    // Verify fix 1.3 from IMPLEMENTATION_PLAN
    Polygon sheet({{0,0}, {500,0}, {500,300}, {0,300}});
    Polygon part({{0,0}, {100,0}, {100,50}, {0,50}});

    std::vector<Polygon> sheets = {sheet};
    std::vector<Polygon> parts = {part};

    auto result = worker.placeParts(sheets, parts);

    // Fitness should include minarea component
    // For gravity mode, minarea ~ width*2 + height ~ 200 + 50 = 250
    // Should be present in fitness value

    // Debug: Check fitness components are logged
    // Fitness should be: sheetArea + (boundsWidth/sheetArea) + minarea
    // Approximately: 150000 + 0.67 + 250 = 150250.67

    EXPECT_GT(result.fitness, 150000.0);
    EXPECT_LT(result.fitness, 151000.0);
}

TEST_F(FitnessTests, FitnessFormulaMatchesJavaScript) {
    // Comprehensive test matching JavaScript behavior
    Polygon sheet({{0,0}, {500,0}, {500,300}, {0,300}});
    Polygon part1({{0,0}, {100,0}, {100,50}, {0,50}});
    Polygon part2({{0,0}, {80,0}, {80,60}, {0,60}});

    std::vector<Polygon> sheets = {sheet};
    std::vector<Polygon> parts = {part1, part2};

    auto result = worker.placeParts(sheets, parts);

    // Expected fitness formula (from JavaScript):
    // fitness = sheetArea + (boundsWidth/sheetArea) + minarea
    // Assuming both parts fit on one sheet
    // sheetArea = 150,000
    // boundsWidth ~ 180 (two parts side by side)
    // boundsWidth/sheetArea = 0.0012
    // minarea (gravity) ~ 180*2 + 110 = 470
    // Total: 150,000 + 0.0012 + 470 = 150,470.0012

    EXPECT_NEAR(result.fitness, 150470.0, 100.0);
}

TEST_F(FitnessTests, LineMergeBonus) {
    // Verify phase 2 from IMPLEMENTATION_PLAN (after implementing)
    config.mergeLines = true;

    // Create two rectangles that can merge edges
    Polygon part1({{0,0}, {100,0}, {100,50}, {0,50}});
    Polygon part2({{100,0}, {200,0}, {200,50}, {100,50}});

    // When placed side-by-side, they share a 50-unit edge
    // Line merge bonus should reduce fitness

    // TODO: Test after implementing MergeDetection
}
```

### 3.2 Genetic Algorithm Tests

**File**: `tests/unit/GeneticAlgorithmTests.cpp`

```cpp
class GeneticAlgorithmTests : public ::testing::Test {
protected:
    DeepNestConfig config;
    std::vector<Polygon> parts;
    std::vector<Polygon> sheets;

    void SetUp() override {
        config = DeepNestConfig::getInstance();
        config.populationSize = 10;
        config.mutationRate = 10;

        // Create test parts
        parts = {
            Polygon({{0,0}, {100,0}, {100,50}, {0,50}}),
            Polygon({{0,0}, {80,0}, {80,60}, {0,60}}),
            Polygon({{0,0}, {120,0}, {120,40}, {0,40}})
        };

        // Create sheet
        sheets = {Polygon({{0,0}, {500,0}, {500,300}, {0,300}})};
    }
};

TEST_F(GeneticAlgorithmTests, PopulationInitialization) {
    GeneticAlgorithm ga(parts, sheets, config);

    auto pop = ga.getPopulation();
    EXPECT_EQ(pop.getIndividuals().size(), config.populationSize);

    // Check that individuals have correct structure
    for (const auto& ind : pop.getIndividuals()) {
        EXPECT_EQ(ind.placement.size(), parts.size());
        EXPECT_EQ(ind.rotation.size(), parts.size());
    }
}

TEST_F(GeneticAlgorithmTests, FitnessVariation) {
    // CRITICAL TEST: Verify fitness is not constant
    GeneticAlgorithm ga(parts, sheets, config);
    ga.generation();  // Run one generation

    auto pop = ga.getPopulation();
    auto individuals = pop.getIndividuals();

    // Collect fitness values
    std::vector<double> fitnessValues;
    for (const auto& ind : individuals) {
        fitnessValues.push_back(ind.fitness);
    }

    // Calculate variation
    double minFitness = *std::min_element(fitnessValues.begin(), fitnessValues.end());
    double maxFitness = *std::max_element(fitnessValues.begin(), fitnessValues.end());
    double avgFitness = std::accumulate(fitnessValues.begin(), fitnessValues.end(), 0.0) / fitnessValues.size();

    // ASSERTIONS
    EXPECT_GT(minFitness, 1000.0) << "Fitness should be > 1000, not ~1.0";
    EXPECT_NE(minFitness, maxFitness) << "Fitness values should vary";

    double variation = (maxFitness - minFitness) / avgFitness;
    EXPECT_GT(variation, 0.05) << "Fitness should vary by at least 5%";

    // Log for debugging
    std::cout << "Fitness range: [" << minFitness << ", " << maxFitness << "]" << std::endl;
    std::cout << "Average: " << avgFitness << ", Variation: " << (variation * 100) << "%" << std::endl;
}

TEST_F(GeneticAlgorithmTests, EvolutionImprovement) {
    GeneticAlgorithm ga(parts, sheets, config);

    double initialBest = ga.getBestIndividual().fitness;

    // Run 20 generations
    for (int i = 0; i < 20; ++i) {
        ga.generation();
    }

    double finalBest = ga.getBestIndividual().fitness;

    // Best fitness should improve (decrease) over time
    EXPECT_LT(finalBest, initialBest) << "GA should evolve better solutions";

    double improvement = (initialBest - finalBest) / initialBest;
    EXPECT_GT(improvement, 0.05) << "Should improve by at least 5% over 20 generations";

    std::cout << "Initial best: " << initialBest << std::endl;
    std::cout << "Final best: " << finalBest << std::endl;
    std::cout << "Improvement: " << (improvement * 100) << "%" << std::endl;
}

TEST_F(GeneticAlgorithmTests, MutationWorks) {
    Individual ind;
    ind.placement = {&parts[0], &parts[1], &parts[2]};
    ind.rotation = {0, 0, 0};

    Individual mutated = ind.clone();
    mutated.mutate(100, 4);  // 100% mutation rate

    // At least one rotation or placement should have changed
    bool rotationChanged = false;
    bool placementChanged = false;

    for (size_t i = 0; i < mutated.rotation.size(); ++i) {
        if (mutated.rotation[i] != ind.rotation[i]) {
            rotationChanged = true;
        }
    }

    for (size_t i = 0; i < mutated.placement.size(); ++i) {
        if (mutated.placement[i] != ind.placement[i]) {
            placementChanged = true;
        }
    }

    EXPECT_TRUE(rotationChanged || placementChanged);
}

TEST_F(GeneticAlgorithmTests, CrossoverWorks) {
    Population pop;
    pop.initialize(parts, config);

    auto individuals = pop.getIndividuals();
    auto [child1, child2] = pop.crossover(individuals[0], individuals[1]);

    // Children should be different from parents
    EXPECT_EQ(child1.placement.size(), individuals[0].placement.size());
    EXPECT_EQ(child2.placement.size(), individuals[1].placement.size());
}
```

---

## LIVELLO 4: TEST DI CONFRONTO CON JAVASCRIPT

### 4.1 Fitness Comparison Tests

**File**: `tests/comparison/FitnessComparisonTests.cpp`

```cpp
class FitnessComparisonTests : public ::testing::Test {
protected:
    // Load JavaScript test results
    struct JSTestCase {
        std::vector<Polygon> parts;
        std::vector<Polygon> sheets;
        double expectedFitness;
        std::string description;
    };

    std::vector<JSTestCase> loadJSTestCases() {
        // Load from testdata/expected/fitness_tests.json
        // This file contains results from running JavaScript version
        return {};  // TODO: Implement
    }
};

TEST_F(FitnessComparisonTests, MatchesJavaScriptFitness) {
    auto testCases = loadJSTestCases();

    for (const auto& testCase : testCases) {
        PlacementWorker worker;
        auto result = worker.placeParts(testCase.sheets, testCase.parts);

        // C++ fitness should match JavaScript within 1% tolerance
        double relativeDiff = std::abs(result.fitness - testCase.expectedFitness) / testCase.expectedFitness;

        EXPECT_LT(relativeDiff, 0.01)
            << "Test case: " << testCase.description
            << "\nExpected: " << testCase.expectedFitness
            << "\nGot: " << result.fitness;
    }
}
```

### 4.2 NFP Comparison Tests

**File**: `tests/comparison/NFPComparisonTests.cpp`

```cpp
TEST(NFPComparison, MinkowskiMatchesJavaScript) {
    // Load test cases with known JavaScript NFP results
    auto testCases = loadNFPTestCases("testdata/expected/nfp_tests.json");

    for (const auto& testCase : testCases) {
        Polygon nfp_cpp = MinkowskiSum::calculateNFP(testCase.A, testCase.B);
        Polygon nfp_js = testCase.expectedNFP;

        // Compare polygons
        EXPECT_TRUE(polygonsApproximatelyEqual(nfp_cpp, nfp_js, 0.01))
            << "NFP mismatch for test case: " << testCase.description;
    }
}

bool polygonsApproximatelyEqual(const Polygon& p1, const Polygon& p2, double tolerance) {
    if (p1.points.size() != p2.points.size()) {
        return false;
    }

    for (size_t i = 0; i < p1.points.size(); ++i) {
        double dist = std::sqrt(
            std::pow(p1.points[i].x - p2.points[i].x, 2) +
            std::pow(p1.points[i].y - p2.points[i].y, 2)
        );
        if (dist > tolerance) {
            return false;
        }
    }

    return true;
}
```

---

## LIVELLO 5: TEST END-TO-END

### 5.1 Complete Nesting Tests

**File**: `tests/integration/EndToEndTests.cpp`

```cpp
class EndToEndTests : public ::testing::Test {
protected:
    DeepNestSolver solver;

    void SetUp() override {
        solver.setSpacing(5.0);
        solver.setRotations(4);
        solver.setPopulationSize(10);
        solver.setPlacementType("gravity");
    }
};

TEST_F(EndToEndTests, SimpleRectangleNesting) {
    // Add parts
    Polygon part1({{0,0}, {100,0}, {100,50}, {0,50}});
    Polygon part2({{0,0}, {80,0}, {80,60}, {0,60}});

    solver.addPart(part1, 5, "Rect100x50");
    solver.addPart(part2, 3, "Rect80x60");

    // Add sheet
    Polygon sheet({{0,0}, {500,0}, {500,300}, {0,300}});
    solver.addSheet(sheet, 1, "Sheet500x300");

    // Run nesting
    solver.start(50);  // 50 generations

    while (solver.isRunning()) {
        solver.step();
    }

    // Get results
    const NestResult* result = solver.getBestResult();

    ASSERT_NE(result, nullptr);
    EXPECT_GT(result->fitness, 0.0);
    EXPECT_FALSE(result->placements.empty());

    // All parts should be placed
    int totalPlaced = 0;
    for (const auto& sheet : result->placements) {
        totalPlaced += sheet.size();
    }
    EXPECT_EQ(totalPlaced, 8);  // 5 + 3 parts
}

TEST_F(EndToEndTests, ComplexShapes) {
    // Load complex shapes from SVG
    SVGLoader loader;
    auto shapes = loader.loadShapes("testdata/polygons/complex_shapes.svg");

    for (const auto& shape : shapes) {
        Polygon poly = QtBoostConverter::fromQPainterPath(shape.path);
        solver.addPart(poly, 1, shape.id.toStdString());
    }

    Polygon sheet({{0,0}, {1000,0}, {1000,800}, {0,800}});
    solver.addSheet(sheet, 2, "Sheet1000x800");

    solver.start(100);

    while (solver.isRunning()) {
        solver.step();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const NestResult* result = solver.getBestResult();
    ASSERT_NE(result, nullptr);

    // Verify quality metrics
    EXPECT_GT(result->fitness, 0.0);
    EXPECT_LT(result->fitness, 10000000.0);  // Reasonable range
}

TEST_F(EndToEndTests, EvolutionProgression) {
    // Track fitness improvement over generations
    std::vector<double> fitnessHistory;

    Polygon part({{0,0}, {100,0}, {100,50}, {0,50}});
    Polygon sheet({{0,0}, {500,0}, {500,300}, {0,300}});

    solver.addPart(part, 10, "Part");
    solver.addSheet(sheet, 2, "Sheet");

    solver.setResultCallback([&](const NestResult& result) {
        fitnessHistory.push_back(result.fitness);
    });

    solver.start(100);

    while (solver.isRunning()) {
        solver.step();
    }

    // Fitness should generally trend downward (improving)
    ASSERT_GT(fitnessHistory.size(), 5);

    double firstFitness = fitnessHistory[0];
    double lastFitness = fitnessHistory.back();

    EXPECT_LT(lastFitness, firstFitness) << "Final fitness should be better than initial";

    // Check for consistent improvement trend
    int improvements = 0;
    for (size_t i = 1; i < fitnessHistory.size(); ++i) {
        if (fitnessHistory[i] < fitnessHistory[i-1]) {
            improvements++;
        }
    }

    double improvementRate = static_cast<double>(improvements) / fitnessHistory.size();
    EXPECT_GT(improvementRate, 0.2) << "Should see improvements in at least 20% of generations";
}
```

---

## LIVELLO 6: TEST DI PERFORMANCE

### 6.1 Benchmark Tests

**File**: `tests/performance/BenchmarkPerformance.cpp`

```cpp
#include <benchmark/benchmark.h>

static void BM_MinkowskiSum(benchmark::State& state) {
    Polygon A({{0,0}, {100,0}, {100,100}, {0,100}});
    Polygon B({{0,0}, {50,0}, {50,50}, {0,50}});

    for (auto _ : state) {
        Polygon nfp = MinkowskiSum::calculateNFP(A, B);
        benchmark::DoNotOptimize(nfp);
    }
}
BENCHMARK(BM_MinkowskiSum);

static void BM_PlacementWorker(benchmark::State& state) {
    std::vector<Polygon> sheets = {Polygon({{0,0}, {500,0}, {500,300}, {0,300}})};
    std::vector<Polygon> parts;

    for (int i = 0; i < 10; ++i) {
        parts.push_back(Polygon({{0,0}, {50,0}, {50,50}, {0,50}}));
    }

    PlacementWorker worker;

    for (auto _ : state) {
        auto result = worker.placeParts(sheets, parts);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_PlacementWorker);

static void BM_GeneticAlgorithm(benchmark::State& state) {
    std::vector<Polygon> parts;
    for (int i = 0; i < 20; ++i) {
        parts.push_back(Polygon({{0,0}, {50,0}, {50,50}, {0,50}}));
    }

    std::vector<Polygon> sheets = {Polygon({{0,0}, {1000,0}, {1000,800}, {0,800}})};

    DeepNestConfig config;
    config.populationSize = 10;

    for (auto _ : state) {
        GeneticAlgorithm ga(parts, sheets, config);
        ga.generation();
        benchmark::DoNotOptimize(ga);
    }
}
BENCHMARK(BM_GeneticAlgorithm);

BENCHMARK_MAIN();
```

### 6.2 Memory Leak Tests

```bash
# Run with valgrind
valgrind --leak-check=full --show-leak-kinds=all \
    --track-origins=yes --verbose \
    ./build/bin/EndToEndTests

# Expected output:
# "All heap blocks were freed -- no leaks are possible"
```

---

## LIVELLO 7: TEST DI REGRESSIONE

### 7.1 Known Good Results

**File**: `tests/regression/RegressionSuite.cpp`

```cpp
class RegressionTests : public ::testing::Test {
protected:
    struct RegressionCase {
        std::string name;
        std::string inputFile;
        double expectedBestFitness;
        int expectedGenerationsToConverge;
        double fitnessTolerance;
    };

    std::vector<RegressionCase> cases = {
        {"Simple10Parts", "testdata/benchmarks/simple_10.json", 250000.0, 20, 0.05},
        {"Complex20Parts", "testdata/benchmarks/complex_20.json", 850000.0, 50, 0.10},
        {"LargeScale50Parts", "testdata/benchmarks/large_50.json", 2500000.0, 100, 0.15}
    };
};

TEST_F(RegressionTests, AllBenchmarksPass) {
    for (const auto& testCase : cases) {
        DeepNestSolver solver;

        // Load test case
        auto data = loadTestCase(testCase.inputFile);
        for (const auto& part : data.parts) {
            solver.addPart(part.polygon, part.quantity, part.name);
        }
        for (const auto& sheet : data.sheets) {
            solver.addSheet(sheet.polygon, sheet.quantity, sheet.name);
        }

        // Run nesting
        solver.start(testCase.expectedGenerationsToConverge);
        while (solver.isRunning()) {
            solver.step();
        }

        // Check results
        const NestResult* result = solver.getBestResult();
        ASSERT_NE(result, nullptr) << "Test case: " << testCase.name;

        double relativeDiff = std::abs(result->fitness - testCase.expectedBestFitness)
                            / testCase.expectedBestFitness;

        EXPECT_LT(relativeDiff, testCase.fitnessTolerance)
            << "Test case: " << testCase.name
            << "\nExpected fitness: " << testCase.expectedBestFitness
            << "\nGot: " << result->fitness;
    }
}
```

---

## DATI DI TEST

### Generazione Test Data da JavaScript

**Script**: `tools/generate_test_data.js`

```javascript
// Run this with Node.js to generate test data
const DeepNest = require('./main/deepnest');
const fs = require('fs');

// Generate fitness test cases
const fitnessTests = [];

const sheet = [{x:0, y:0}, {x:500, y:0}, {x:500, y:300}, {x:0, y:300}];
const part1 = [{x:0, y:0}, {x:100, y:0}, {x:100, y:50}, {x:0, y:50}];
const part2 = [{x:0, y:0}, {x:80, y:0}, {x:80, y:60}, {x:0, y:60}];

const solver = new DeepNest();
solver.addSheet(sheet, 1);
solver.addPart(part1, 5);
solver.addPart(part2, 3);

solver.start();

// After completion
setTimeout(() => {
    const result = solver.getBestResult();

    fitnessTests.push({
        description: "5x Rect100x50 + 3x Rect80x60 in Sheet500x300",
        parts: [
            {polygon: part1, quantity: 5},
            {polygon: part2, quantity: 3}
        ],
        sheets: [
            {polygon: sheet, quantity: 1}
        ],
        expectedFitness: result.fitness,
        expectedPlacements: result.placements
    });

    fs.writeFileSync(
        'testdata/expected/fitness_tests.json',
        JSON.stringify(fitnessTests, null, 2)
    );

    console.log('Test data generated');
}, 30000);  // Wait 30 seconds for nesting to complete
```

---

## AUTOMAZIONE TEST

### CMakeLists.txt per Test Suite

```cmake
# tests/CMakeLists.txt

enable_testing()

# Google Test
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Unit tests
add_executable(unit_tests
    unit/GeometryTests.cpp
    unit/TransformationTests.cpp
    unit/MinkowskiTests.cpp
    unit/NFPTests.cpp
    unit/FitnessTests.cpp
    unit/GeneticAlgorithmTests.cpp
)

target_link_libraries(unit_tests
    deepnest
    ${GTEST_LIBRARIES}
    pthread
)

add_test(NAME UnitTests COMMAND unit_tests)

# Integration tests
add_executable(integration_tests
    integration/EndToEndTests.cpp
    integration/PipelineTests.cpp
)

target_link_libraries(integration_tests
    deepnest
    ${GTEST_LIBRARIES}
    pthread
)

add_test(NAME IntegrationTests COMMAND integration_tests)

# Comparison tests
add_executable(comparison_tests
    comparison/JSComparisonTests.cpp
    comparison/FitnessComparisonTests.cpp
    comparison/NFPComparisonTests.cpp
)

target_link_libraries(comparison_tests
    deepnest
    ${GTEST_LIBRARIES}
    pthread
)

add_test(NAME ComparisonTests COMMAND comparison_tests)

# Performance benchmarks
find_package(benchmark REQUIRED)

add_executable(performance_benchmarks
    performance/BenchmarkPerformance.cpp
)

target_link_libraries(performance_benchmarks
    deepnest
    benchmark::benchmark
    pthread
)

# Regression suite
add_executable(regression_tests
    regression/RegressionSuite.cpp
)

target_link_libraries(regression_tests
    deepnest
    ${GTEST_LIBRARIES}
    pthread
)

add_test(NAME RegressionTests COMMAND regression_tests)
```

### Script per Test Completo

**File**: `run_all_tests.sh`

```bash
#!/bin/bash

set -e  # Exit on error

echo "==================================="
echo "DeepNest C++ - Complete Test Suite"
echo "==================================="

# Build
echo ""
echo "Building..."
cd build
cmake ..
make -j$(nproc)

# Unit tests
echo ""
echo "Running Unit Tests..."
./unit_tests --gtest_output=xml:test_results/unit_tests.xml

# Integration tests
echo ""
echo "Running Integration Tests..."
./integration_tests --gtest_output=xml:test_results/integration_tests.xml

# Comparison tests
echo ""
echo "Running Comparison Tests..."
./comparison_tests --gtest_output=xml:test_results/comparison_tests.xml

# Regression tests
echo ""
echo "Running Regression Tests..."
./regression_tests --gtest_output=xml:test_results/regression_tests.xml

# Performance benchmarks
echo ""
echo "Running Performance Benchmarks..."
./performance_benchmarks --benchmark_out=test_results/benchmarks.json --benchmark_out_format=json

# Memory check
echo ""
echo "Running Memory Leak Tests..."
valgrind --leak-check=full --error-exitcode=1 ./unit_tests > test_results/valgrind.log 2>&1

echo ""
echo "==================================="
echo "All tests passed successfully!"
echo "==================================="
```

---

## CRITERI DI ACCETTAZIONE

### Test Minimi per Release

Prima di considerare l'implementazione completa, TUTTI i seguenti test devono passare:

#### ✅ **Fitness Calculation**
- [ ] `FitnessTests::SheetAreaPenaltyCorrect`
- [ ] `FitnessTests::UnplacedPartsPenaltyMassive`
- [ ] `FitnessTests::MinariaComponentPresent`
- [ ] `FitnessTests::FitnessFormulaMatchesJavaScript`

#### ✅ **Genetic Algorithm**
- [ ] `GeneticAlgorithmTests::FitnessVariation` - **CRITICO**
- [ ] `GeneticAlgorithmTests::EvolutionImprovement` - **CRITICO**
- [ ] `GeneticAlgorithmTests::MutationWorks`
- [ ] `GeneticAlgorithmTests::CrossoverWorks`

#### ✅ **NFP Calculation**
- [ ] `MinkowskiTests::SimpleSquares`
- [ ] `MinkowskiTests::WithHoles`
- [ ] `NFPTests::OuterNFP`
- [ ] `NFPTests::InnerNFP`
- [ ] `NFPTests::RectangleOptimization`

#### ✅ **End-to-End**
- [ ] `EndToEndTests::SimpleRectangleNesting`
- [ ] `EndToEndTests::EvolutionProgression` - **CRITICO**

#### ✅ **Comparison**
- [ ] `FitnessComparisonTests::MatchesJavaScriptFitness` - **CRITICO**
- [ ] `NFPComparison::MinkowskiMatchesJavaScript`

#### ✅ **Memory & Stability**
- [ ] Valgrind reports 0 memory leaks
- [ ] No segfaults in 100 consecutive runs

---

## METRICHE DI QUALITÀ

### Code Coverage Target
- **Minimum**: 80% line coverage
- **Target**: 90% line coverage
- **Critical paths**: 100% coverage (fitness calculation, GA evolution)

### Performance Targets
- **Minkowski Sum**: < 100ms per calculation (simple polygons)
- **PlacementWorker**: < 500ms per generation (10 parts)
- **Full Nesting**: < 60 seconds (20 parts, 100 generations)

### Comparison Tolerance
- **Fitness values**: < 1% difference from JavaScript
- **NFP coordinates**: < 0.01 unit difference
- **Final placement**: Similar or better utilization

---

## PRIORITÀ TEST

### FASE 1: Critici (Settimana 1)
1. `FitnessTests::*` - Tutti i test fitness
2. `GeneticAlgorithmTests::FitnessVariation`
3. `GeneticAlgorithmTests::EvolutionImprovement`

### FASE 2: Importanti (Settimana 2)
1. `MinkowskiTests::*` - Tutti i test Minkowski
2. `NFPTests::*` - Tutti i test NFP
3. `EndToEndTests::SimpleRectangleNesting`

### FASE 3: Validazione (Settimana 3)
1. `FitnessComparisonTests::*` - Confronto con JavaScript
2. `NFPComparison::*` - Confronto NFP
3. `EndToEndTests::EvolutionProgression`

### FASE 4: Stabilità (Settimana 4)
1. Memory leak tests
2. Stress tests
3. Regression suite

---

## CONCLUSIONI

Questo piano di test garantisce:

✅ **Correttezza**: Ogni componente è testato individualmente e in integrazione
✅ **Completezza**: Confronto sistematico con implementazione JavaScript
✅ **Robustezza**: Test di edge cases, memory leaks, threading
✅ **Performance**: Benchmark per identificare colli di bottiglia
✅ **Regressione**: Suite per prevenire reintroduzione di bugs

Con questo piano, l'implementazione C++ può essere validata completamente e certificata come **equivalente funzionalmente al JavaScript** prima del rilascio.
