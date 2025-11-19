# Analisi e Piano di Implementazione - DeepNest C++

## ANALISI DEI PROBLEMI IDENTIFICATI

### 1. PROBLEMA CRITICO: Calcolo del Fitness Incompleto

**Problema**: Il valore del fitness è sempre costante a 1.0, impedendo all'algoritmo genetico di funzionare correttamente.

**Causa Root**: Nel file `PlacementWorker.cpp`, il calcolo del fitness è **gravemente incompleto** rispetto all'implementazione JavaScript originale.

#### Codice JavaScript Originale (background.js, linee 832-1167):
```javascript
var fitness = 0;

// Per ogni sheet:
fitness += sheetarea;  // Linea 848

// Dopo aver piazzato le parti in uno sheet:
fitness += (minwidth/sheetarea) + minarea;  // Linea 1142

// Per parti non piazzate:
fitness += 100000000*(Math.abs(GeometryUtil.polygonArea(parts[i]))/totalsheetarea);  // Linea 1167
```

#### Codice C++ Attuale (PlacementWorker.cpp, linee 58-358):
```cpp
double fitness = 0.0;

// Per ogni sheet:
fitness += sheetArea;  // Linea 77

// ❌ MANCANTE: fitness += (minwidth/sheetarea) + minarea;

// Per parti non piazzate:
fitness += 2.0 * parts.size();  // Linea 358
// ❌ SBAGLIATO: dovrebbe essere 100000000*(area/totalsheetarea)
```

**Impatto**:
- L'algoritmo genetico non può distinguere tra soluzioni buone e cattive
- Tutte le soluzioni hanno fitness praticamente identico
- L'evoluzione della popolazione è casuale invece che guidata dalla qualità

---

### 2. PROBLEMA: Missing Merged Lines Calculation

**Problema**: Il calcolo della lunghezza totale delle linee mergeable non è implementato.

**File Affetto**: `PlacementWorker.cpp`, funzione `calculateTotalMergedLength()` (linee 412-435)

#### Implementazione Attuale:
```cpp
double PlacementWorker::calculateTotalMergedLength(...) {
    double totalMerged = 0.0;
    // For now, return 0 - this would be expanded in full implementation
    return totalMerged;  // ❌ Sempre 0!
}
```

#### Implementazione JavaScript (background.js, linee 1082-1094):
```javascript
if(config.mergeLines){
    var merged = mergedLength(shiftedplaced, shiftedpart, minlength, 0.1*config.curveTolerance);
    area -= merged.totalLength*config.timeRatio;  // Sottrae dai calcoli di fitness!
}
```

**Impatto**:
- L'ottimizzazione delle linee merge non funziona
- Il fitness non considera il risparmio di tempo di taglio
- `config.mergeLines` è ignorato completamente

---

### 3. PROBLEMA: Missing minwidth/minarea Tracking

**Problema**: Le variabili `minwidth` e `minarea` non sono tracciate durante il placement.

**File Affetto**: `PlacementWorker.cpp`, funzione `placeParts()`

#### Codice JavaScript (background.js, linee 997-1142):
```javascript
var minwidth = null;
var minarea = null;

// Durante la valutazione delle posizioni:
for(j=0; j<finalNfp.length; j++){
    for(k=0; k<nf.length; k++){
        // Calcola area per ogni posizione candidata
        area = rectbounds.width*2 + rectbounds.height;  // gravity
        // oppure
        area = rectbounds.width * rectbounds.height;    // box

        if(minarea === null || area < minarea || ...){
            minarea = area;
            minwidth = rectbounds.width;
            position = shiftvector;
        }
    }
}

// Aggiunge al fitness:
fitness += (minwidth/sheetarea) + minarea;
```

#### Codice C++ Attuale:
```cpp
// ❌ Non traccia minwidth e minarea!
// ❌ Non le aggiunge al fitness!
```

**Impatto**:
- Il fitness non riflette la compattezza del layout
- Manca il componente principale del fitness che guida l'ottimizzazione

---

### 4. PROBLEMA: Penalty per Parti Non Piazzate Sbagliata

**Problema**: La penalty per parti non piazzate è troppo bassa e calcolata in modo errato.

#### JavaScript (background.js, linea 1167):
```javascript
fitness += 100000000 * (Math.abs(GeometryUtil.polygonArea(parts[i])) / totalsheetarea);
```

#### C++ Attuale (PlacementWorker.cpp, linea 358):
```cpp
fitness += 2.0 * parts.size();  // ❌ Troppo basso! Dovrebbe essere 100000000 * area!
```

**Impatto**:
- L'algoritmo non è sufficientemente penalizzato per parti non piazzate
- Potrebbe preferire soluzioni che lasciano parti fuori

---

### 5. PROBLEMA: Candidate Position Calculation Semplificata

**Problema**: La funzione `extractCandidatePositions()` estrae solo i punti NFP grezzi senza sottrazione del primo punto della parte.

