# Piano di Test Dettagliato - DeepNest C++

## OBIETTIVO
Verificare che tutte le funzionalità dell'applicazione JavaScript DeepNest siano state portate completamente e correttamente nella versione C++.

---

## STRATEGIA DI TEST

### 1. Test Pyramid

```
           /\
          /  \
         / E2E \
        /--------\
       /Integration\
      /--------------\
     /   Unit Tests   \
    /------------------\
```

- **Unit Tests (70%)**: Test di singoli componenti isolati
- **Integration Tests (20%)**: Test di interazione tra componenti
- **End-to-End Tests (10%)**: Test di scenari completi

### 2. Approccio Test-Driven per le Fix

Per ogni fix critico:
1. Scrivere test che FALLISCE (dimostra il bug)
2. Implementare la fix
3. Verificare che il test PASSI
4. Verificare che non ci siano regressioni

---

## TEST PLAN DETTAGLIATO

## CATEGORIA 1: GEOMETRIA BASE

### TEST 1.1: Polygon Area Calculation
**Obiettivo**: Verificare che l'area dei poligoni sia calcolata correttamente

**File Test**: `tests/unit/test_geometry_util.cpp`

```cpp
#include <gtest/gtest.h>
#include "deepnest/geometry/GeometryUtil.h"

class GeometryUtilTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup comune
    }
};

TEST_F(GeometryUtilTest, PolygonArea_Square) {
    // Test: Quadrato 10x10
    std::vector<Point> square = {
        {0, 0}, {10, 0}, {10, 10}, {0, 10}
    };

    double area = GeometryUtil::polygonArea(square);

    EXPECT_DOUBLE_EQ(area, -100.0); // Negativo per orientamento CCW
}

TEST_F(GeometryUtilTest, PolygonArea_Rectangle) {
    std::vector<Point> rect = {
        {0, 0}, {20, 0}, {20, 10}, {0, 10}
    };

    double area = GeometryUtil::polygonArea(rect);

    EXPECT_DOUBLE_EQ(area, -200.0);
}

TEST_F(GeometryUtilTest, PolygonArea_Triangle) {
    std::vector<Point> triangle = {
        {0, 0}, {10, 0}, {5, 10}
    };

    double area = GeometryUtil::polygonArea(triangle);

    EXPECT_DOUBLE_EQ(area, -50.0);
}

TEST_F(GeometryUtilTest, PolygonArea_LShape) {
    // L-shape polygon
    std::vector<Point> lshape = {
        {0, 0}, {10, 0}, {10, 5}, {5, 5}, {5, 10}, {0, 10}
    };

    double area = GeometryUtil::polygonArea(lshape);
    double expectedArea = -75.0; // 10*5 + 5*5

    EXPECT_NEAR(area, expectedArea, 0.001);
}
```

**Criteri di Successo**:
- ✅ Tutti i test passano
- ✅ Area calcolata con precisione ≥ 0.001

---

### TEST 1.2: Point in Polygon
**Obiettivo**: Verificare la funzione pointInPolygon

**File Test**: `tests/unit/test_geometry_util.cpp`

```cpp
TEST_F(GeometryUtilTest, PointInPolygon_Inside) {
    std::vector<Point> square = {
        {0, 0}, {10, 0}, {10, 10}, {0, 10}
    };

    EXPECT_TRUE(GeometryUtil::pointInPolygon({5, 5}, square));
    EXPECT_TRUE(GeometryUtil::pointInPolygon({1, 1}, square));
    EXPECT_TRUE(GeometryUtil::pointInPolygon({9, 9}, square));
}

TEST_F(GeometryUtilTest, PointInPolygon_Outside) {
    std::vector<Point> square = {
        {0, 0}, {10, 0}, {10, 10}, {0, 10}
    };

    EXPECT_FALSE(GeometryUtil::pointInPolygon({15, 5}, square));
    EXPECT_FALSE(GeometryUtil::pointInPolygon({-1, 5}, square));
    EXPECT_FALSE(GeometryUtil::pointInPolygon({5, 15}, square));
}

TEST_F(GeometryUtilTest, PointInPolygon_OnEdge) {
    std::vector<Point> square = {
        {0, 0}, {10, 0}, {10, 10}, {0, 10}
    };

    // Punto sul bordo - comportamento da verificare con JS
    bool result = GeometryUtil::pointInPolygon({5, 0}, square);
    // Verificare con JavaScript quale è il risultato atteso
}
```

---

### TEST 1.3: Bounding Box
**Obiettivo**: Verificare il calcolo del bounding box

```cpp
TEST_F(GeometryUtilTest, BoundingBox_Rectangle) {
    std::vector<Point> points = {
        {10, 20}, {30, 20}, {30, 50}, {10, 50}
    };

    BoundingBox bb = BoundingBox::fromPoints(points);

    EXPECT_DOUBLE_EQ(bb.x, 10.0);
    EXPECT_DOUBLE_EQ(bb.y, 20.0);
    EXPECT_DOUBLE_EQ(bb.width, 20.0);
    EXPECT_DOUBLE_EQ(bb.height, 30.0);
}

TEST_F(GeometryUtilTest, BoundingBox_IrregularPolygon) {
    std::vector<Point> points = {
        {5, 10}, {15, 5}, {20, 15}, {10, 20}, {3, 12}
    };

    BoundingBox bb = BoundingBox::fromPoints(points);

    EXPECT_DOUBLE_EQ(bb.x, 3.0);
    EXPECT_DOUBLE_EQ(bb.y, 5.0);
    EXPECT_DOUBLE_EQ(bb.width, 17.0);  // 20 - 3
    EXPECT_DOUBLE_EQ(bb.height, 15.0); // 20 - 5
}
```

