# Piano di Implementazione - Funzionalità Mancanti DeepNest C++

## Sommario Esecutivo

Analisi completa dell'implementazione C++ di DeepNest ha rivelato **problemi critici** che spiegano perché l'algoritmo genetico non funziona correttamente e il fitness rimane costante a 1.0.

### Problemi Identificati per Priorità

| Priorità | Componente | Problema | Impatto |
|----------|------------|----------|---------|
| 🔴 CRITICO | Fitness Calculation | Sheet area penalty = 1.0 invece di sheetArea | Fitness non varia tra individui |
| 🔴 CRITICO | Fitness Calculation | Manca componente minarea | Ottimizzazione placement incorretta |
| 🔴 CRITICO | Fitness Calculation | Penalty parti non piazzate = 2.0 invece di 100M | GA non prioritizza piazzare tutte le parti |
| 🔴 CRITICO | Line Merge | Completamente mancante | Nessun bonus per linee coincidenti |
| 🟡 ALTO | Fitness Calculation | Manca componente minwidth/sheetarea | Formula fitness incompleta |
| 🟡 ALTO | Placement Strategy | Non calcola/restituisce area per posizione | Fitness non include metrica qualità |
| 🟡 ALTO | NFP Calculation | Orbital tracing mancante | Nessun fallback se Minkowski fallisce |
| 🟢 MEDIO | Minkowski Sum | Manca reference point shift | Output in frame di riferimento diverso |
| 🟢 MEDIO | NFP Calculation | Manca supporto searchEdges | Non esplora tutte le regioni NFP |

---

## FASE 1: FIX CRITICI FITNESS CALCULATION (PRIORITÀ MASSIMA)

### 1.1 Fix Sheet Area Penalty

**File**: `deepnest-cpp/src/placement/PlacementWorker.cpp`
**Linea**: 101
**Tempo stimato**: 5 minuti

**PRIMA**:
```cpp
fitness += 1.0;  // Line 101
```

**DOPO**:
```cpp
// Add full sheet area as penalty (matches JavaScript background.js:848)
fitness += sheetArea;
```

**Test di verifica**:
```cpp
// Con sheet 500x300 = 150,000 unità²
// Prima: fitness += 1.0
// Dopo: fitness += 150000.0
// Il fitness dovrebbe essere ordini di grandezza diversi
```

---

### 1.2 Fix Unplaced Parts Penalty

**File**: `deepnest-cpp/src/placement/PlacementWorker.cpp`
**Linea**: 472
**Tempo stimato**: 15 minuti

**PRIMA**:
```cpp
fitness += 2.0 * parts.size();  // Line 472
```

**DOPO**:
```cpp
// Add massive penalty for unplaced parts (matches JavaScript background.js:1167)
for (const auto& part : parts) {
    double partArea = std::abs(GeometryUtil::polygonArea(part.points));
    fitness += 100000000.0 * (partArea / totalSheetArea);
}
```

**Calcolo totalSheetArea**:
```cpp
// Prima del loop, calcola totalSheetArea
double totalSheetArea = 0.0;
for (const auto& sheet : originalSheets) {
    totalSheetArea += std::abs(GeometryUtil::polygonArea(sheet.points));
}
// Gestione edge case
if (totalSheetArea < 1.0) {
    totalSheetArea = 1.0;
}
```

**Test di verifica**:
```cpp
// Esempio: parte 100x50 = 5000 unità², sheet totale = 150,000 unità²
// JavaScript: 100,000,000 * (5000/150000) = 3,333,333
// C++ vecchio: 2.0
// Differenza: 1.6 milioni di volte!
```

---

### 1.3 Aggiungi Componente minarea al Fitness

**File**: `deepnest-cpp/src/placement/PlacementWorker.cpp`
**Linee**: 300-462
**Tempo stimato**: 60 minuti

**Problema**: Il codice attuale calcola bounds DOPO tutti i posizionamenti, ma non traccia il valore "area" usato per selezionare ogni posizione.

**Soluzione**: Modificare PlacementStrategy per restituire l'area utilizzata nella selezione.