#### JavaScript (placementworker.js, linee 235-257):
```javascript
shiftvector = {
    x: nf[k].x - part[0].x,  // Sottrae il primo punto!
    y: nf[k].y - part[0].y,
    id: part.id,
    rotation: part.rotation
};
```

#### C++ Attuale (PlacementWorker.cpp, linee 391-394):
```cpp
for (const auto& point : nfp.points) {
    positions.push_back(point);  // ❌ Non sottrae part[0]!
}
```

**Impatto**:
- Le posizioni candidate potrebbero non essere corrette
- Il placement potrebbe non allinearsi correttamente

---

### 6. PROBLEMA: NFP Calculation - Possibili Discrepanze

**Area da Verificare**: `MinkowskiSum.cpp` e `NFPCalculator.cpp`

**Verifiche Necessarie**:

1. **Scale Factor Calculation** (MinkowskiSum.cpp, linee 122-157)
   - Verifica che il fattore di scala sia identico al JavaScript
   - JavaScript usa `addon.nfp` (C++ binding da minkowski.cc)

2. **NFP Translation** (NFPCalculator.cpp, linee 28-32)
   - JavaScript: `clipperNfp[i].x += B[0].x; clipperNfp[i].y += B[0].y;`
   - C++: `result = result.translate(B.points[0].x, B.points[0].y);`
   - ✅ Sembra corretto

3. **Inner NFP Calculation** (NFPCalculator.cpp, linee 118-178)
   - Verifica che il frame, i children, e le operazioni di differenza siano identiche

---

### 7. PROBLEMA: Polygon Operations (Clipper2)

**Area da Verificare**: `PolygonOperations.cpp`

**Verifiche Necessarie**:
1. Union operation - identica a ClipperLib.ClipType.ctUnion?
2. Difference operation - identica a ClipperLib.ClipType.ctDifference?
3. Scaling factor - usa `config.clipperScale = 10000000`?
4. Clean polygon - usa stessa tolleranza?

---

### 8. PROBLEMA: Placement Strategy Metric Calculation

**File**: `PlacementStrategy.cpp`

**Verifica**: La metrica calcolata in `calculateMetric()` deve includere tutte le parti già piazzate.

#### Potenziale Problema (PlacementStrategy.cpp, linee 83-85):
```cpp
BoundingBox allBounds = BoundingBox::fromPoints(allPoints);
```

Se `placed` è vuoto, `allPoints` è vuoto → `fromPoints` potrebbe fallire!

**Soluzione Necessaria**: Gestire il caso in cui `placed` è vuoto.

---

### 9. PROBLEMA: Genetic Algorithm - Individual Selection

**File**: `Population.cpp`, funzione `selectWeightedRandom()`

**Verifica**: La selezione pesata è identica al JavaScript?

#### JavaScript (deepnest.js):
```javascript
for(var i=0; i<pop.length; i++){
    if(rand > lower && rand < upper){
        return pop[i];
    }
    lower = upper;
    upper += 2*weight * ((pop.length-i)/pop.length);
}
```

#### C++ (Population.cpp, linee 126-132):
```cpp
for (size_t i = 0; i < pop.size(); ++i) {
    if (rand > lower && rand < upper) {
        return pop[i];
    }
    lower = upper;
    upper += 2.0 * weight * (static_cast<double>(pop.size() - i) / pop.size());
}
```

✅ Sembra corretto, ma **IMPORTANTE**: La popolazione deve essere **ordinata per fitness** prima della selezione!

---

### 10. PROBLEMA: Missing Sort Before Selection

**File**: `Population.cpp`, `GeneticAlgorithm.cpp`

**Problema Potenziale**: La selezione pesata assume che la popolazione sia ordinata, ma potrebbe non esserlo sempre.

**Verifica Necessaria**:
- `nextGeneration()` ordina (linea 145) ✅
- Ma la selezione avviene sulla popolazione **prima** dell'ordinamento?

---

## PRIORITÀ DI IMPLEMENTAZIONE

### 🔴 CRITICO (Blocca l'algoritmo):
1. **Fix Fitness Calculation** - PlacementWorker.cpp
2. **Track minwidth/minarea** - PlacementWorker.cpp
3. **Fix Unplaced Parts Penalty** - PlacementWorker.cpp

### 🟡 ALTO (Degrada le performance):
4. **Implement Merged Lines Calculation** - MergeDetection.cpp → PlacementWorker.cpp
5. **Fix Candidate Position Extraction** - PlacementWorker.cpp
6. **Verify NFP Calculation** - MinkowskiSum.cpp, NFPCalculator.cpp

### 🟢 MEDIO (Completezza):
7. **Verify Polygon Operations** - PolygonOperations.cpp
8. **Fix Placement Strategy Edge Cases** - PlacementStrategy.cpp
9. **Verify GA Selection** - Population.cpp

---

## PIANO DI IMPLEMENTAZIONE DETTAGLIATO