---

## CATEGORIA 2: NFP CALCULATION

### TEST 2.1: MinkowskiSum - Simple Rectangles
**Obiettivo**: Verificare Minkowski sum con forme semplici

**File Test**: `tests/unit/test_minkowski.cpp`

```cpp
#include <gtest/gtest.h>
#include "deepnest/nfp/MinkowskiSum.h"

class MinkowskiSumTest : public ::testing::Test {
protected:
    Polygon createRectangle(double width, double height) {
        Polygon rect;
        rect.points = {
            {0, 0}, {width, 0}, {width, height}, {0, height}
        };
        return rect;
    }
};

TEST_F(MinkowskiSumTest, TwoRectangles_10x5_and_5x3) {
    Polygon A = createRectangle(10, 5);
    Polygon B = createRectangle(5, 3);

    A.id = 1;
    B.id = 2;

    auto nfps = MinkowskiSum::calculateNFP(A, B, false);

    // Verifiche:
    ASSERT_GT(nfps.size(), 0) << "NFP should not be empty";

    // Il NFP di due rettangoli dovrebbe essere un rettangolo più grande
    Polygon& nfp = nfps[0];

    EXPECT_GT(nfp.points.size(), 3) << "NFP should have at least 4 points";

    // Verificare area approssimativa
    // Area NFP ≈ (width_A + width_B) * (height_A + height_B)
    double expectedArea = (10 + 5) * (5 + 3); // 15 * 8 = 120
    double actualArea = std::abs(GeometryUtil::polygonArea(nfp.points));

    EXPECT_NEAR(actualArea, expectedArea, 1.0);
}

TEST_F(MinkowskiSumTest, ScaleFactor) {
    Polygon A = createRectangle(10, 5);
    Polygon B = createRectangle(5, 3);

    double scale = MinkowskiSum::calculateScale(A, B);

    // Verificare che lo scale sia ragionevole
    EXPECT_GT(scale, 1000.0);
    EXPECT_LT(scale, 1e10);
}
```

---

### TEST 2.2: NFPCalculator - Outer NFP
**Obiettivo**: Verificare il calcolo dell'outer NFP

**File Test**: `tests/unit/test_nfp_calculator.cpp`

```cpp
#include <gtest/gtest.h>
#include "deepnest/nfp/NFPCalculator.h"
#include "deepnest/nfp/NFPCache.h"

class NFPCalculatorTest : public ::testing::Test {
protected:
    NFPCache cache;
    std::unique_ptr<NFPCalculator> calculator;

    void SetUp() override {
        calculator = std::make_unique<NFPCalculator>(cache);
    }

    Polygon createRectangle(double w, double h, int id) {
        Polygon rect;
        rect.points = {{0, 0}, {w, 0}, {w, h}, {0, h}};
        rect.id = id;
        rect.source = id;
        rect.rotation = 0.0;
        return rect;
    }
};

TEST_F(NFPCalculatorTest, OuterNFP_TwoRectangles) {
    Polygon A = createRectangle(10, 5, 1);
    Polygon B = createRectangle(5, 3, 2);

    Polygon nfp = calculator->getOuterNFP(A, B, false);

    ASSERT_FALSE(nfp.points.empty()) << "NFP should not be empty";
    EXPECT_GT(nfp.points.size(), 3);

    // Verificare che non ci siano NaN o infiniti
    for (const auto& pt : nfp.points) {
        EXPECT_FALSE(std::isnan(pt.x));
        EXPECT_FALSE(std::isnan(pt.y));
        EXPECT_FALSE(std::isinf(pt.x));
        EXPECT_FALSE(std::isinf(pt.y));
    }
}

TEST_F(NFPCalculatorTest, OuterNFP_CacheHit) {
    Polygon A = createRectangle(10, 5, 1);
    Polygon B = createRectangle(5, 3, 2);

    // Prima chiamata
    auto stats1 = calculator->getCacheStats();
    size_t misses1 = std::get<1>(stats1);

    Polygon nfp1 = calculator->getOuterNFP(A, B, false);

    auto stats2 = calculator->getCacheStats();
    size_t misses2 = std::get<1>(stats2);

    EXPECT_EQ(misses2, misses1 + 1) << "First call should be a cache miss";

    // Seconda chiamata (stessi parametri)
    auto stats3 = calculator->getCacheStats();
    size_t hits1 = std::get<0>(stats3);

    Polygon nfp2 = calculator->getOuterNFP(A, B, false);

    auto stats4 = calculator->getCacheStats();
    size_t hits2 = std::get<0>(stats4);

    EXPECT_EQ(hits2, hits1 + 1) << "Second call should be a cache hit";

    // I due NFP dovrebbero essere identici
    EXPECT_EQ(nfp1.points.size(), nfp2.points.size());
}
```

---

### TEST 2.3: NFPCalculator - Inner NFP
**Obiettivo**: Verificare il calcolo dell'inner NFP