#### Step 1: Modifica PlacementStrategy Interface

**File**: `deepnest-cpp/include/deepnest/placement/PlacementStrategy.h`

```cpp
class PlacementStrategy {
public:
    struct PlacementResult {
        Point position;
        double area;           // NEW: Area metric used for selection
        double mergedLength;   // NEW: Merged line length (for future)
    };

    // Modifica firma del metodo
    virtual PlacementResult findBestPosition(
        const Polygon& part,
        const std::vector<Polygon>& placed,
        const std::vector<Point>& positions,
        const std::vector<Polygon>& nfps
    ) = 0;
};
```

#### Step 2: Implementa in GravityPlacement

**File**: `deepnest-cpp/src/placement/PlacementStrategy.cpp`

```cpp
PlacementStrategy::PlacementResult GravityPlacement::findBestPosition(
    const Polygon& part,
    const std::vector<Polygon>& placed,
    const std::vector<Point>& positions,
    const std::vector<Polygon>& nfps
) {
    double minArea = std::numeric_limits<double>::max();
    Point bestPosition;

    for (const auto& pos : positions) {
        // Translate part to candidate position
        Polygon testPart = part;
        for (auto& p : testPart.points) {
            p.x += pos.x;
            p.y += pos.y;
        }

        // Calculate bounding box including all placed parts + test part
        std::vector<Polygon> testPlacement = placed;
        testPlacement.push_back(testPart);

        BoundingBox bounds = GeometryUtil::getPolygonsBounds(testPlacement);

        // Gravity formula (matches JavaScript background.js:1060)
        double area = bounds.width * 2.0 + bounds.height;

        // TODO: Add merged line calculation here
        // if (config.mergeLines) {
        //     double mergedLen = MergeDetection::calculateMergedLength(...);
        //     area -= mergedLen * config.timeRatio;
        // }

        if (area < minArea) {
            minArea = area;
            bestPosition = pos;
        }
    }

    return PlacementResult{bestPosition, minArea, 0.0};
}
```

#### Step 3: Implementa in BoundingBoxPlacement

```cpp
PlacementStrategy::PlacementResult BoundingBoxPlacement::findBestPosition(...) {
    // Similar, but use:
    double area = bounds.width * bounds.height;  // Box mode (JS:1063)
}
```

#### Step 4: Implementa in ConvexHullPlacement

```cpp
PlacementStrategy::PlacementResult ConvexHullPlacement::findBestPosition(...) {
    // Calculate convex hull area
    std::vector<Point> allPoints;
    // Collect all points from testPlacement
    Polygon hull = ConvexHull::computeHull(allPoints);
    double area = std::abs(GeometryUtil::polygonArea(hull.points));
}
```

#### Step 5: Aggiorna PlacementWorker per usare minarea

**File**: `deepnest-cpp/src/placement/PlacementWorker.cpp`
**Linea**: ~410

```cpp
// OLD:
Point bestPos = strategy->findBestPosition(part, placed, positions, nfps);

// NEW:
PlacementStrategy::PlacementResult result = strategy->findBestPosition(part, placed, positions, nfps);
Point bestPos = result.position;

// Accumula minarea per questo sheet
double minarea_accumulator = 0.0;

// ... dopo ogni posizionamento:
minarea_accumulator += result.area;
```

#### Step 6: Aggiorna Formula Fitness Finale

**File**: `deepnest-cpp/src/placement/PlacementWorker.cpp`
**Linea**: ~461

```cpp
// OLD:
fitness += boundsWidth / sheetArea;

// NEW (matches JavaScript background.js:1142):
fitness += (boundsWidth / sheetArea) + minarea_accumulator;
```

---

## FASE 2: IMPLEMENTAZIONE LINE MERGE (CRITICO)

### 2.1 Implementa MergeDetection::calculateMergedLength

**File**: `deepnest-cpp/src/geometry/MergeDetection.cpp`
**Tempo stimato**: 2-3 ore

**Riferimento JavaScript**: `background.js:350-487` (funzione `mergedLength`)