### FASE 1: Fix Fitness Calculation (CRITICO)

**File**: `deepnest-cpp/src/placement/PlacementWorker.cpp`

#### Step 1.1: Aggiungere tracking di minwidth e minarea

**Posizione**: Dentro il loop di placement (dopo linea 310)

```cpp
// Dopo extractCandidatePositions (linea 311):
std::vector<Point> candidatePositions = extractCandidatePositions(finalNfp);

// AGGIUNGERE:
double minwidth = 0.0;
double minarea = std::numeric_limits<double>::max();
double minx = std::numeric_limits<double>::max();
double miny = std::numeric_limits<double>::max();
```

#### Step 1.2: Tracciare minwidth/minarea durante la valutazione

**Posizione**: Dentro findBestPosition (modificare PlacementStrategy::calculateMetric)

Dobbiamo restituire ANCHE la larghezza e l'area, non solo la metrica!

**Nuovo Approccio**: Modificare la signature di `findBestPosition` per restituire anche questi valori.

```cpp
// In PlacementStrategy.h:
struct PlacementMetrics {
    Point position;
    double metric;
    double width;
    double area;
};

virtual PlacementMetrics findBestPositionWithMetrics(
    const Polygon& part,
    const std::vector<PlacedPart>& placed,
    const std::vector<Point>& candidatePositions
) const;
```

#### Step 1.3: Aggiungere minwidth e minarea al fitness

**Posizione**: Dopo il loop di placement, prima di linea 358

```cpp
// Dopo l'ultimo }  del loop for(size_t i = 0; i < parts.size(); )
// PRIMA di: fitness += 2.0 * parts.size();

// AGGIUNGERE:
if (minwidth > 0.0 && sheetArea > 0.0) {
    fitness += (minwidth / sheetArea) + minarea;
}
```

#### Step 1.4: Fix penalty per parti non piazzate

**Posizione**: Linea 358

```cpp
// SOSTITUIRE:
// fitness += 2.0 * parts.size();

// CON:
for (const auto& unplacedPart : parts) {
    double partArea = std::abs(GeometryUtil::polygonArea(unplacedPart.points));
    if (totalSheetArea > 0.0) {
        fitness += 100000000.0 * (partArea / totalSheetArea);
    }
}
```

---

### FASE 2: Implement Merged Lines Calculation

**File**: `deepnest-cpp/src/placement/MergeDetection.cpp`

**Verifica**: Il file esiste ma potrebbe non essere completamente implementato.

#### Step 2.1: Leggere l'implementazione JavaScript

**File JavaScript**: `background.js`, funzione `mergedLength` (cercare "function mergedLength")

#### Step 2.2: Implementare `calculateMergedLength` in MergeDetection.cpp

**Algoritmo** (da background.js):
```javascript
function mergedLength(placed, part, minLength, tolerance) {
    var i, j, k;
    var segments = [];
    var totalLength = 0;

    // Per ogni segmento della parte:
    for(i=0; i<part.length; i++) {
        var A1 = part[i];
        var A2 = part[(i+1) % part.length];
        var Alength = Math.sqrt((A2.x-A1.x)*(A2.x-A1.x) + (A2.y-A1.y)*(A2.y-A1.y));

        // Skip segmenti corti
        if(Alength < minLength) continue;

        // Per ogni parte già piazzata:
        for(j=0; j<placed.length; j++) {
            // Per ogni segmento della parte piazzata:
            for(k=0; k<placed[j].length; k++) {
                var B1 = placed[j][k];
                var B2 = placed[j][(k+1) % placed[j].length];
                var Blength = Math.sqrt((B2.x-B1.x)*(B2.x-B1.x) + (B2.y-B1.y)*(B2.y-B1.y));

                if(Blength < minLength) continue;

                // Controlla se i segmenti sono allineati e adiacenti
                // ...logica di merge detection...
            }
        }
    }

    return {totalLength: totalLength, segments: segments};
}
```

#### Step 2.3: Integrare in PlacementWorker

**Posizione**: Durante la valutazione delle posizioni candidate (dopo calcolo area)

```cpp
// Dopo il calcolo di metric in findBestPosition:
double metric = calculateMetric(part, position, placed);

// AGGIUNGERE (se config.mergeLines è true):
if (config_.mergeLines) {
    // Trasforma la parte alla posizione candidata
    Polygon shiftedPart = part.translate(position.x, position.y);

    // Trasforma le parti piazzate
    std::vector<Polygon> shiftedPlaced;
    for (size_t j = 0; j < placed.size(); j++) {
        shiftedPlaced.push_back(
            placed[j].polygon.translate(placed[j].position.x, placed[j].position.y)
        );
    }

    // Calcola merged length
    double minLength = 0.5 * config_.scale;
    double tolerance = 0.1 * config_.curveTolerance;
    auto mergeResult = MergeDetection::calculateMergedLength(
        shiftedPlaced, shiftedPart, minLength, tolerance
    );

    // Sottrai dal metric
    metric -= mergeResult.totalLength * config_.timeRatio;
}
```