```cpp
TEST_F(NFPCalculatorTest, InnerNFP_RectangleInSquare) {
    Polygon sheet = createRectangle(100, 100, -1); // Sheet grande
    Polygon part = createRectangle(10, 10, 1);     // Parte piccola

    auto innerNfps = calculator->getInnerNFP(sheet, part);

    ASSERT_GT(innerNfps.size(), 0) << "Inner NFP should not be empty";

    // L'inner NFP dovrebbe definire l'area dove il part può essere piazzato
    // Per un rettangolo 10x10 in uno sheet 100x100, l'area dovrebbe essere ~90x90

    Polygon& innerNfp = innerNfps[0];
    double area = std::abs(GeometryUtil::polygonArea(innerNfp.points));

    // Area approssimativa: (100-10) * (100-10) = 90 * 90 = 8100
    EXPECT_NEAR(area, 8100.0, 500.0); // Tolleranza 500
}

TEST_F(NFPCalculatorTest, InnerNFP_PartTooBig) {
    Polygon sheet = createRectangle(10, 10, -1);  // Sheet piccolo
    Polygon part = createRectangle(20, 20, 1);    // Parte troppo grande

    auto innerNfps = calculator->getInnerNFP(sheet, part);

    // Dovrebbe restituire NFP vuoto o molto piccolo
    if (!innerNfps.empty()) {
        double area = std::abs(GeometryUtil::polygonArea(innerNfps[0].points));
        EXPECT_LT(area, 10.0) << "Part too big should result in small/empty NFP";
    }
}
```

---

## CATEGORIA 3: PLACEMENT & FITNESS

### TEST 3.1: PlacementWorker - Fitness Calculation
**Obiettivo**: **CRITICO** - Verificare che il fitness sia calcolato correttamente

**File Test**: `tests/unit/test_placement_worker.cpp`

```cpp
#include <gtest/gtest.h>
#include "deepnest/placement/PlacementWorker.h"
#include "deepnest/nfp/NFPCalculator.h"
#include "deepnest/nfp/NFPCache.h"
#include "deepnest/config/DeepNestConfig.h"

class PlacementWorkerTest : public ::testing::Test {
protected:
    DeepNestConfig config;
    NFPCache cache;
    std::unique_ptr<NFPCalculator> calculator;
    std::unique_ptr<PlacementWorker> worker;

    void SetUp() override {
        config.spacing = 0.0;
        config.rotations = 1;
        config.placementType = "gravity";
        config.scale = 72.0;
        config.curveTolerance = 0.3;
        config.mergeLines = false; // Disabilitato per semplicità

        calculator = std::make_unique<NFPCalculator>(cache);
        worker = std::make_unique<PlacementWorker>(config, *calculator);
    }

    Polygon createRectangle(double w, double h, int id) {
        Polygon rect;
        rect.points = {{0, 0}, {w, 0}, {w, h}, {0, h}};
        rect.id = id;
        rect.source = id;
        rect.rotation = 0.0;
        return rect;
    }
};

TEST_F(PlacementWorkerTest, FitnessIsNotConstant) {
    // QUESTO TEST FALLISCE CON IL CODICE ATTUALE!

    Polygon sheet = createRectangle(100, 100, -1);

    // Scenario 1: 3 piccoli rettangoli (dovrebbe essere facile da piazzare)
    std::vector<Polygon> parts1;
    for (int i = 0; i < 3; i++) {
        parts1.push_back(createRectangle(10, 10, i));
    }

    auto result1 = worker->placeParts({sheet}, parts1);

    // Scenario 2: 10 rettangoli più grandi (più difficile)
    std::vector<Polygon> parts2;
    for (int i = 0; i < 10; i++) {
        parts2.push_back(createRectangle(20, 20, i + 10));
    }

    auto result2 = worker->placeParts({sheet}, parts2);

    // I fitness DEVONO essere diversi!
    EXPECT_NE(result1.fitness, result2.fitness)
        << "Fitness should not be constant for different scenarios!";

    // Il secondo scenario dovrebbe avere fitness peggiore (più alto)
    EXPECT_GT(result2.fitness, result1.fitness)
        << "More difficult scenario should have worse (higher) fitness";

    std::cout << "Fitness 1 (3 small parts): " << result1.fitness << std::endl;
    std::cout << "Fitness 2 (10 large parts): " << result2.fitness << std::endl;
}

TEST_F(PlacementWorkerTest, FitnessComponentsExist) {
    // Verificare che il fitness includa tutti i componenti

    Polygon sheet = createRectangle(100, 100, -1);

    std::vector<Polygon> parts;
    for (int i = 0; i < 5; i++) {
        parts.push_back(createRectangle(15, 15, i));
    }

    auto result = worker->placeParts({sheet}, parts);

    // Il fitness dovrebbe essere > 0 e < infinito
    EXPECT_GT(result.fitness, 0.0);
    EXPECT_LT(result.fitness, std::numeric_limits<double>::max());

    // Il fitness dovrebbe includere:
    // 1. Sheet area
    // 2. (minwidth / sheetarea) + minarea
    // 3. Penalty per parti non piazzate (se ce ne sono)

    // Test: fitness > solo sheet area
    double sheetArea = 100.0 * 100.0;
    EXPECT_GT(result.fitness, sheetArea)
        << "Fitness should include more than just sheet area";

    std::cout << "Fitness: " << result.fitness << std::endl;
    std::cout << "Placements: " << result.placements.size() << " sheets" << std::endl;
    std::cout << "Unplaced: " << result.unplacedParts.size() << " parts" << std::endl;
}

TEST_F(PlacementWorkerTest, UnplacedPartsPenalty) {
    // Scenario con parti che NON possono essere piazzate

    Polygon sheet = createRectangle(50, 50, -1); // Sheet piccolo

    std::vector<Polygon> parts;
    for (int i = 0; i < 5; i++) {
        parts.push_back(createRectangle(40, 40, i)); // Parti grandi
    }

    auto result = worker->placeParts({sheet}, parts);

    // Dovrebbero esserci parti non piazzate
    EXPECT_GT(result.unplacedParts.size(), 0)
        << "Some parts should not fit in the small sheet";

    // Il fitness dovrebbe includere una penalty ALTA
    // JavaScript: fitness += 100000000 * (area / totalsheetarea)

    double sheetArea = 50.0 * 50.0;
    double expectedMinPenalty = 100000000.0 * 0.1; // Almeno 10% della sheet area

    EXPECT_GT(result.fitness, expectedMinPenalty)
        << "Unplaced parts should have high penalty";

    std::cout << "Unplaced parts: " << result.unplacedParts.size() << std::endl;
    std::cout << "Fitness with penalty: " << result.fitness << std::endl;
}
```