```cpp
MergeDetection::MergeResult MergeDetection::calculateMergedLength(
    const std::vector<Polygon>& placed,
    const Polygon& newPart,
    double minLength,
    double tolerance
) {
    MergeResult result;
    result.totalLength = 0.0;

    // Estrai tutti i segmenti da placed e newPart
    std::vector<Segment> placedSegments = extractSegments(placed);
    std::vector<Segment> newSegments = extractSegments(newPart);

    // Per ogni segmento del nuovo pezzo
    for (const auto& seg1 : newSegments) {
        // Cerca segmenti coincidenti nei pezzi piazzati
        for (const auto& seg2 : placedSegments) {
            // Verifica se i segmenti sono paralleli e vicini
            if (areSegmentsParallel(seg1, seg2, tolerance) &&
                areSegmentsClose(seg1, seg2, tolerance)) {

                // Calcola sovrapposizione
                double overlap = calculateSegmentOverlap(seg1, seg2);

                if (overlap >= minLength) {
                    result.totalLength += overlap;
                    result.segments.push_back({seg1.p1, seg1.p2});
                }
            }
        }
    }

    return result;
}
```

**Helper methods necessari**:

```cpp
struct Segment {
    Point p1, p2;
    double length() const {
        double dx = p2.x - p1.x;
        double dy = p2.y - p1.y;
        return std::sqrt(dx*dx + dy*dy);
    }
    Point direction() const {
        double len = length();
        return Point{(p2.x - p1.x)/len, (p2.y - p1.y)/len};
    }
};

std::vector<Segment> extractSegments(const std::vector<Polygon>& polygons) {
    std::vector<Segment> segments;
    for (const auto& poly : polygons) {
        for (size_t i = 0; i < poly.points.size(); ++i) {
            size_t j = (i + 1) % poly.points.size();
            segments.push_back({poly.points[i], poly.points[j]});
        }
        // Gestisci anche i children (holes)
        for (const auto& child : poly.children) {
            for (size_t i = 0; i < child.points.size(); ++i) {
                size_t j = (i + 1) % child.points.size();
                segments.push_back({child.points[i], child.points[j]});
            }
        }
    }
    return segments;
}

bool areSegmentsParallel(const Segment& s1, const Segment& s2, double tolerance) {
    Point dir1 = s1.direction();
    Point dir2 = s2.direction();

    // Cross product should be near zero for parallel segments
    double cross = std::abs(dir1.x * dir2.y - dir1.y * dir2.x);
    return cross < tolerance;
}

bool areSegmentsClose(const Segment& s1, const Segment& s2, double tolerance) {
    // Check if segments are within tolerance distance
    double dist1 = GeometryUtil::pointToSegmentDistance(s1.p1, s2.p1, s2.p2);
    double dist2 = GeometryUtil::pointToSegmentDistance(s1.p2, s2.p1, s2.p2);
    return (dist1 < tolerance) && (dist2 < tolerance);
}

double calculateSegmentOverlap(const Segment& s1, const Segment& s2) {
    // Project segments onto common axis and calculate overlap
    // This is complex - see JavaScript implementation for details
    // Returns length of overlapping section

    // Simplified version:
    // 1. Project both segments onto common axis (direction of s1)
    // 2. Find overlap interval
    // 3. Return length of overlap

    // ... implementation omitted for brevity, see JS code ...
    return 0.0;  // TODO: Implement
}
```

### 2.2 Integra Line Merge in PlacementStrategy

**File**: `deepnest-cpp/src/placement/PlacementStrategy.cpp`

```cpp
PlacementStrategy::PlacementResult GravityPlacement::findBestPosition(...) {
    // ... calcolo area come prima ...

    // Add merged line bonus (matches JavaScript background.js:1094)
    double mergedLength = 0.0;
    if (config_.mergeLines) {
        double minLength = 0.1;  // Minimum merged line length to count
        double tolerance = 0.1 * config_.curveTolerance;

        auto mergeResult = MergeDetection::calculateMergedLength(
            placed, testPart, minLength, tolerance
        );

        mergedLength = mergeResult.totalLength;

        // Subtract from area (line merging reduces fitness = better)
        area -= mergedLength * config_.timeRatio;
    }

    return PlacementResult{bestPosition, area, mergedLength};
}
```