---

### FASE 3: Fix Candidate Position Extraction

**File**: `deepnest-cpp/src/placement/PlacementWorker.cpp`

**Funzione**: `extractCandidatePositions` (linea 373)

#### Step 3.1: Modificare per ricevere il poligono parte

```cpp
// CAMBIARE signature:
std::vector<Point> PlacementWorker::extractCandidatePositions(
    const std::vector<Polygon>& finalNfp,
    const Polygon& part  // AGGIUNGERE parametro
) const {
```

#### Step 3.2: Sottrarre part[0] da ogni punto

```cpp
std::vector<Point> positions;

for (const auto& nfp : finalNfp) {
    if (std::abs(GeometryUtil::polygonArea(nfp.points)) < 2.0) {
        continue;
    }

    for (const auto& nfpPoint : nfp.points) {
        // MODIFICARE: sottrarre part[0]
        if (!part.points.empty()) {
            Point position(
                nfpPoint.x - part.points[0].x,
                nfpPoint.y - part.points[0].y
            );
            positions.push_back(position);
        }
    }
}

return positions;
```

#### Step 3.3: Aggiornare la chiamata (linea 311)

```cpp
// CAMBIARE:
// std::vector<Point> candidatePositions = extractCandidatePositions(finalNfp);

// IN:
std::vector<Point> candidatePositions = extractCandidatePositions(finalNfp, part);
```

---

### FASE 4: Verify and Fix NFP Calculation

**File**: `deepnest-cpp/src/nfp/MinkowskiSum.cpp`

#### Step 4.1: Verificare Scale Factor

**Confrontare**:
- JavaScript: minkowski.cc (addon nativo)
- C++: MinkowskiSum::calculateScale() (linea 122)

**Test**: Calcolare NFP per gli stessi poligoni e confrontare risultati.

#### Step 4.2: Verificare Minkowski Convolution

**File**: MinkowskiSum.cpp, linee 23-116

**Verifica**: Algoritmo identico a Boost.Polygon e minkowski.cc originale?

#### Step 4.3: Verificare Inner NFP

**File**: NFPCalculator.cpp, `getInnerNFP()` (linea 118)

**Punti Critici**:
1. Frame creation (linea 127)
2. Frame NFP calculation con `inside=true` (linea 130)
3. Extraction dei children (linea 141)
4. Handling delle holes (linee 144-170)

---

### FASE 5: Verify Polygon Operations

**File**: `deepnest-cpp/src/geometry/PolygonOperations.cpp`

#### Step 5.1: Verificare che usi Clipper2 correttamente

**Verifiche**:
1. Scale factor = 10000000 (come config.clipperScale)
2. Union con PolyFillType corretto
3. Difference con PolyFillType corretto
4. Clean con tolleranza corretta

#### Step 5.2: Aggiungere logging/debug per confrontare

```cpp
// Aggiungere macro di debug:
#define DEBUG_POLYGON_OPS 1

#ifdef DEBUG_POLYGON_OPS
    std::cout << "Union input polygons: " << polygons.size() << std::endl;
    std::cout << "Union result polygons: " << result.size() << std::endl;
#endif
```

---

### FASE 6: Fix Placement Strategy Edge Cases

**File**: `deepnest-cpp/src/placement/PlacementStrategy.cpp`

#### Step 6.1: Gestire caso placed vuoto

**Funzioni**: `GravityPlacement::calculateMetric`, etc.

```cpp
// All'inizio di calculateMetric:
if (placed.empty()) {
    // Solo i bounds della parte
    BoundingBox partBounds = part.bounds();

    if (strategy == GRAVITY) {
        return (partBounds.width + position.x) * 2.0 + (partBounds.height + position.y);
    } else if (strategy == BOUNDING_BOX) {
        return (partBounds.width + position.x) * (partBounds.height + position.y);
    }
    // ...
}
```

---

### FASE 7: Verify Genetic Algorithm

**File**: `deepnest-cpp/src/algorithm/Population.cpp`

#### Step 7.1: Verificare che la popolazione sia ordinata prima della selezione

**In `selectWeightedRandom()`**:

```cpp
Individual Population::selectWeightedRandom(const Individual* exclude) {
    if (individuals_.empty()) {
        throw std::runtime_error("Cannot select from empty population");
    }

    // AGGIUNGERE: Sort se non già ordinata
    // OPPURE: assicurarsi che sia sempre ordinata

    // La selezione assume popolazione ordinata per fitness!
    // Verificare che nextGeneration() chiami sortByFitness() prima!
}
```

#### Step 7.2: Verificare Crossover

**Funzione**: `Population::crossover()`

**Verifica**: Identico al JavaScript?

---

### FASE 8: Integration Testing

#### Step 8.1: Creare test con forma semplice

**Test**: 10 rettangoli identici in un contenitore