---

### TEST 3.2: Placement Strategy - Gravity
**Obiettivo**: Verificare la strategia gravity

**File Test**: `tests/unit/test_placement_strategy.cpp`

```cpp
#include <gtest/gtest.h>
#include "deepnest/placement/PlacementStrategy.h"

class PlacementStrategyTest : public ::testing::Test {
protected:
    Polygon createRectangle(double w, double h) {
        Polygon rect;
        rect.points = {{0, 0}, {w, 0}, {w, h}, {0, h}};
        return rect;
    }
};

TEST_F(PlacementStrategyTest, Gravity_PreferLeftAndDown) {
    auto strategy = PlacementStrategy::create("gravity");

    Polygon part = createRectangle(10, 5);

    std::vector<PlacedPart> placed; // Nessuna parte piazzata

    // Candidate positions
    std::vector<Point> candidates = {
        {0, 0},    // Top-left (MIGLIORE)
        {10, 0},   // Top-right
        {0, 10},   // Bottom-left
        {10, 10}   // Bottom-right
    };

    Point best = strategy->findBestPosition(part, placed, candidates);

    // Dovrebbe scegliere (0, 0)
    EXPECT_DOUBLE_EQ(best.x, 0.0);
    EXPECT_DOUBLE_EQ(best.y, 0.0);
}

TEST_F(PlacementStrategyTest, Gravity_CompressWidth) {
    auto strategy = PlacementStrategy::create("gravity");

    Polygon part = createRectangle(10, 10);

    // Una parte già piazzata
    PlacedPart placed1;
    placed1.polygon = createRectangle(10, 10);
    placed1.position = {0, 0};
    placed1.rotation = 0.0;

    std::vector<PlacedPart> placed = {placed1};

    // Candidates: vicino (comprime larghezza) vs lontano
    std::vector<Point> candidates = {
        {10, 0},   // Vicino sulla destra (MIGLIORE per gravity)
        {0, 10},   // Sotto
        {20, 0},   // Lontano
    };

    Point best = strategy->findBestPosition(part, placed, candidates);

    // Gravity dovrebbe preferire comprimere la larghezza
    // Metrica: width*2 + height
    // Posizione (10, 0): width=20, height=10 → 20*2 + 10 = 50
    // Posizione (0, 10): width=10, height=20 → 10*2 + 20 = 40 (migliore!)
    // Posizione (20, 0): width=30, height=10 → 30*2 + 10 = 70

    // In realtà (0, 10) dovrebbe essere il migliore!
    EXPECT_DOUBLE_EQ(best.x, 0.0);
    EXPECT_DOUBLE_EQ(best.y, 10.0);
}
```

---

## CATEGORIA 4: GENETIC ALGORITHM

### TEST 4.1: Individual - Mutation
**Obiettivo**: Verificare che la mutazione funzioni

**File Test**: `tests/unit/test_individual.cpp`

```cpp
#include <gtest/gtest.h>
#include "deepnest/algorithm/Individual.h"
#include "deepnest/config/DeepNestConfig.h"

class IndividualTest : public ::testing::Test {
protected:
    DeepNestConfig config;
    std::vector<Polygon> parts;
    std::vector<Polygon*> partPtrs;

    void SetUp() override {
        config.rotations = 4;
        config.mutationRate = 50; // 50% per testare facilmente

        // Crea 5 parti di test
        for (int i = 0; i < 5; i++) {
            Polygon p;
            p.points = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
            p.id = i;
            p.source = i;
            parts.push_back(p);
        }

        for (auto& p : parts) {
            partPtrs.push_back(&p);
        }
    }
};

TEST_F(IndividualTest, InitializationWithRandomRotations) {
    Individual ind(partPtrs, config, 12345); // Seed fisso

    EXPECT_EQ(ind.placement.size(), 5);
    EXPECT_EQ(ind.rotation.size(), 5);

    // Le rotazioni dovrebbero essere multipli di 90° (4 rotazioni)
    for (double rot : ind.rotation) {
        EXPECT_TRUE(
            rot == 0.0 || rot == 90.0 || rot == 180.0 || rot == 270.0
        ) << "Rotation should be 0, 90, 180, or 270, got: " << rot;
    }
}

TEST_F(IndividualTest, Mutation_ChangesIndividual) {
    Individual ind(partPtrs, config, 12345);
    Individual original = ind.clone();

    // Muta con seed diverso per garantire cambiamento
    ind.mutate(100.0, config.rotations, 54321); // 100% mutation rate

    // Verifica che qualcosa sia cambiato
    bool placementChanged = false;
    bool rotationChanged = false;

    for (size_t i = 0; i < ind.placement.size(); i++) {
        if (ind.placement[i] != original.placement[i]) {
            placementChanged = true;
        }
        if (ind.rotation[i] != original.rotation[i]) {
            rotationChanged = true;
        }
    }

    EXPECT_TRUE(placementChanged || rotationChanged)
        << "Mutation with 100% rate should change something";
}

TEST_F(IndividualTest, Clone_CreatesIdenticalCopy) {
    Individual ind(partPtrs, config, 12345);
    ind.fitness = 123.45;

    Individual copy = ind.clone();

    EXPECT_EQ(copy.placement.size(), ind.placement.size());
    EXPECT_EQ(copy.rotation.size(), ind.rotation.size());
    EXPECT_DOUBLE_EQ(copy.fitness, ind.fitness);

    // Placement dovrebbe essere identico
    for (size_t i = 0; i < ind.placement.size(); i++) {
        EXPECT_EQ(copy.placement[i], ind.placement[i]);
        EXPECT_DOUBLE_EQ(copy.rotation[i], ind.rotation[i]);
    }

    // Processing flag dovrebbe essere resettato
    EXPECT_FALSE(copy.processing);
}
```