---

## FASE 3: MIGLIORAMENTI MINKOWSKI E NFP (ALTA PRIORITÀ)

### 3.1 Aggiungi Reference Point Shift in MinkowskiSum

**File**: `deepnest-cpp/src/nfp/MinkowskiSum.cpp`
**Linee**: 242-292
**Tempo stimato**: 30 minuti

**Problema**: L'output NFP dovrebbe essere riferito al primo punto del poligono B, come nel JavaScript.

```cpp
Polygon MinkowskiSum::calculateNFP(const Polygon& A, const Polygon& B, bool inner) {
    // ... codice esistente per negazione e scaling ...

    // NEW: Store reference point (first point of B)
    double xshift = 0.0;
    double yshift = 0.0;
    if (!B_to_use.points.empty()) {
        xshift = B.points[0].x;  // Use ORIGINAL B, not negated
        yshift = B.points[0].y;
    }

    // ... esegui convolution ...

    // NEW: Apply shift to output
    for (auto& polygon : nfps) {
        for (auto& point : polygon.points) {
            point.x += xshift;
            point.y += yshift;
        }
        for (auto& child : polygon.children) {
            for (auto& point : child.points) {
                point.x += xshift;
                point.y += yshift;
            }
        }
    }

    return nfps[0];  // Return largest NFP
}
```

### 3.2 Implementa Orbital-Based noFitPolygon (Fallback)

**File**: `deepnest-cpp/src/geometry/GeometryUtil.cpp`
**Linee**: 556-564 (attualmente stub)
**Tempo stimato**: 4-6 ore

**Riferimento JavaScript**: `geometryutil.js:1437-1727`

Questa è un'implementazione complessa. Ecco la struttura:

```cpp
std::vector<Polygon> GeometryUtil::noFitPolygon(
    const Polygon& A,
    const Polygon& B,
    bool inside,
    bool searchEdges
) {
    std::vector<Polygon> nfps;

    // 1. Find start point
    Point startPoint = searchStartPoint(A, B, inside, Polygon());

    // 2. Initialize NFP with start point
    Polygon nfp;
    nfp.points.push_back(startPoint);

    // 3. Track reference and previous vectors
    Point reference = startPoint;
    std::vector<double> prevVector = {0, 0};

    // 4. Main loop: trace B around A
    int maxIterations = 10 * (A.points.size() + B.points.size());
    for (int i = 0; i < maxIterations; ++i) {
        // 4a. Get touching edges
        struct TouchingEdge {
            int edgeIndex;
            char polygon;  // 'A' or 'B'
            double angle;
        };
        std::vector<TouchingEdge> touching = findTouchingEdges(A, B, reference);

        if (touching.empty()) {
            break;  // No more touching edges
        }

        // 4b. Select next edge based on angle
        TouchingEdge nextEdge = selectNextEdge(touching, prevVector);

        // 4c. Calculate translation vector
        Point translateVector = calculateTranslationVector(
            A, B, reference, nextEdge
        );

        // 4d. Calculate slide distance
        double distance = polygonSlideDistance(
            A, B, translateVector, !inside
        );

        // 4e. Update reference point
        reference.x += translateVector.x * distance;
        reference.y += translateVector.y * distance;

        // 4f. Add point to NFP
        nfp.points.push_back(reference);

        // 4g. Check if we've returned to start
        if (almostEqual(reference.x, startPoint.x) &&
            almostEqual(reference.y, startPoint.y)) {
            break;
        }

        prevVector = {translateVector.x, translateVector.y};
    }

    // 5. Clean up NFP
    if (!nfp.points.empty()) {
        nfp = cleanNFP(nfp);
        nfps.push_back(nfp);
    }

    // 6. If searchEdges, repeat for all starting edges
    if (searchEdges) {
        // ... explore all edges of A as starting points ...
    }

    return nfps;
}
```