**Verifiche**:
- Fitness diminuisce nelle generazioni successive
- Le soluzioni migliorano visibilmente
- I rettangoli sono compattati

#### Step 8.2: Confrontare con JavaScript

**Test**: Stessa configurazione, stesso seed random

**Confronto**:
- Fitness values
- Placement positions
- NFP results

---

## PIANO DI TEST DETTAGLIATO

### TEST SUITE 1: Unit Tests per Componenti Individuali

#### Test 1.1: GeometryUtil Tests
```cpp
TEST(GeometryUtil, PolygonArea) {
    // Test con poligoni noti
    Polygon square = {{0,0}, {10,0}, {10,10}, {0,10}};
    EXPECT_DOUBLE_EQ(GeometryUtil::polygonArea(square.points), 100.0);
}

TEST(GeometryUtil, PointInPolygon) {
    Polygon square = {{0,0}, {10,0}, {10,10}, {0,10}};
    EXPECT_TRUE(GeometryUtil::pointInPolygon({5,5}, square.points));
    EXPECT_FALSE(GeometryUtil::pointInPolygon({15,5}, square.points));
}
```

#### Test 1.2: MinkowskiSum Tests
```cpp
TEST(MinkowskiSum, SimpleRectangles) {
    // Due rettangoli semplici
    Polygon A = {{0,0}, {10,0}, {10,5}, {0,5}};
    Polygon B = {{0,0}, {5,0}, {5,3}, {0,3}};

    auto nfps = MinkowskiSum::calculateNFP(A, B, false);

    EXPECT_GT(nfps.size(), 0);
    // Verificare area e forma del NFP
}

TEST(MinkowskiSum, CompareWithJavaScript) {
    // Caricare risultati da file JSON generato da JS
    // Confrontare con risultati C++
}
```

#### Test 1.3: NFPCalculator Tests
```cpp
TEST(NFPCalculator, OuterNFP) {
    NFPCache cache;
    NFPCalculator calc(cache);

    Polygon A = createTestRectangle(10, 5);
    Polygon B = createTestRectangle(5, 3);

    Polygon nfp = calc.getOuterNFP(A, B, false);

    EXPECT_FALSE(nfp.points.empty());
}

TEST(NFPCalculator, InnerNFP) {
    NFPCache cache;
    NFPCalculator calc(cache);

    Polygon sheet = createTestRectangle(100, 100);
    Polygon part = createTestRectangle(10, 10);

    auto innerNfps = calc.getInnerNFP(sheet, part);

    EXPECT_GT(innerNfps.size(), 0);
}

TEST(NFPCalculator, CacheHit) {
    NFPCache cache;
    NFPCalculator calc(cache);

    Polygon A = createTestRectangle(10, 5);
    Polygon B = createTestRectangle(5, 3);

    // Prima chiamata
    calc.getOuterNFP(A, B, false);

    // Seconda chiamata - dovrebbe usare cache
    auto stats = calc.getCacheStats();
    calc.getOuterNFP(A, B, false);
    auto stats2 = calc.getCacheStats();

    EXPECT_GT(std::get<0>(stats2), std::get<0>(stats)); // hit count aumentato
}
```

#### Test 1.4: PlacementStrategy Tests
```cpp
TEST(PlacementStrategy, GravityMetric) {
    auto strategy = PlacementStrategy::create("gravity");

    Polygon part = createTestRectangle(10, 5);
    std::vector<PlacedPart> placed; // vuoto
    std::vector<Point> candidates = {{0, 0}, {10, 0}, {0, 10}};

    Point best = strategy->findBestPosition(part, placed, candidates);

    // Dovrebbe scegliere (0, 0)
    EXPECT_DOUBLE_EQ(best.x, 0.0);
    EXPECT_DOUBLE_EQ(best.y, 0.0);
}
```

#### Test 1.5: Individual Tests
```cpp
TEST(Individual, Mutation) {
    std::vector<Polygon*> parts = createTestParts(5);
    DeepNestConfig config;
    config.mutationRate = 50; // 50% mutation rate
    config.rotations = 4;

    Individual ind(parts, config, 12345); // seed fisso
    Individual original = ind.clone();

    ind.mutate(config.mutationRate, config.rotations, 12345);

    // Verificare che almeno qualcosa sia cambiato
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

    EXPECT_TRUE(placementChanged || rotationChanged);
}
```