---

### TEST 4.2: Population - Crossover
**Obiettivo**: Verificare che il crossover funzioni correttamente

**File Test**: `tests/unit/test_population.cpp`

```cpp
#include <gtest/gtest.h>
#include "deepnest/algorithm/Population.h"

class PopulationTest : public ::testing::Test {
protected:
    DeepNestConfig config;
    std::vector<Polygon> parts;
    std::vector<Polygon*> partPtrs;

    void SetUp() override {
        config.populationSize = 10;
        config.mutationRate = 10;
        config.rotations = 4;

        for (int i = 0; i < 5; i++) {
            Polygon p;
            p.points = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
            p.id = i;
            p.source = i;
            parts.push_back(p);
        }

        for (auto& p : parts) {
            partPtrs.push_back(&p);
        }
    }
};

TEST_F(PopulationTest, Initialization_CreatesCorrectPopulationSize) {
    Population pop(config, 12345);
    pop.initialize(partPtrs);

    EXPECT_EQ(pop.size(), config.populationSize);
}

TEST_F(PopulationTest, Crossover_ProducesValidChildren) {
    Population pop(config, 12345);

    Individual parent1(partPtrs, config, 111);
    Individual parent2(partPtrs, config, 222);

    auto [child1, child2] = pop.crossover(parent1, parent2);

    // I children devono avere tutte le parti
    EXPECT_EQ(child1.placement.size(), partPtrs.size());
    EXPECT_EQ(child2.placement.size(), partPtrs.size());

    // Nessuna parte duplicata
    std::set<int> ids1, ids2;
    for (auto* p : child1.placement) {
        ids1.insert(p->id);
    }
    for (auto* p : child2.placement) {
        ids2.insert(p->id);
    }

    EXPECT_EQ(ids1.size(), partPtrs.size()) << "Child1 has duplicate parts";
    EXPECT_EQ(ids2.size(), partPtrs.size()) << "Child2 has duplicate parts";

    // I children dovrebbero essere diversi dai genitori
    bool child1DifferentFromParent1 = false;
    for (size_t i = 0; i < child1.placement.size(); i++) {
        if (child1.placement[i] != parent1.placement[i]) {
            child1DifferentFromParent1 = true;
            break;
        }
    }

    EXPECT_TRUE(child1DifferentFromParent1)
        << "Child should be different from parent";
}

TEST_F(PopulationTest, NextGeneration_PreservesBestIndividual) {
    Population pop(config, 12345);
    pop.initialize(partPtrs);

    // Assegna fitness (crescente)
    for (size_t i = 0; i < pop.size(); i++) {
        pop[i].fitness = 100.0 + i * 10.0;
    }

    Individual best = pop.getBest();
    double bestFitness = best.fitness;

    // Crea nuova generazione
    pop.nextGeneration();

    // Il migliore dovrebbe essere preservato (elitism)
    Individual newBest = pop.getBest();

    EXPECT_DOUBLE_EQ(newBest.fitness, bestFitness)
        << "Best individual should be preserved by elitism";
}
```

---

### TEST 4.3: Genetic Algorithm - Fitness Convergence
**Obiettivo**: **CRITICO** - Verificare che l'algoritmo genetico converga verso soluzioni migliori

**File Test**: `tests/integration/test_ga_convergence.cpp`