**Note**: Questa è una versione semplificata. L'implementazione completa richiede:
- `findTouchingEdges()`: Trova vertici di A e B che si toccano
- `selectNextEdge()`: Seleziona il prossimo edge basato sull'angolo
- `calculateTranslationVector()`: Calcola il vettore di traslazione
- `cleanNFP()`: Rimuove punti duplicati e quasi-colineari

**Priorità**: Medio-Alta. Implementare solo se Minkowski produce risultati incorretti in testing.

---

## FASE 4: TESTING E VALIDAZIONE

### 4.1 Unit Test per Fitness Components

**File**: `deepnest-cpp/tests/FitnessTests.cpp` (nuovo)

```cpp
TEST(FitnessTest, SheetAreaPenalty) {
    // Create a sheet 500x300 = 150,000 units²
    Polygon sheet({{0,0}, {500,0}, {500,300}, {0,300}});
    double sheetArea = std::abs(GeometryUtil::polygonArea(sheet.points));

    EXPECT_NEAR(sheetArea, 150000.0, 1.0);

    // Fitness should add sheet area, not 1.0
    double fitness = sheetArea;
    EXPECT_GT(fitness, 1000.0);  // Should be much larger than 1.0
}

TEST(FitnessTest, UnplacedPartsPenalty) {
    Polygon part({{0,0}, {100,0}, {100,50}, {0,50}});
    double partArea = std::abs(GeometryUtil::polygonArea(part.points));
    double totalSheetArea = 150000.0;

    double penalty = 100000000.0 * (partArea / totalSheetArea);

    EXPECT_NEAR(penalty, 3333333.33, 1.0);
    EXPECT_GT(penalty, 1000000.0);  // Should be massive
}

TEST(FitnessTest, MinariaComponent) {
    // Test gravity mode
    BoundingBox bounds{0, 0, 200, 100};
    double minarea_gravity = bounds.width * 2.0 + bounds.height;
    EXPECT_NEAR(minarea_gravity, 500.0, 0.1);

    // Test box mode
    double minarea_box = bounds.width * bounds.height;
    EXPECT_NEAR(minarea_box, 20000.0, 0.1);
}
```

### 4.2 Integration Test per GA Evolution

```cpp
TEST(GeneticAlgorithmTest, FitnessVariation) {
    // Create simple test case
    std::vector<Polygon> parts = createTestParts(5);
    std::vector<Polygon> sheets = createTestSheets(1);

    GeneticAlgorithm ga(parts, sheets, config);
    ga.generation();  // Run one generation

    auto pop = ga.getPopulation();
    auto individuals = pop.getIndividuals();

    // Check that fitness values vary significantly
    double minFitness = individuals[0].fitness;
    double maxFitness = individuals[0].fitness;

    for (const auto& ind : individuals) {
        minFitness = std::min(minFitness, ind.fitness);
        maxFitness = std::max(maxFitness, ind.fitness);
    }

    // Fitness should vary by at least 10%
    double variation = (maxFitness - minFitness) / minFitness;
    EXPECT_GT(variation, 0.1);

    // Fitness should be greater than 1000 (not ~1.0)
    EXPECT_GT(minFitness, 1000.0);
}

TEST(GeneticAlgorithmTest, EvolutionImprovement) {
    // Run GA for 10 generations
    GeneticAlgorithm ga(parts, sheets, config);

    double initialBestFitness = ga.getBestIndividual().fitness;

    for (int i = 0; i < 10; ++i) {
        ga.generation();
    }

    double finalBestFitness = ga.getBestIndividual().fitness;

    // Fitness should improve (decrease) over generations
    EXPECT_LT(finalBestFitness, initialBestFitness);

    // Improvement should be at least 5%
    double improvement = (initialBestFitness - finalBestFitness) / initialBestFitness;
    EXPECT_GT(improvement, 0.05);
}
```

### 4.3 Comparison Test: JavaScript vs C++

**File**: `deepnest-cpp/tests/JSComparisonTests.cpp` (nuovo)