#### Test 1.6: Population Tests
```cpp
TEST(Population, Initialization) {
    DeepNestConfig config;
    config.populationSize = 10;

    Population pop(config, 12345); // seed fisso
    std::vector<Polygon*> parts = createTestParts(5);

    pop.initialize(parts);

    EXPECT_EQ(pop.size(), 10);
}

TEST(Population, Crossover) {
    DeepNestConfig config;
    Population pop(config, 12345);

    std::vector<Polygon*> parts = createTestParts(5);
    Individual parent1(parts, config, 111);
    Individual parent2(parts, config, 222);

    auto children = pop.crossover(parent1, parent2);

    // Verificare che i children abbiano parti da entrambi i genitori
    EXPECT_EQ(children.first.placement.size(), parts.size());
    EXPECT_EQ(children.second.placement.size(), parts.size());
}

TEST(Population, NextGeneration) {
    DeepNestConfig config;
    config.populationSize = 10;

    Population pop(config, 12345);
    std::vector<Polygon*> parts = createTestParts(5);
    pop.initialize(parts);

    // Assegna fitness agli individui
    for (size_t i = 0; i < pop.size(); i++) {
        pop[i].fitness = static_cast<double>(i); // fitness crescente
    }

    Individual best = pop.getBest();
    double bestFitness = best.fitness;

    pop.nextGeneration();

    // Il migliore dovrebbe essere preservato (elitism)
    EXPECT_DOUBLE_EQ(pop[0].fitness, bestFitness);
}
```

---

### TEST SUITE 2: Integration Tests

#### Test 2.1: PlacementWorker Integration Test
```cpp
TEST(PlacementWorker, SimpleRectanglePlacement) {
    DeepNestConfig config;
    config.spacing = 0.0;
    config.rotations = 1; // no rotazioni per semplicità
    config.placementType = "gravity";

    NFPCache cache;
    NFPCalculator calculator(cache);
    PlacementWorker worker(config, calculator);

    // Sheet: 100x100
    Polygon sheet;
    sheet.points = {{0,0}, {100,0}, {100,100}, {0,100}};
    sheet.id = -1;
    sheet.source = -1;

    // Parts: 3 rettangoli 10x10
    std::vector<Polygon> parts;
    for (int i = 0; i < 3; i++) {
        Polygon part;
        part.points = {{0,0}, {10,0}, {10,10}, {0,10}};
        part.id = i;
        part.source = i;
        part.rotation = 0.0;
        parts.push_back(part);
    }

    auto result = worker.placeParts({sheet}, parts);

    // Verifiche:
    EXPECT_EQ(result.unplacedParts.size(), 0); // Tutte le parti piazzate
    EXPECT_GT(result.placements.size(), 0);
    EXPECT_GT(result.fitness, 0.0);
    EXPECT_LT(result.fitness, std::numeric_limits<double>::max());

    // Verificare che le parti non si sovrappongano
    // ...
}

TEST(PlacementWorker, FitnessDecreases) {
    // Test che il fitness sia influenzato dalla disposizione

    // Configurazione 1: parti ben disposte
    // Configurazione 2: parti mal disposte

    // EXPECT_LT(fitness1, fitness2);
}
```

#### Test 2.2: GeneticAlgorithm Integration Test
```cpp
TEST(GeneticAlgorithm, Evolution) {
    DeepNestConfig config;
    config.populationSize = 10;
    config.mutationRate = 10;
    config.rotations = 4;

    std::vector<Polygon> parts = createTestPartsVector(5);
    std::vector<Polygon*> partPtrs;
    for (auto& p : parts) {
        partPtrs.push_back(&p);
    }

    GeneticAlgorithm ga(partPtrs, config);

    // Simula valutazione fitness per generazione iniziale
    auto& pop = ga.getPopulation();
    for (size_t i = 0; i < pop.size(); i++) {
        pop[i].fitness = 100.0 + static_cast<double>(i) * 10.0;
    }

    double initialBestFitness = ga.getBestIndividual().fitness;

    // Crea nuova generazione
    ga.generation();

    // Simula valutazione per nuova generazione
    for (size_t i = 0; i < pop.size(); i++) {
        pop[i].fitness = 90.0 + static_cast<double>(i) * 10.0; // Migliorato
    }

    double newBestFitness = ga.getBestIndividual().fitness;

    // Il miglior fitness dovrebbe essere preservato o migliorato (elitism)
    EXPECT_LE(newBestFitness, initialBestFitness);
}
```

---

### TEST SUITE 3: End-to-End Tests