```cpp
#include <gtest/gtest.h>
#include "deepnest/algorithm/GeneticAlgorithm.h"
#include "deepnest/placement/PlacementWorker.h"
#include "deepnest/nfp/NFPCalculator.h"

class GAConvergenceTest : public ::testing::Test {
protected:
    DeepNestConfig config;
    std::vector<Polygon> parts;
    std::vector<Polygon*> partPtrs;
    std::vector<Polygon> sheets;

    void SetUp() override {
        config.populationSize = 10;
        config.mutationRate = 10;
        config.rotations = 4;
        config.placementType = "gravity";
        config.spacing = 0.0;
        config.mergeLines = false;

        // Sheet 100x100
        Polygon sheet;
        sheet.points = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
        sheet.id = -1;
        sheet.source = -1;
        sheets.push_back(sheet);

        // 5 rettangoli
        for (int i = 0; i < 5; i++) {
            Polygon p;
            p.points = {{0, 0}, {15, 0}, {15, 20}, {0, 20}};
            p.id = i;
            p.source = i;
            parts.push_back(p);
            partPtrs.push_back(&parts.back());
        }
    }

    void evaluatePopulation(GeneticAlgorithm& ga, PlacementWorker& worker) {
        auto& pop = ga.getPopulation();
        for (auto& ind : pop) {
            if (!ind.hasValidFitness()) {
                // Assegna id e rotation alle parti
                std::vector<Polygon> partsForPlacement;
                for (size_t i = 0; i < ind.placement.size(); i++) {
                    Polygon p = *(ind.placement[i]);
                    p.rotation = ind.rotation[i];
                    partsForPlacement.push_back(p);
                }

                auto result = worker.placeParts(sheets, partsForPlacement);
                ind.fitness = result.fitness;
                ind.processing = false;
            }
        }
    }
};

TEST_F(GAConvergenceTest, FitnessImproves_OverGenerations) {
    // QUESTO TEST FALLISCE CON IL CODICE ATTUALE!

    NFPCache cache;
    NFPCalculator calculator(cache);
    PlacementWorker worker(config, calculator);

    GeneticAlgorithm ga(partPtrs, config);

    std::vector<double> bestFitnessHistory;

    // Simula 10 generazioni
    for (int gen = 0; gen < 10; gen++) {
        evaluatePopulation(ga, worker);

        double bestFitness = ga.getBestIndividual().fitness;
        bestFitnessHistory.push_back(bestFitness);

        std::cout << "Generation " << gen << ": Best fitness = "
                  << bestFitness << std::endl;

        if (gen < 9) {
            ga.generation();
        }
    }

    // Verifiche:
    ASSERT_GT(bestFitnessHistory.size(), 2);

    // Il fitness dovrebbe migliorare (diminuire) o rimanere stabile
    double firstFitness = bestFitnessHistory[0];
    double lastFitness = bestFitnessHistory.back();

    EXPECT_LE(lastFitness, firstFitness)
        << "Best fitness should improve (decrease) or stay the same";

    // Verificare che il fitness non sia costante
    std::set<double> uniqueFitness(
        bestFitnessHistory.begin(),
        bestFitnessHistory.end()
    );

    EXPECT_GT(uniqueFitness.size(), 1)
        << "Fitness should vary across generations, not be constant!";
}
```

---

## CATEGORIA 5: END-TO-END TESTS

### TEST 5.1: Complete Nesting Scenario
**Obiettivo**: Test completo di un scenario di nesting

**File Test**: `tests/e2e/test_complete_nesting.cpp`

```cpp
#include <gtest/gtest.h>
#include "deepnest/DeepNestSolver.h"

class CompleteNestingTest : public ::testing::Test {
protected:
    DeepNestSolver solver;

    void SetUp() override {
        solver.setSpacing(2.0);
        solver.setRotations(4);
        solver.setPopulationSize(10);
        solver.setPlacementType("gravity");
    }
};

TEST_F(CompleteNestingTest, RectanglesNesting_10Parts) {
    // Sheet 200x200
    Polygon sheet;
    sheet.points = {{0, 0}, {200, 0}, {200, 200}, {0, 200}};
    solver.addSheet(sheet, 1, "Sheet1");

    // 10 rettangoli 20x30
    for (int i = 0; i < 10; i++) {
        Polygon part;
        part.points = {{0, 0}, {20, 0}, {20, 30}, {0, 30}};
        solver.addPart(part, 1, "Part" + std::to_string(i));
    }

    // Callback per tracciare il progresso
    std::vector<double> fitnessHistory;
    bool progressCalled = false;

    solver.setProgressCallback([&](const NestProgress& progress) {
        progressCalled = true;
        fitnessHistory.push_back(progress.bestFitness);

        std::cout << "Gen " << progress.generation
                  << ": fitness = " << progress.bestFitness
                  << ", progress = " << progress.percentComplete << "%"
                  << std::endl;

        EXPECT_GE(progress.generation, 0);
        EXPECT_LE(progress.percentComplete, 100.0);
    });

    // Esegui nesting per 20 generazioni
    solver.start(20);

    int iterations = 0;
    while (solver.isRunning() && iterations < 1000) {
        solver.step();
        iterations++;
    }

    // Verifiche:
    EXPECT_TRUE(progressCalled) << "Progress callback should be called";
    EXPECT_GT(fitnessHistory.size(), 0);

    const NestResult* result = solver.getBestResult();
    ASSERT_NE(result, nullptr) << "Should have a result";

    EXPECT_GT(result->fitness, 0.0);
    EXPECT_LT(result->fitness, std::numeric_limits<double>::max());
    EXPECT_GT(result->placements.size(), 0);

    // Verificare che il fitness sia variato
    std::set<double> uniqueFitness(fitnessHistory.begin(), fitnessHistory.end());
    EXPECT_GT(uniqueFitness.size(), 1) << "Fitness should not be constant!";

    // Verificare convergenza
    if (fitnessHistory.size() > 5) {
        double avgFirst5 = 0.0;
        for (int i = 0; i < 5; i++) {
            avgFirst5 += fitnessHistory[i];
        }
        avgFirst5 /= 5.0;

        double avgLast5 = 0.0;
        int start = fitnessHistory.size() - 5;
        for (int i = start; i < fitnessHistory.size(); i++) {
            avgLast5 += fitnessHistory[i];
        }
        avgLast5 /= 5.0;

        EXPECT_LE(avgLast5, avgFirst5)
            << "Average fitness should improve from first to last generations";
    }

    std::cout << "\nFinal result:" << std::endl;
    std::cout << "  Fitness: " << result->fitness << std::endl;
    std::cout << "  Sheets used: " << result->placements.size() << std::endl;
    std::cout << "  Generation: " << result->generation << std::endl;
}
```