```cpp
TEST(JSComparison, FitnessFormula) {
    // Create identical scenario to JavaScript test
    Polygon sheet({{0,0}, {500,0}, {500,300}, {0,300}});
    double sheetArea = 150000.0;

    Polygon part1({{0,0}, {100,0}, {100,50}, {0,50}});
    Polygon part2({{0,0}, {80,0}, {80,60}, {0,60}});

    // Simulate fitness calculation
    double fitness = 0.0;

    // Sheet penalty
    fitness += sheetArea;  // 150,000

    // Minarea component (gravity mode)
    BoundingBox bounds{0, 0, 180, 110};
    double minarea = bounds.width * 2.0 + bounds.height;  // 470
    fitness += (bounds.width / sheetArea) + minarea;  // 0.0012 + 470

    // Unplaced part penalty (assume 1 part unplaced)
    double unplacedArea = 5000.0;
    fitness += 100000000.0 * (unplacedArea / sheetArea);  // 3,333,333

    // Expected total: ~3,483,803
    EXPECT_NEAR(fitness, 3483803.0, 100.0);

    // This matches JavaScript behavior
}

TEST(JSComparison, MinkowskiSum) {
    // Load same polygons used in JavaScript tests
    Polygon A = loadFromFile("testdata/polygon_A.json");
    Polygon B = loadFromFile("testdata/polygon_B.json");

    // Calculate NFP
    Polygon nfp_cpp = MinkowskiSum::calculateNFP(A, B);

    // Load JavaScript result
    Polygon nfp_js = loadFromFile("testdata/nfp_expected.json");

    // Compare results (within tolerance)
    EXPECT_TRUE(polygonsAlmostEqual(nfp_cpp, nfp_js, 0.01));
}
```

---

## FASE 5: OTTIMIZZAZIONI E RAFFINAMENTI (BASSA PRIORITÀ)

### 5.1 Aggiungi searchEdges Support in NFPCalculator

**Tempo stimato**: 1-2 ore

Permette di esplorare tutte le regioni NFP possibili, non solo la più grande.

### 5.2 Implementa NFP Batch Processing Parallelo

**Tempo stimato**: 2-3 ore

Usa `MinkowskiSum::calculateNFPBatch()` per calcolare più NFP in parallelo.

### 5.3 Aggiungi Caching su Disco per NFP

**Tempo stimato**: 2-3 ore

Implementa serializzazione NFP su file per persistenza tra sessioni.

---

## PIANO DI IMPLEMENTAZIONE PER PRIORITÀ

### Settimana 1: FIX CRITICI (Obbligatori)
1. **Giorno 1**: Fix 1.1 + 1.2 (Sheet penalty + Unplaced penalty) - **30 min**
2. **Giorno 2**: Fix 1.3 Step 1-3 (PlacementStrategy interface + Gravity) - **2 ore**
3. **Giorno 3**: Fix 1.3 Step 4-6 (Box, ConvexHull, PlacementWorker) - **2 ore**
4. **Giorno 4**: Test Phase 4.1 (Unit tests per fitness) - **1 ora**
5. **Giorno 5**: Test Phase 4.2 (Integration tests GA) - **2 ore**

**Risultato atteso Settimana 1**: Fitness funzionante, GA evolve correttamente

### Settimana 2: LINE MERGE (Critico per qualità risultati)
1. **Giorno 1-2**: Implementa 2.1 (MergeDetection core) - **4 ore**
2. **Giorno 3**: Implementa 2.2 (Integrazione in PlacementStrategy) - **2 ore**
3. **Giorno 4**: Test line merge con casi reali - **2 ore**
4. **Giorno 5**: Tuning e ottimizzazione - **2 ore**

**Risultato atteso Settimana 2**: Line merge funzionante, bonus per linee coincidenti

### Settimana 3: MINKOWSKI E NFP (Alta priorità)
1. **Giorno 1**: Implementa 3.1 (Reference point shift) - **30 min**
2. **Giorno 2-3**: Test Minkowski con casi JavaScript - **3 ore**
3. **Giorno 4**: Implementa 3.2 parziale (orbital NFP framework) - **2 ore**
4. **Giorno 5**: Test e confronto NFP risultati - **2 ore**