#### Test 3.1: Complete Nesting Scenario
```cpp
TEST(EndToEnd, RectangleNesting) {
    // Setup completo come in un'applicazione reale
    DeepNestSolver solver;

    solver.setSpacing(2.0);
    solver.setRotations(4);
    solver.setPopulationSize(10);
    solver.setPlacementType("gravity");

    // Sheet 200x200
    Polygon sheet;
    sheet.points = {{0,0}, {200,0}, {200,200}, {0,200}};
    solver.addSheet(sheet, 1, "Sheet1");

    // 10 rettangoli 20x30
    for (int i = 0; i < 10; i++) {
        Polygon part;
        part.points = {{0,0}, {20,0}, {20,30}, {0,30}};
        solver.addPart(part, 1, "Part" + std::to_string(i));
    }

    // Esegui nesting per 10 generazioni
    bool progressCalled = false;
    solver.setProgressCallback([&](const NestProgress& progress) {
        progressCalled = true;
        EXPECT_GE(progress.generation, 0);
        EXPECT_LE(progress.percentComplete, 100.0);
    });

    solver.start(10);

    // Passo per passo
    int iterations = 0;
    while (solver.isRunning() && iterations < 100) {
        solver.step();
        iterations++;
    }

    EXPECT_TRUE(progressCalled);

    const NestResult* result = solver.getBestResult();
    ASSERT_NE(result, nullptr);

    // Verifiche sul risultato:
    EXPECT_GT(result->fitness, 0.0);
    EXPECT_LT(result->fitness, std::numeric_limits<double>::max());
    EXPECT_GT(result->placements.size(), 0);

    std::cout << "Best fitness: " << result->fitness << std::endl;
    std::cout << "Placements: " << result->placements.size() << std::endl;
}

TEST(EndToEnd, FitnessConvergence) {
    // Test che il fitness converga nelle generazioni

    DeepNestSolver solver;
    solver.setPopulationSize(20);
    // ...setup...

    std::vector<double> fitnessHistory;

    solver.setProgressCallback([&](const NestProgress& progress) {
        fitnessHistory.push_back(progress.bestFitness);
    });

    solver.runUntilComplete(50); // 50 generazioni

    // Verificare che il fitness sia generalmente decrescente
    // (potrebbe avere fluttuazioni ma la tendenza deve essere migliorativa)

    ASSERT_GT(fitnessHistory.size(), 10);

    double firstFitness = fitnessHistory[0];
    double lastFitness = fitnessHistory[fitnessHistory.size() - 1];

    EXPECT_LT(lastFitness, firstFitness) << "Fitness should improve over generations";
}
```

---

### TEST SUITE 4: JavaScript Comparison Tests

#### Test 4.1: NFP Comparison
```cpp
TEST(JSComparison, NFPResults) {
    // Caricare dati di test da file JSON generato da JavaScript
    // formato: {
    //   "polygonA": [...],
    //   "polygonB": [...],
    //   "expectedNFP": [...]
    // }

    auto testData = loadJSONTestData("nfp_test_data.json");

    NFPCache cache;
    NFPCalculator calc(cache);

    Polygon A = polygonFromJSON(testData["polygonA"]);
    Polygon B = polygonFromJSON(testData["polygonB"]);

    Polygon nfp = calc.getOuterNFP(A, B, false);

    Polygon expectedNFP = polygonFromJSON(testData["expectedNFP"]);

    // Confronta i poligoni (con tolleranza per errori numerici)
    EXPECT_TRUE(polygonsAlmostEqual(nfp, expectedNFP, 0.001));
}
```

#### Test 4.2: Fitness Comparison
```cpp
TEST(JSComparison, FitnessCalculation) {
    // Caricare uno scenario completo da JSON
    // Eseguire placement in C++
    // Confrontare fitness con quello calcolato da JS

    auto testData = loadJSONTestData("placement_scenario.json");

    // ...setup ed esecuzione...

    double cppFitness = result.fitness;
    double jsFitness = testData["expectedFitness"].asDouble();

    // Tolleranza 0.1%
    EXPECT_NEAR(cppFitness, jsFitness, jsFitness * 0.001);
}
```

---

### TEST SUITE 5: Performance Tests

#### Test 5.1: NFP Cache Performance
```cpp
TEST(Performance, NFPCacheSpeedup) {
    NFPCache cache;
    NFPCalculator calc(cache);

    Polygon A = createComplexPolygon(100); // 100 vertici
    Polygon B = createComplexPolygon(80);

    // Prima chiamata (senza cache)
    auto start = std::chrono::high_resolution_clock::now();
    calc.getOuterNFP(A, B, false);
    auto end = std::chrono::high_resolution_clock::now();
    auto uncachedTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Seconda chiamata (con cache)
    start = std::chrono::high_resolution_clock::now();
    calc.getOuterNFP(A, B, false);
    end = std::chrono::high_resolution_clock::now();
    auto cachedTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Uncached: " << uncachedTime.count() << "μs, "
              << "Cached: " << cachedTime.count() << "μs" << std::endl;

    // Cache dovrebbe essere molto più veloce
    EXPECT_LT(cachedTime.count(), uncachedTime.count() / 10);
}
```

---

### TEST SUITE 6: Regression Tests

#### Test 6.1: Known Issue Tests
```cpp
TEST(Regression, FitnessNotConstant) {
    // Questo test fallisce con il codice attuale!
    // Dovrebbe passare dopo le fix

    DeepNestSolver solver;
    solver.setPopulationSize(5);

    // ...setup semplice...

    std::set<double> uniqueFitness;

    solver.setProgressCallback([&](const NestProgress& progress) {
        uniqueFitness.insert(progress.bestFitness);
    });

    solver.runUntilComplete(10);

    // Dovrebbero esserci valori diversi di fitness
    EXPECT_GT(uniqueFitness.size(), 1) << "Fitness should vary, not be constant!";
}
```