---

## CATEGORIA 6: JAVASCRIPT COMPARISON TESTS

### TEST 6.1: NFP Comparison with JavaScript
**Obiettivo**: Verificare che i NFP siano identici a quelli di JavaScript

**Preparazione**:
1. Creare script JavaScript che genera NFP per forme di test
2. Salvare risultati in JSON
3. Caricare JSON in C++ e confrontare

**Script JavaScript** (`generate_nfp_test_data.js`):
```javascript
const fs = require('fs');

// Funzioni da background.js
function getOuterNfp(A, B) {
    // ... implementazione originale...
}

// Test cases
const testCases = [
    {
        name: "two_rectangles_10x5_and_5x3",
        A: [{x:0,y:0}, {x:10,y:0}, {x:10,y:5}, {x:0,y:5}],
        B: [{x:0,y:0}, {x:5,y:0}, {x:5,y:3}, {x:0,y:3}]
    },
    // ... altri test cases...
];

const results = testCases.map(tc => {
    const nfp = getOuterNfp(tc.A, tc.B);
    return {
        name: tc.name,
        A: tc.A,
        B: tc.B,
        nfp: nfp
    };
});

fs.writeFileSync('nfp_test_data.json', JSON.stringify(results, null, 2));
```

**Test C++**:
```cpp
TEST(JSComparison, NFP_TwoRectangles) {
    // Carica dati di test
    auto testData = loadJSONFromFile("nfp_test_data.json");
    auto testCase = testData[0]; // "two_rectangles_10x5_and_5x3"

    // Converti da JSON
    Polygon A = polygonFromJSON(testCase["A"]);
    Polygon B = polygonFromJSON(testCase["B"]);
    Polygon expectedNFP = polygonFromJSON(testCase["nfp"]);

    // Calcola NFP in C++
    NFPCache cache;
    NFPCalculator calc(cache);
    Polygon actualNFP = calc.getOuterNFP(A, B, false);

    // Confronta
    EXPECT_TRUE(polygonsAlmostEqual(actualNFP, expectedNFP, 0.01))
        << "NFP should match JavaScript result";
}

// Helper function
bool polygonsAlmostEqual(const Polygon& p1, const Polygon& p2, double tolerance) {
    if (p1.points.size() != p2.points.size()) {
        return false;
    }

    for (size_t i = 0; i < p1.points.size(); i++) {
        double dx = std::abs(p1.points[i].x - p2.points[i].x);
        double dy = std::abs(p1.points[i].y - p2.points[i].y);

        if (dx > tolerance || dy > tolerance) {
            return false;
        }
    }

    return true;
}
```

---

### TEST 6.2: Fitness Comparison
**Obiettivo**: Verificare che il fitness sia calcolato identicamente

**Script JavaScript**:
```javascript
// Calcola fitness per uno scenario specifico
const scenario = {
    sheet: {/* ... */},
    parts: [/* ... */],
    placements: [/* ... */]
};

const fitness = calculateFitness(scenario);

fs.writeFileSync('fitness_test_data.json', JSON.stringify({
    scenario: scenario,
    expectedFitness: fitness
}, null, 2));
```

**Test C++**:
```cpp
TEST(JSComparison, Fitness_Calculation) {
    auto data = loadJSONFromFile("fitness_test_data.json");

    // ... setup da JSON ...

    PlacementWorker worker(config, calculator);
    auto result = worker.placeParts(sheets, parts);

    double expectedFitness = data["expectedFitness"].asDouble();
    double actualFitness = result.fitness;

    // Tolleranza 0.1%
    EXPECT_NEAR(actualFitness, expectedFitness, expectedFitness * 0.001)
        << "Fitness should match JavaScript calculation";
}
```

---

## CATEGORIA 7: PERFORMANCE TESTS

### TEST 7.1: NFP Cache Performance

```cpp
TEST(Performance, NFPCache_Speedup) {
    NFPCache cache;
    NFPCalculator calc(cache);

    // Poligoni complessi (100 punti ciascuno)
    Polygon A = generateComplexPolygon(100);
    Polygon B = generateComplexPolygon(80);

    A.id = 1;
    B.id = 2;

    // Prima chiamata (cache miss)
    auto start = std::chrono::high_resolution_clock::now();
    calc.getOuterNFP(A, B, false);
    auto end = std::chrono::high_resolution_clock::now();
    auto uncachedTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Seconda chiamata (cache hit)
    start = std::chrono::high_resolution_clock::now();
    calc.getOuterNFP(A, B, false);
    end = std::chrono::high_resolution_clock::now();
    auto cachedTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Uncached: " << uncachedTime.count() << " μs" << std::endl;
    std::cout << "Cached: " << cachedTime.count() << " μs" << std::endl;
    std::cout << "Speedup: " << (double)uncachedTime.count() / cachedTime.count() << "x" << std::endl;

    // Cache dovrebbe essere almeno 10x più veloce
    EXPECT_LT(cachedTime.count(), uncachedTime.count() / 10);
}
```

---

## CATEGORIA 8: REGRESSION TESTS

### TEST 8.1: Fitness Not Constant (Current Bug)