**Risultato atteso Settimana 3**: Minkowski corretto, base per orbital NFP

### Settimana 4: VALIDAZIONE FINALE
1. **Giorno 1-2**: Test 4.3 (Comparison JS vs C++) - **4 ore**
2. **Giorno 3**: Test end-to-end con progetti reali - **2 ore**
3. **Giorno 4**: Bug fixes e raffinamenti - **2 ore**
4. **Giorno 5**: Documentazione e benchmark - **2 ore**

**Risultato atteso Settimana 4**: Implementazione validata e performante

---

## METRICHE DI SUCCESSO

### Fitness Values
- ✅ Fitness non più costante a 1.0
- ✅ Fitness varia significativamente tra individui (>10% variation)
- ✅ Fitness su scala corretta (migliaia o milioni, non ~1)

### Genetic Algorithm Evolution
- ✅ Best fitness migliora nel tempo (almeno 5% in 10 generazioni)
- ✅ Population diversity mantenuta
- ✅ Convergenza verso soluzioni ottimali

### Placement Quality
- ✅ Parti piazzate in modo compatto
- ✅ Line merge bonus applicato correttamente
- ✅ Orientazioni esplorate efficacemente

### Correctness vs JavaScript
- ✅ Fitness formula matematicamente identica
- ✅ NFP risultati entro 1% di tolleranza
- ✅ Placement simile o migliore del JavaScript

---

## RISORSE NECESSARIE

### Conoscenze Richieste
- C++17 (template, lambda, smart pointers)
- Algoritmi geometrici
- Boost.Polygon API
- Algoritmi genetici

### Tools
- Compilatore C++17 (GCC 7+, Clang 5+, MSVC 2017+)
- Qt 5+ per testing GUI
- Boost libraries
- CMake o qmake

### Dati di Test
- File SVG di test dal repository originale
- Dataset JavaScript con risultati noti
- Benchmark cases per confronto prestazioni

---

## NOTE IMPLEMENTATIVE

### Debugging Fitness
Aggiungi logging dettagliato:

```cpp
std::cout << "=== FITNESS BREAKDOWN ===" << std::endl;
std::cout << "Sheets used: " << sheetsUsed << std::endl;
std::cout << "Sheet area penalty: " << (sheetsUsed * avgSheetArea) << std::endl;
std::cout << "Bounds component: " << (boundsWidth / sheetArea) << std::endl;
std::cout << "Minarea component: " << minarea_accumulator << std::endl;
std::cout << "Unplaced penalty: " << unplacedPenalty << std::endl;
std::cout << "TOTAL FITNESS: " << fitness << std::endl;
```

### Performance Considerations
- Line merge è O(n²) sui segmenti - ottimizza con spatial partitioning se lento
- Minkowski sum è computazionalmente costoso - usa cache aggressivamente
- PlacementStrategy valuta molte posizioni - parallelizza se possibile

### Backward Compatibility
- Mantieni vecchio codice in `#ifdef LEGACY_FITNESS` per A/B testing
- Aggiungi flag configurazione per abilitare/disabilitare line merge
- Versiona il formato cache NFP

---

## CONCLUSIONI

L'implementazione C++ di DeepNest è **strutturalmente solida** ma presenta **bugs critici nella formula di fitness** che impediscono all'algoritmo genetico di funzionare correttamente.

I fix proposti sono:
- ✅ **Fattibili** - Modifiche localizzate, no refactoring massiccio
- ✅ **Testabili** - Ogni fix ha test di verifica chiari
- ✅ **Incrementali** - Possono essere applicati uno alla volta
- ✅ **Ad alto impatto** - Risolvono il problema principale (fitness = 1.0)

Con l'implementazione di FASE 1 (1-2 giorni di lavoro), l'algoritmo genetico dovrebbe iniziare a funzionare correttamente.

Con l'implementazione completa di FASE 1-3 (3 settimane), l'implementazione C++ sarà **equivalente al JavaScript** in termini di correttezza e qualità dei risultati.