---

## ROADMAP DI IMPLEMENTAZIONE

### SPRINT 1 (Giorni 1-3): Fix Critici
- ✅ Analisi completa (FATTO)
- [ ] Implementare FASE 1: Fix Fitness Calculation
- [ ] Test 6.1: Verificare che fitness non sia più costante
- [ ] Test 2.1: PlacementWorker integration test

**Deliverable**: Fitness che varia correttamente tra le soluzioni

### SPRINT 2 (Giorni 4-6): Merged Lines
- [ ] Leggere implementazione JavaScript di mergedLength
- [ ] Implementare FASE 2: Merged Lines Calculation
- [ ] Test 1.4: MergeDetection unit tests
- [ ] Integration test con mergeLines = true/false

**Deliverable**: MergeDetection funzionante

### SPRINT 3 (Giorni 7-9): NFP Verification
- [ ] FASE 3: Fix Candidate Position Extraction
- [ ] FASE 4: Verify NFP Calculation
- [ ] Test 1.2: MinkowskiSum tests
- [ ] Test 1.3: NFPCalculator tests
- [ ] Test 4.1: NFP comparison con JavaScript

**Deliverable**: NFP calculation verificato e corretto

### SPRINT 4 (Giorni 10-12): Polygon Operations & GA
- [ ] FASE 5: Verify Polygon Operations
- [ ] FASE 6: Fix Placement Strategy Edge Cases
- [ ] FASE 7: Verify Genetic Algorithm
- [ ] Test 1.5: Individual tests
- [ ] Test 1.6: Population tests
- [ ] Test 2.2: GA integration tests

**Deliverable**: GA completamente funzionante

### SPRINT 5 (Giorni 13-15): End-to-End Testing
- [ ] TEST SUITE 3: End-to-End tests
- [ ] TEST SUITE 4: JavaScript comparison
- [ ] TEST SUITE 5: Performance tests
- [ ] Documentazione risultati

**Deliverable**: Sistema completo testato e validato

---

## METRICHE DI SUCCESSO

### Metriche Funzionali:
1. ✅ **Fitness Variable**: Il fitness deve variare tra soluzioni diverse
2. ✅ **Fitness Convergence**: Il fitness deve migliorare nelle generazioni successive
3. ✅ **Parts Placement**: Tutte le parti devono essere piazzate (o penalty corretta)
4. ✅ **No Overlaps**: Le parti non devono sovrapporsi
5. ✅ **NFP Correctness**: NFP identici (±0.1%) rispetto a JavaScript

### Metriche di Performance:
1. ✅ **NFP Cache Hit Rate**: >80% per test ripetuti
2. ✅ **Generation Time**: <5s per generazione (10 parti, pop=10)
3. ✅ **Memory Usage**: <500MB per nesting di 100 parti

### Metriche di Qualità:
1. ✅ **Unit Test Coverage**: >80%
2. ✅ **Integration Test Coverage**: >70%
3. ✅ **End-to-End Tests**: Almeno 5 scenari diversi
4. ✅ **JavaScript Parity**: Risultati entro ±1% del JavaScript

---

## STRUMENTI DI DEBUG

### Debug Logging System
```cpp
// debug.h
#define DEBUG_FITNESS 1
#define DEBUG_NFP 1
#define DEBUG_PLACEMENT 1

#ifdef DEBUG_FITNESS
    #define LOG_FITNESS(msg, value) \
        std::cout << "[FITNESS] " << msg << ": " << value << std::endl;
#else
    #define LOG_FITNESS(msg, value)
#endif
```

### Fitness Trace
```cpp
// In PlacementWorker::placeParts():
LOG_FITNESS("Sheet area", sheetArea);
LOG_FITNESS("minwidth", minwidth);
LOG_FITNESS("minarea", minarea);
LOG_FITNESS("Total fitness", fitness);
```

### NFP Visualization
```cpp
// Funzione helper per esportare NFP in SVG per visualizzazione
void exportNFPtoSVG(const Polygon& nfp, const std::string& filename);
```

---

## CONCLUSIONI

### Problemi Principali Identificati:
1. **CRITICO**: Fitness calculation incompleto → algoritmo non funziona
2. **ALTO**: Merged lines non implementato → ottimizzazione mancante
3. **MEDIO**: Possibili discrepanze in NFP calculation

### Priorità Assoluta:
**Fix del calcolo del fitness** - senza questo fix, l'algoritmo genetico è completamente inutilizzabile.

### Tempo Stimato:
- Fix critici: 3 giorni
- Implementazione completa: 15 giorni
- Testing e validazione: +5 giorni
- **Totale: ~20 giorni lavorativi**

### Rischio:
- **BASSO**: I problemi sono ben identificati e le soluzioni sono chiare
- **MEDIO**: Potrebbero esserci altre discrepanze non ancora scoperte nell'NFP calculation
- **BASSO**: I test di confronto con JavaScript permetteranno validazione completa