```cpp
TEST(Regression, Issue001_FitnessConstant) {
    // BUG: Fitness è sempre 1.0
    // Questo test documenta il bug e dovrebbe FALLIRE con il codice attuale
    // Dovrebbe PASSARE dopo le fix

    DeepNestSolver solver;
    solver.setPopulationSize(5);
    solver.setRotations(4);

    Polygon sheet;
    sheet.points = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
    solver.addSheet(sheet, 1, "Sheet1");

    for (int i = 0; i < 5; i++) {
        Polygon part;
        part.points = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
        solver.addPart(part, 1, "Part" + std::to_string(i));
    }

    std::set<double> uniqueFitness;

    solver.setProgressCallback([&](const NestProgress& progress) {
        uniqueFitness.insert(progress.bestFitness);
        std::cout << "Fitness: " << progress.bestFitness << std::endl;
    });

    solver.runUntilComplete(10);

    // ASPETTATIVA: Dovrebbe esserci variazione nel fitness
    EXPECT_GT(uniqueFitness.size(), 1)
        << "BUG: Fitness should not be constant!";

    // ASPETTATIVA: Nessun fitness dovrebbe essere esattamente 1.0
    EXPECT_EQ(uniqueFitness.count(1.0), 0)
        << "BUG: Fitness of 1.0 suggests incomplete calculation";
}
```

---

## EXECUTION PLAN

### Phase 1: Setup (Day 1)
- [ ] Setup Google Test framework
- [ ] Create test directory structure
- [ ] Write helper functions (createRectangle, loadJSON, etc.)
- [ ] Create JSON test data generator script

### Phase 2: Critical Tests (Days 2-3)
**Priority: Fix Fitness Calculation**
- [ ] TEST 3.1: PlacementWorker Fitness Calculation
- [ ] TEST 8.1: Regression test for fitness bug
- [ ] TEST 4.3: GA Convergence test

**Goal**: Questi test devono PASSARE dopo le fix del fitness

### Phase 3: Core Functionality (Days 4-6)
- [ ] TEST 1.x: Geometry utilities
- [ ] TEST 2.x: NFP calculation
- [ ] TEST 4.1-4.2: Individual & Population

### Phase 4: Integration (Days 7-9)
- [ ] TEST 5.1: Complete nesting scenario
- [ ] TEST 3.2: Placement strategies

### Phase 5: Validation (Days 10-12)
- [ ] TEST 6.x: JavaScript comparison
- [ ] TEST 7.x: Performance tests
- [ ] Coverage analysis

### Phase 6: Documentation (Days 13-14)
- [ ] Document test results
- [ ] Create test report
- [ ] Update README with test instructions

---

## SUCCESS CRITERIA

### Must Have (Blockers):
1. ✅ TEST 8.1 passes (Fitness is not constant)
2. ✅ TEST 4.3 passes (GA convergence)
3. ✅ TEST 3.1 passes (Fitness calculation correct)
4. ✅ TEST 5.1 passes (End-to-end nesting works)

### Should Have:
5. ✅ All geometry tests pass (TEST 1.x)
6. ✅ All NFP tests pass (TEST 2.x)
7. ✅ All GA tests pass (TEST 4.x)
8. ✅ Unit test coverage > 80%

### Nice to Have:
9. ✅ JavaScript comparison tests pass (TEST 6.x)
10. ✅ Performance benchmarks meet targets
11. ✅ Integration test coverage > 70%

---

## TEST DATA

### Geometric Test Shapes

```cpp
// Helper functions per creare forme di test

Polygon createRectangle(double w, double h, int id = 0) {
    Polygon rect;
    rect.points = {{0, 0}, {w, 0}, {w, h}, {0, h}};
    rect.id = id;
    rect.source = id;
    return rect;
}

Polygon createLShape(double w, double h, double cutW, double cutH, int id = 0) {
    Polygon lshape;
    lshape.points = {
        {0, 0}, {w, 0}, {w, h - cutH},
        {cutW, h - cutH}, {cutW, h}, {0, h}
    };
    lshape.id = id;
    lshape.source = id;
    return lshape;
}

Polygon createTShape(double w, double h, int id = 0) {
    // T-shape implementation
    // ...
}

Polygon createCross(double size, int id = 0) {
    // Cross shape implementation
    // ...
}

Polygon generateComplexPolygon(int numPoints, double radius = 50.0, int id = 0) {
    // Genera poligono irregolare con numPoints vertici
    // Utile per stress testing
    // ...
}
```

---

## CONTINUOUS INTEGRATION

### GitHub Actions Workflow

```yaml
name: DeepNest C++ Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest

    steps:
    - uses: actions/checkout@v2

    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y qt5-default libboost-all-dev

    - name: Install Google Test
      run: |
        git clone https://github.com/google/googletest.git
        cd googletest
        cmake .
        make
        sudo make install

    - name: Build
      run: |
        cd deepnest-cpp
        mkdir build && cd build
        cmake ..
        make

    - name: Run Tests
      run: |
        cd deepnest-cpp/build
        ctest --output-on-failure

    - name: Generate Coverage Report
      run: |
        # gcov/lcov commands
        # ...
```

---

## CONCLUSION

Questo piano di test fornisce una copertura completa per:
1. ✅ Verificare che tutte le funzionalità JavaScript siano implementate in C++
2. ✅ Identificare e documentare i bug attuali (fitness costante)
3. ✅ Validare le fix implementate
4. ✅ Garantire che non ci siano regressioni future
5. ✅ Confrontare risultati con JavaScript per validazione assoluta

**Focus Immediato**: Implementare e far passare TEST 3.1 e TEST 8.1 dopo le fix del fitness!
