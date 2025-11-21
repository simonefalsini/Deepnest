# PIANO DI SVILUPPO - FIX DEEPNEST C++
**Data Creazione**: 2025-11-21
**Priorità**: CRITICA
**Owner**: Team Development

---

## OBIETTIVO

Implementare le correzioni necessarie per risolvere i problemi critici identificati nell'analisi del porting DeepNest C++, con focus su:

1. **noFitPolygon() non funzionante** (genera poligoni distorti)
2. **Spacing genera eccezioni Boost** (poligoni invalidi)
3. **Fitness calculation incompleto** (manca timeRatio)
4. **Race conditions** nei restart

---

## FASE 1: FIX CRITICI (Settimana 1) - 5-7 giorni

### 1.1 Riabilitare Loop Closure Detection in noFitPolygon()

**Priorità**: 🔴 **CRITICA**
**Effort**: 1-2 giorni
**Owner**: Developer 1
**Rischio**: Alto (potrebbe scoprire altri bug)

#### Task 1.1.1: Riabilitare codice commentato

**File**: `deepnest-cpp/src/geometry/GeometryUtil.cpp`
**Linee**: 822-834

**Azione**:
```cpp
// RIMUOVERE questo commento:
// TEMPORARY: Disabled to force closure only at start point

// DECOMMENTARE questo blocco:
bool looped = false;
if (nfp.size() > 2 * std::max(A.size(), B.size())) {
    for (size_t i = 0; i < nfp.size() - 1; i++) {
        if (almostEqual(reference.x, nfp[i].x) && almostEqual(reference.y, nfp[i].y)) {
            LOG_NFP("    Loop closed: returned to point " << i << " ("
                   << nfp[i].x << ", " << nfp[i].y << ")");
            looped = true;
            break;
        }
    }
}

if (looped) {
    break;  // Completed loop
}
```

**Test**:
1. Creare test con rettangoli semplici (0° e 90°)
2. Testare con L-shapes ruotate (0°, 45°, 90°, 135°)
3. Testare con poligoni concavi
4. Verificare che NFP si chiuda sempre

**Success Criteria**:
- NFP ha sempre loop chiuso (primo punto = ultimo punto)
- Nessun crossing o self-intersection
- Forma corrisponde a quella attesa

#### Task 1.1.2: Aggiungere Validation Output

**File**: `deepnest-cpp/src/geometry/GeometryUtil.cpp`
**Dopo**: Linea 874 (fine funzione noFitPolygon)

**Azione**:
```cpp
// Validate NFP before returning
if (!nfpList.empty()) {
    for (auto& nfp : nfpList) {
        // Check if loop is closed
        if (nfp.size() >= 3) {
            const Point& first = nfp.front();
            const Point& last = nfp.back();

            if (!almostEqual(first.x, last.x) || !almostEqual(first.y, last.y)) {
                LOG_NFP("WARNING: NFP loop not closed! Distance: "
                       << std::sqrt((last.x - first.x) * (last.x - first.x) +
                                   (last.y - first.y) * (last.y - first.y)));

                // FORCE close the loop
                nfp.push_back(first);
            }

            // Check for self-intersection (basic)
            double area = GeometryUtil::polygonArea(nfp);
            if (std::abs(area) < 1e-6) {
                LOG_NFP("WARNING: NFP has near-zero area! Area: " << area);
            }
        }
    }
}
```

**Test**:
- Verificare che warning vengano stampati quando loop non si chiude
- Verificare che force-close funzioni

**Success Criteria**:
- Tutti gli NFP hanno loop chiusi
- Warning visibili nei log quando ci sono problemi

#### Task 1.1.3: Unit Tests per noFitPolygon

**File**: `deepnest-cpp/tests/NFPRegressionTests.cpp` (NUOVO)

**Azione**:
```cpp
#include <gtest/gtest.h>
#include "../include/deepnest/geometry/GeometryUtil.h"

TEST(NoFitPolygonTest, SimpleRectangles) {
    // Rettangolo A: 100x50
    std::vector<Point> A = {{0, 0}, {100, 0}, {100, 50}, {0, 50}};

    // Rettangolo B: 30x20
    std::vector<Point> B = {{0, 0}, {30, 0}, {30, 20}, {0, 20}};

    // Calculate outer NFP
    auto nfpList = GeometryUtil::noFitPolygon(A, B, false, false);

    ASSERT_EQ(nfpList.size(), 1) << "Should generate 1 NFP";
    ASSERT_GE(nfpList[0].size(), 4) << "NFP should have at least 4 points";

    // Check loop closure
    const Point& first = nfpList[0].front();
    const Point& last = nfpList[0].back();
    EXPECT_NEAR(first.x, last.x, 1e-6) << "Loop not closed (X)";
    EXPECT_NEAR(first.y, last.y, 1e-6) << "Loop not closed (Y)";

    // Check area is positive (CCW)
    double area = GeometryUtil::polygonArea(nfpList[0]);
    EXPECT_GT(area, 0) << "NFP should be CCW (positive area)";
}

TEST(NoFitPolygonTest, RotatedShapes) {
    // L-shape
    std::vector<Point> A = {{0, 0}, {50, 0}, {50, 20}, {20, 20}, {20, 50}, {0, 50}};

    // Small rectangle
    std::vector<Point> B = {{0, 0}, {15, 0}, {15, 15}, {0, 15}};

    // Test multiple rotations
    for (int angle = 0; angle < 360; angle += 45) {
        auto rotatedA = GeometryUtil::rotatePolygon(A, angle);

        auto nfpList = GeometryUtil::noFitPolygon(rotatedA, B, false, false);

        ASSERT_FALSE(nfpList.empty()) << "Should generate NFP for rotation " << angle;

        // Validate each NFP
        for (const auto& nfp : nfpList) {
            ASSERT_GE(nfp.size(), 3) << "NFP too small";

            // Check closure
            const Point& first = nfp.front();
            const Point& last = nfp.back();
            EXPECT_NEAR(first.x, last.x, 1e-6) << "Loop not closed at angle " << angle;
            EXPECT_NEAR(first.y, last.y, 1e-6) << "Loop not closed at angle " << angle;
        }
    }
}

TEST(NoFitPolygonTest, PolygonWithHoles) {
    // TODO: Implement test with holes
}
```

**Build**:
```bash
cd deepnest-cpp/tests
# Aggiungere NFPRegressionTests.cpp a CMakeLists.txt
qmake
make
./NFPRegressionTests
```

**Success Criteria**:
- Tutti i test passano
- Test coprono casi edge (rotazioni, concavi, holes)

---

### 1.2 Aggiungere timeRatio al Fitness Calculation

**Priorità**: 🔴 **CRITICA**
**Effort**: 1 giorno
**Owner**: Developer 2
**Rischio**: Basso

#### Task 1.2.1: Aggiungere timeRatio a DeepNestConfig

**File 1**: `deepnest-cpp/include/deepnest/config/DeepNestConfig.h`

**Azione** (dopo linea ~30):
```cpp
class DeepNestConfig {
public:
    // ... existing members ...

    double timeRatio = 0.5;  // ⬅️ AGGIUNGERE QUESTO

    // ... existing methods ...

    // ⬅️ AGGIUNGERE GETTER/SETTER
    double getTimeRatio() const { return timeRatio; }
    void setTimeRatio(double value) { timeRatio = value; }
};
```

**File 2**: `deepnest-cpp/src/config/DeepNestConfig.cpp`

**Azione**: Aggiungere parsing da JSON (se presente):
```cpp
void DeepNestConfig::loadFromJson(const QString& jsonStr) {
    // ... existing parsing ...

    if (obj.contains("timeRatio")) {
        timeRatio = obj["timeRatio"].toDouble();
    }
}

QString DeepNestConfig::toJson() const {
    // ... existing serialization ...

    obj["timeRatio"] = timeRatio;  // ⬅️ AGGIUNGERE

    // ... return JSON ...
}
```

**Test**:
```cpp
// Unit test
DeepNestConfig config;
config.setTimeRatio(0.75);
EXPECT_EQ(config.getTimeRatio(), 0.75);

// JSON test
QString json = "{\"timeRatio\": 0.3}";
config.loadFromJson(json);
EXPECT_EQ(config.getTimeRatio(), 0.3);
```

**Success Criteria**:
- timeRatio presente in config
- Default value = 0.5
- Getter/setter funzionanti
- Load/save da JSON

#### Task 1.2.2: Modificare Fitness Calculation

**File**: `deepnest-cpp/src/placement/PlacementWorker.cpp`
**Linea**: 617

**Azione**:
```cpp
// PRIMA:
if (config_.mergeLines) {
    totalMerged = calculateTotalMergedLength(allPlacements, rotatedParts);
    fitness -= totalMerged;  // ❌ PROBLEMA
}

// DOPO:
if (config_.mergeLines) {
    totalMerged = calculateTotalMergedLength(allPlacements, rotatedParts);
    fitness -= totalMerged * config_.timeRatio;  // ✅ CORRETTO
}
```

**Test**:
```cpp
// Test fitness with different timeRatio values
DeepNestConfig config1;
config1.timeRatio = 0.0;  // No line merge bonus
// ... run placement ...
double fitness1 = result.fitness;

DeepNestConfig config2;
config2.timeRatio = 0.5;  // Medium bonus
// ... run placement ...
double fitness2 = result.fitness;

DeepNestConfig config3;
config3.timeRatio = 1.0;  // Full bonus
// ... run placement ...
double fitness3 = result.fitness;

// Fitness should decrease with higher timeRatio (if mergedLength > 0)
EXPECT_GE(fitness1, fitness2);
EXPECT_GE(fitness2, fitness3);
```

**Success Criteria**:
- Fitness calculation usa timeRatio
- timeRatio=0 disabilita bonus
- timeRatio=1 applica bonus completo
- Default (0.5) comportamento ragionevole

#### Task 1.2.3: Update Documentation

**File**: `deepnest-cpp/README.md`

**Azione**: Aggiungere sezione:
```markdown
### Configuration Parameters

...

- **timeRatio** (0.0-1.0, default: 0.5): Weight for line merging bonus in fitness calculation.
  - 0.0 = Disable line merge bonus
  - 0.5 = Moderate bonus (default)
  - 1.0 = Maximum bonus (prioritize merged lines)
```

**Success Criteria**:
- Documentazione aggiornata
- Esempio di uso incluso

---

### 1.3 Fix Spacing Validation e Boost Exceptions

**Priorità**: 🔴 **CRITICA**
**Effort**: 2-3 giorni
**Owner**: Developer 1
**Rischio**: Medio

#### Task 1.3.1: Aumentare Epsilon in cleanPolygon()

**File**: `deepnest-cpp/src/geometry/PolygonOperations.cpp`
**Linea**: 102

**Azione**:
```cpp
// PRIMA:
Paths64 solution = SimplifyPaths<int64_t>({path}, scale * 0.0001);  // ❌ Troppo piccolo

// DOPO:
Paths64 solution = SimplifyPaths<int64_t>({path}, scale * 0.001);  // ✅ Più robusto
```

**Reasoning**:
- `0.0001 * 10000000 = 1000` unità Clipper
- `0.001 * 10000000 = 10000` unità Clipper (più aggressivo)

**Test**:
1. Poligono con piccole self-intersections
2. Poligono con spikes
3. Poligono con collinear edges

**Success Criteria**:
- cleanPolygon() rimuove più artefatti
- Poligoni output validi per Boost

#### Task 1.3.2: Aumentare Threshold Area Minima

**File**: `deepnest-cpp/src/engine/NestingEngine.cpp`
**Linee**: 122, 145

**Azione**:
```cpp
// PRIMA:
if (sheet.points.size() < 3 || std::abs(sheet.area()) < 1e-6) {  // ❌ Troppo piccolo
    continue;
}

// DOPO:
if (sheet.points.size() < 3 || std::abs(sheet.area()) < 1e-2) {  // ✅ Più realistico
    LOG_NESTING("WARNING: Sheet " << sheet.id << " has area " << sheet.area()
               << " < 1e-2, skipping");
    continue;
}
```

**Reasoning**:
- `1e-6` è 0.000001 square units → troppo piccolo
- `1e-2` è 0.01 square units → più ragionevole per poligoni reali

**Test**:
1. Spacing su foglio 100x100 con spacing=50 → dovrebbe rimanere valido
2. Spacing su foglio 10x10 con spacing=50 → dovrebbe essere scartato

**Success Criteria**:
- Poligoni quasi-degenerati vengono scartati
- Warning visibili nei log

#### Task 1.3.3: Aggiungere Self-Intersection Detection

**File**: `deepnest-cpp/src/geometry/PolygonOperations.cpp`

**Azione**: Nuova funzione
```cpp
bool PolygonOperations::hasSelfintersection(const std::vector<Point>& poly) {
    // Usa Clipper2 per verificare validità
    auto& config = DeepNestConfig::getInstance();
    double scale = config.getClipperScale();

    Path64 path = toClipperPath64(poly, scale);

    // Check if path is valid (no self-intersections)
    // Clipper2 non ha IsValid(), quindi testiamo indirettamente
    // facendo una union con se stesso
    Paths64 solution = Union({path}, {path}, FillRule::NonZero);

    // Se union produce più di 1 poligono, c'è self-intersection
    return solution.size() != 1;
}

std::vector<Point> PolygonOperations::repairPolygon(const std::vector<Point>& poly) {
    // Repair usando buffer positivo+negativo
    auto& config = DeepNestConfig::getInstance();
    double scale = config.getClipperScale();

    PathD pathD = toClipperPathD(poly);

    // Inflate di piccolo ammontare
    double bufferSize = 0.1;
    PathsD inflated = InflatePaths({pathD}, bufferSize, JoinType::Miter, EndType::Polygon);

    if (inflated.empty()) {
        return {};
    }

    // Deflate per tornare a dimensione originale
    PathsD deflated = InflatePaths(inflated, -bufferSize, JoinType::Miter, EndType::Polygon);

    if (deflated.empty()) {
        return {};
    }

    // Prendi il poligono più grande
    PathD biggest = deflated[0];
    double biggestArea = std::abs(Clipper2Lib::Area(biggest));

    for (size_t i = 1; i < deflated.size(); i++) {
        double area = std::abs(Clipper2Lib::Area(deflated[i]));
        if (area > biggestArea) {
            biggest = deflated[i];
            biggestArea = area;
        }
    }

    return fromClipperPathD(biggest);
}
```

**Header** (`PolygonOperations.h`):
```cpp
class PolygonOperations {
public:
    // ... existing ...

    static bool hasSelfintersection(const std::vector<Point>& poly);
    static std::vector<Point> repairPolygon(const std::vector<Point>& poly);
};
```

**Uso in NestingEngine**:
```cpp
// Dopo applySpacing()
if (config_.spacing > 0) {
    part = applySpacing(part, 0.5 * config_.spacing, config_.curveTolerance);

    // Check e repair
    if (part.points.size() >= 3) {
        if (PolygonOperations::hasSelfintersection(part.points)) {
            LOG_NESTING("WARNING: Part " << part.id << " has self-intersection after spacing, repairing...");

            part.points = PolygonOperations::repairPolygon(part.points);

            if (part.points.empty()) {
                LOG_NESTING("ERROR: Failed to repair part " << part.id << ", skipping");
                continue;
            }
        }
    }
}
```

**Test**:
1. Poligono con self-intersection evidente → detection TRUE
2. Poligono valido → detection FALSE
3. Poligono con self-intersection → repair SUCCESSFUL

**Success Criteria**:
- Detection funziona al 100%
- Repair risolve la maggior parte dei casi
- Nessuna eccezione Boost dopo spacing

#### Task 1.3.4: Integration Testing

**File**: `deepnest-cpp/tests/SpacingValidationTests.cpp` (NUOVO)

**Azione**:
```cpp
#include <gtest/gtest.h>
#include "../include/deepnest/engine/NestingEngine.h"
#include "../include/deepnest/geometry/PolygonOperations.h"

TEST(SpacingValidationTest, SmallSpacing) {
    DeepNestConfig config;
    config.spacing = 5.0;

    // Rectangle 100x50
    Polygon part;
    part.points = {{0, 0}, {100, 0}, {100, 50}, {0, 50}};

    // Apply spacing
    std::vector<std::vector<Point>> offset = PolygonOperations::offset(
        part.points, 0.5 * config.spacing, 4.0, 0.25
    );

    ASSERT_FALSE(offset.empty()) << "Offset should succeed";
    ASSERT_GE(offset[0].size(), 4) << "Offset polygon should have points";

    // Check no self-intersection
    bool hasSI = PolygonOperations::hasSelfintersection(offset[0]);
    EXPECT_FALSE(hasSI) << "Offset should not have self-intersection";
}

TEST(SpacingValidationTest, LargeSpacing) {
    DeepNestConfig config;
    config.spacing = 100.0;  // Larger than polygon!

    // Small rectangle 50x30
    Polygon part;
    part.points = {{0, 0}, {50, 0}, {50, 30}, {0, 30}};

    // Apply spacing - should fail or return empty
    std::vector<std::vector<Point>> offset = PolygonOperations::offset(
        part.points, 0.5 * config.spacing, 4.0, 0.25
    );

    // Offset negativo troppo grande dovrebbe dare risultato vuoto
    EXPECT_TRUE(offset.empty() || offset[0].size() < 3)
        << "Large offset should invalidate small polygon";
}

TEST(SpacingValidationTest, AcuteAngles) {
    DeepNestConfig config;
    config.spacing = 10.0;

    // Triangle with acute angle
    Polygon part;
    part.points = {{0, 0}, {100, 0}, {50, 10}};  // Very flat triangle

    // Apply spacing
    std::vector<std::vector<Point>> offset = PolygonOperations::offset(
        part.points, 0.5 * config.spacing, 4.0, 0.25
    );

    if (!offset.empty() && offset[0].size() >= 3) {
        // Check and repair if needed
        if (PolygonOperations::hasSelfintersection(offset[0])) {
            std::vector<Point> repaired = PolygonOperations::repairPolygon(offset[0]);

            ASSERT_FALSE(repaired.empty()) << "Repair should succeed";
            EXPECT_FALSE(PolygonOperations::hasSelfintersection(repaired))
                << "Repaired polygon should be valid";
        }
    }
}
```

**Success Criteria**:
- Tutti i test passano
- Nessuna eccezione Boost
- Poligoni sempre validi dopo spacing

---

## FASE 2: FIX ALTA PRIORITÀ (Settimane 2-3) - 7-10 giorni

### 2.1 Rimuovere searchStartPoint Fallback

**Priorità**: 🟠 **ALTA**
**Effort**: 1-2 giorni
**Owner**: Developer 1
**Rischio**: Basso

#### Task 2.1.1: Eliminare Fallback a Heuristic

**File**: `deepnest-cpp/src/geometry/GeometryUtil.cpp`
**Linee**: 648-654

**Azione**:
```cpp
// RIMUOVERE:
if (startOpt.has_value()) {
    LOG_NFP("  Start point (search - external): (" << startOpt->x << ", " << startOpt->y << ")");
} else {
    // Fallback to heuristic if search fails
    LOG_NFP("  WARNING: searchStartPoint failed, using heuristic");
    startOpt = heuristicStart;  // ❌ RIMUOVERE QUESTO
}

// SOSTITUIRE CON:
if (!startOpt.has_value()) {
    LOG_NFP("  ERROR: searchStartPoint failed!");
    return {};  // Return empty NFP list instead of invalid result
}

LOG_NFP("  Start point (search): (" << startOpt->x << ", " << startOpt->y << ")");
```

**Test**:
1. Rettangoli 0° → deve funzionare
2. Rettangoli 45° → deve funzionare
3. L-shapes ruotate → deve funzionare
4. Forme impossibili (B > A) → deve ritornare vuoto (non crash)

**Success Criteria**:
- searchStartPoint sempre usato
- Nessun fallback silente
- Return vuoto se start point non trovato

#### Task 2.1.2: Migliorare searchStartPoint Implementation

**File**: `deepnest-cpp/src/geometry/GeometryUtilAdvanced.cpp`

**Verifica**: La funzione `searchStartPoint()` implementa correttamente:
1. Sampling di punti sul perimetro di A
2. Test di non-overlap con B
3. Priorità a punti sul perimetro esterno (non loop interni)

**Se necessario, aggiungere**:
```cpp
std::optional<Point> searchStartPoint(
    const std::vector<Point>& A,
    const std::vector<Point>& B,
    bool inside,
    const std::vector<std::vector<Point>>& nfpList
) {
    // Sample points along edges of A
    std::vector<Point> samples;

    for (size_t i = 0; i < A.size(); i++) {
        const Point& p1 = A[i];
        const Point& p2 = (i + 1 == A.size()) ? A[0] : A[i + 1];

        // Sample multiple points along edge
        int numSamples = 5;
        for (int j = 0; j <= numSamples; j++) {
            double t = static_cast<double>(j) / numSamples;
            Point sample(
                p1.x + t * (p2.x - p1.x),
                p1.y + t * (p2.y - p1.y)
            );
            samples.push_back(sample);
        }
    }

    // Test each sample
    for (const Point& sample : samples) {
        // Calculate offset to place B[0] at sample
        Point offset(sample.x - B[0].x, sample.y - B[0].y);

        // Translate B by offset
        std::vector<Point> translatedB;
        for (const Point& bp : B) {
            translatedB.push_back(Point(bp.x + offset.x, bp.y + offset.y));
        }

        // Check if B overlaps with A
        bool overlaps = false;

        if (inside) {
            // For inner NFP, B should be completely inside A
            for (const Point& bp : translatedB) {
                auto inResult = pointInPolygon(bp, A);
                if (!inResult.has_value() || !inResult.value()) {
                    overlaps = true;
                    break;
                }
            }
        } else {
            // For outer NFP, B should not overlap with A
            overlaps = intersect(A, translatedB);
        }

        if (!overlaps) {
            // Valid start point found
            return offset;
        }
    }

    // No valid start point found
    return std::nullopt;
}
```

**Test**:
- Test con varie forme e rotazioni
- Verificare che start point sia sempre sul perimetro

**Success Criteria**:
- searchStartPoint trova sempre punto valido (se esiste)
- Nessun punto interno o invalido
- Performance accettabile (< 10ms per poligoni normali)

---

### 2.2 Gestire Orientamento Holes in NFP

**Priorità**: 🟠 **ALTA**
**Effort**: 2 giorni
**Owner**: Developer 2
**Rischio**: Medio

#### Task 2.2.1: Correggere Orientamento Children

**File**: `deepnest-cpp/src/geometry/GeometryUtil.cpp`
**Linee**: 585-608

**Azione**:
```cpp
// PRIMA (solo outer boundary):
if (!inside) {
    if (areaA < 0) {
        std::reverse(A.begin(), A.end());
    }
    if (areaB < 0) {
        std::reverse(B.begin(), B.end());
    }
}

// DOPO (outer + holes):
if (!inside) {
    // OUTSIDE NFP: Both must be CCW
    if (areaA < 0) {
        LOG_NFP("  Correcting A orientation: CW → CCW");
        std::reverse(A.begin(), A.end());

        // ⬅️ AGGIUNGERE: Reverse anche children (holes devono essere CW se outer è CCW)
        // Ma in realtà, se invertiamo l'outer, dobbiamo anche invertire i children?
        // NO! I children devono mantenere orientamento OPPOSTO all'outer.
        // Se outer diventa CCW, children devono rimanere CW (già lo sono se areaA < 0)
        // Quindi NON serve invertire children
    }
    if (areaB < 0) {
        LOG_NFP("  Correcting B orientation: CW → CCW");
        std::reverse(B.begin(), B.end());
    }
} else {
    // INSIDE NFP: A must be CW, B must be CCW
    if (areaA > 0) {
        LOG_NFP("  Correcting A orientation: CCW → CW");
        std::reverse(A.begin(), A.end());
    }
    if (areaB < 0) {
        LOG_NFP("  Correcting B orientation: CW → CCW");
        std::reverse(B.begin(), B.end());
    }
}
```

**Problema Più Complesso**: I children (holes) NON sono passati come parametri a `noFitPolygon()`. Questa funzione lavora solo su `std::vector<Point>`, non su `Polygon` con children!

**Soluzione**:
1. Verificare dove vengono estratti i points da Polygon
2. Assicurarsi che children abbiano orientamento corretto PRIMA di chiamare noFitPolygon
3. Aggiungere assertion per verificare orientamento

#### Task 2.2.2: Validation Orientamento Pre-NFP

**File**: `deepnest-cpp/src/nfp/NFPCalculator.cpp` o `MinkowskiSum.cpp`

**Azione**: Prima di chiamare MinkowskiSum o noFitPolygon, validare:
```cpp
// In NFPCalculator::computeNFP() BEFORE calling MinkowskiSum::calculateNFP()
double areaA = GeometryUtil::polygonArea(A.points);
double areaB = GeometryUtil::polygonArea(B.points);

// Outer boundaries dovrebbero essere CCW (area positiva)
if (areaA < 0) {
    std::cerr << "WARNING: Polygon A has CW orientation (negative area), reversing..." << std::endl;
    std::reverse(const_cast<Polygon&>(A).points.begin(), const_cast<Polygon&>(A).points.end());
}

if (areaB < 0) {
    std::cerr << "WARNING: Polygon B has CW orientation (negative area), reversing..." << std::endl;
    std::reverse(const_cast<Polygon&>(B).points.begin(), const_cast<Polygon&>(B).points.end());
}

// Validate children (holes should be CW = negative area)
for (const auto& hole : A.children) {
    double holeArea = GeometryUtil::polygonArea(hole.points);
    if (holeArea > 0) {
        std::cerr << "WARNING: Hole in A has CCW orientation (positive area), should be CW!" << std::endl;
    }
}

for (const auto& hole : B.children) {
    double holeArea = GeometryUtil::polygonArea(hole.points);
    if (holeArea > 0) {
        std::cerr << "WARNING: Hole in B has CCW orientation (positive area), should be CW!" << std::endl;
    }
}
```

**Test**:
1. Polygon senza holes → nessun warning
2. Polygon con holes orientati correttamente → nessun warning
3. Polygon con holes orientati male → warning e correzione

**Success Criteria**:
- Validation attiva
- Warning visibili se orientamento sbagliato
- NFP calculation non fallisce per orientamento

---

### 2.3 Integrare Merged Length in Placement Strategy

**Priorità**: 🟠 **ALTA**
**Effort**: 3 giorni
**Owner**: Developer 2
**Rischio**: Medio

#### Task 2.3.1: Aggiungere mergedLength a BestPositionResult

**File**: `deepnest-cpp/include/deepnest/placement/PlacementStrategy.h`

**Azione**:
```cpp
struct BestPositionResult {
    Point position;
    double area;
    double mergedLength;  // ⬅️ AGGIUNGERE
};
```

#### Task 2.3.2: Calcolare Merged Length in findBestPosition()

**File**: `deepnest-cpp/src/placement/GravityPlacement.cpp` (e altri strategy)

**Azione**:
```cpp
BestPositionResult GravityPlacement::findBestPosition(
    const Polygon& part,
    const std::vector<PlacedPart>& placed,
    const std::vector<Point>& candidatePositions,
    const DeepNestConfig& config
) {
    // ... existing code ...

    for (const Point& candidatePos : candidatePositions) {
        // ... calculate bounds area ...

        double merged = 0.0;

        // ⬅️ AGGIUNGERE: Calculate merged length if enabled
        if (config.mergeLines && !placed.empty()) {
            // Transform part to candidate position
            Polygon shiftedPart = part;
            for (auto& p : shiftedPart.points) {
                p.x += candidatePos.x;
                p.y += candidatePos.y;
            }

            // Calculate merged length with all placed parts
            std::vector<Polygon> placedPolygons;
            for (const auto& placedPart : placed) {
                Polygon transformedPlaced = placedPart.polygon;
                for (auto& p : transformedPlaced.points) {
                    p.x += placedPart.position.x;
                    p.y += placedPart.position.y;
                }
                placedPolygons.push_back(transformedPlaced);
            }

            MergeDetection::MergeResult mergeResult = MergeDetection::calculateMergedLength(
                placedPolygons,
                shiftedPart,
                0.1,  // minLength
                config.curveTolerance
            );

            merged = mergeResult.totalLength;
        }

        // ⬅️ MODIFICARE: Include merged length in fitness
        // Gravity: area = width*2 + height - merged*timeRatio
        double fitness = area - (merged * config.timeRatio);

        if (fitness < bestFitness) {
            bestFitness = fitness;
            bestPosition = candidatePos;
            bestArea = area;
            bestMerged = merged;  // ⬅️ SALVARE
        }
    }

    return BestPositionResult{bestPosition, bestArea, bestMerged};
}
```

**Test**:
1. Placement senza line merge → mergedLength = 0
2. Placement con line merge → mergedLength > 0
3. Confronto fitness con/senza timeRatio

**Success Criteria**:
- mergedLength calcolato correttamente
- Fitness integra merged length
- Placement favorisce posizioni con linee merged (se timeRatio > 0)

#### Task 2.3.3: Update PlacementWorker per Usare mergedLength

**File**: `deepnest-cpp/src/placement/PlacementWorker.cpp`
**Linea**: 547

**Azione**:
```cpp
// PRIMA:
position = Placement(positionResult.position, part.id, part.source, part.rotation);
placements.push_back(position);
placed.push_back(part);

minarea_accumulator += positionResult.area;  // ⬅️ MODIFICARE

// DOPO:
position = Placement(positionResult.position, part.id, part.source, part.rotation);
placements.push_back(position);
placed.push_back(part);

// ⬅️ Subtract merged length from area accumulator (già applicato timeRatio in strategy)
minarea_accumulator += positionResult.area - (positionResult.mergedLength * config_.timeRatio);
```

**OPPURE** (più semplice): Non sottrarre qui perché già fatto in strategy:
```cpp
minarea_accumulator += positionResult.area;
// mergedLength già integrato nel positionResult.area dalla strategy!
```

**Verificare** quale approccio è corretto confrontando con JavaScript.

**Test**:
- Fitness totale corretto
- Line merge influenza selezione posizioni

**Success Criteria**:
- Fitness calculation consistente
- Merged length influenza placement
- Risultati migliori con timeRatio > 0

---

## FASE 3: FIX MEDIA PRIORITÀ (Settimana 4) - 3-5 giorni

### 3.1 Implementare Thread Synchronization Robusta

**Priorità**: 🟡 **MEDIA**
**Effort**: 2-3 giorni
**Owner**: Developer 1
**Rischio**: Alto

#### Task 3.1.1: Aggiungere Mutex per Parts Access

**File**: `deepnest-cpp/include/deepnest/engine/NestingEngine.h`

**Azione**:
```cpp
class NestingEngine {
private:
    // ... existing ...

    mutable std::mutex partsMutex_;      // ⬅️ AGGIUNGERE
    std::atomic<bool> running_{false};   // ⬅️ CAMBIARE da bool a atomic

    // ... existing ...
};
```

**File**: `deepnest-cpp/src/engine/NestingEngine.cpp`

**Azione in initialize()**:
```cpp
void NestingEngine::initialize(...) {
    // Force stop if running
    if (running_.load()) {
        stop();
    }

    // Wait for all threads to complete
    if (parallelProcessor_) {
        while (parallelProcessor_->hasActiveTasks()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // Lock before modifying parts
    std::lock_guard<std::mutex> lock(partsMutex_);

    parts_.clear();
    partPointers_.clear();

    // ... rest of initialization ...
}
```

**Azione in step()**:
```cpp
bool NestingEngine::step() {
    if (!running_.load() || !geneticAlgorithm_) {
        return false;
    }

    // ... rest of step logic (no lock needed for READ-ONLY access to partPointers_) ...
}
```

**Success Criteria**:
- Nessun race condition su parts_
- Nessun crash durante restart
- Thread-safe access garantito

#### Task 3.1.2: Implementare Condition Variable in ParallelProcessor

**File**: `deepnest-cpp/include/deepnest/parallel/ParallelProcessor.h`

**Azione**:
```cpp
class ParallelProcessor {
private:
    // ... existing ...

    std::condition_variable allTasksComplete_;  // ⬅️ AGGIUNGERE
    std::mutex taskMutex_;                      // ⬅️ AGGIUNGERE
    std::atomic<int> activeTasks_{0};           // ⬅️ AGGIUNGERE

public:
    // ... existing ...

    bool hasActiveTasks() const {
        return activeTasks_.load() > 0;
    }

    void waitAll();  // ⬅️ AGGIUNGERE DICHIARAZIONE
};
```

**File**: `deepnest-cpp/src/parallel/ParallelProcessor.cpp`

**Azione**:
```cpp
void ParallelProcessor::waitAll() {
    std::unique_lock<std::mutex> lock(taskMutex_);
    allTasksComplete_.wait(lock, [this] {
        return activeTasks_.load() == 0;
    });
}

template<typename Func>
std::future<typename std::result_of<Func()>::type>
ParallelProcessor::enqueue(Func&& f) {
    // ... existing code ...

    // Increment active tasks BEFORE submitting
    {
        std::lock_guard<std::mutex> lock(taskMutex_);
        activeTasks_++;
    }

    // Wrap function to decrement on completion
    auto wrapped = [this, func = std::forward<Func>(f)]() mutable {
        auto result = func();

        // Decrement and notify
        {
            std::lock_guard<std::mutex> lock(taskMutex_);
            activeTasks_--;
            if (activeTasks_ == 0) {
                allTasksComplete_.notify_all();
            }
        }

        return result;
    };

    // ... submit wrapped task ...
}
```

**Test**:
1. Start nesting → stop → wait → start again (ripetere 10 volte)
2. Verificare nessun crash
3. Verificare tutti i thread terminati dopo stop

**Success Criteria**:
- waitAll() funziona correttamente
- Nessun race condition
- Restart robusto

---

### 3.2 Aggiungere NFP Output Validation

**Priorità**: 🟡 **MEDIA**
**Effort**: 1 giorno
**Owner**: Developer 2
**Rischio**: Basso

#### Task 3.2.1: Validation in NFPCalculator

**File**: `deepnest-cpp/src/nfp/NFPCalculator.cpp`

**Azione**:
```cpp
Polygon NFPCalculator::computeNFP(const Polygon& A, const Polygon& B, bool inside) const {
    // ... existing MinkowskiSum call ...

    if (nfps.empty()) {
        // ... existing fallback ...
    }

    // ⬅️ AGGIUNGERE VALIDATION
    Polygon result = /* ... select largest NFP ... */;

    // Validate result
    if (!result.points.empty()) {
        // Check minimum size
        if (result.points.size() < 3) {
            std::cerr << "ERROR: NFP has < 3 points (" << result.points.size() << ")" << std::endl;
            return Polygon();  // Return empty
        }

        // Check area is non-zero
        double area = std::abs(GeometryUtil::polygonArea(result.points));
        if (area < 1e-6) {
            std::cerr << "ERROR: NFP has near-zero area (" << area << ")" << std::endl;
            return Polygon();  // Return empty
        }

        // Check for self-intersection (if available)
        if (PolygonOperations::hasSelfintersection(result.points)) {
            std::cerr << "WARNING: NFP has self-intersection, attempting repair..." << std::endl;

            std::vector<Point> repaired = PolygonOperations::repairPolygon(result.points);

            if (!repaired.empty() && repaired.size() >= 3) {
                result.points = repaired;
                std::cerr << "SUCCESS: NFP repaired" << std::endl;
            } else {
                std::cerr << "ERROR: Failed to repair NFP" << std::endl;
                return Polygon();  // Return empty
            }
        }
    }

    return result;
}
```

**Test**:
- NFP valido → nessun warning
- NFP con < 3 punti → error + return empty
- NFP con area zero → error + return empty
- NFP con self-intersection → warning + repair

**Success Criteria**:
- Validation attiva
- NFP invalidi scartati
- Self-intersections riparate

---

### 3.3 Implementare Polygon Repair Utility

**Priorità**: 🟡 **MEDIA**
**Effort**: 1-2 giorni
**Owner**: Developer 1
**Rischio**: Medio

(Già implementato in Task 1.3.3, solo testing aggiuntivo)

#### Task 3.3.1: Extended Testing

**File**: `deepnest-cpp/tests/PolygonRepairTests.cpp` (NUOVO)

**Azione**:
```cpp
TEST(PolygonRepairTest, SelfIntersection) {
    // Bow-tie shape (self-intersecting)
    std::vector<Point> bowtie = {
        {0, 0}, {100, 100}, {100, 0}, {0, 100}
    };

    EXPECT_TRUE(PolygonOperations::hasSelfintersection(bowtie));

    std::vector<Point> repaired = PolygonOperations::repairPolygon(bowtie);

    ASSERT_FALSE(repaired.empty());
    EXPECT_FALSE(PolygonOperations::hasSelfintersection(repaired));
}

TEST(PolygonRepairTest, Spike) {
    // Polygon with spike
    std::vector<Point> spiked = {
        {0, 0}, {100, 0}, {50, 50}, {50, 51}, {50, 50}, {100, 100}, {0, 100}
    };

    std::vector<Point> repaired = PolygonOperations::repairPolygon(spiked);

    ASSERT_FALSE(repaired.empty());
    // Spike should be removed or smoothed
}

TEST(PolygonRepairTest, CollinearEdges) {
    // Polygon with collinear edges
    std::vector<Point> collinear = {
        {0, 0}, {50, 0}, {100, 0}, {100, 100}, {0, 100}
    };

    std::vector<Point> repaired = PolygonOperations::repairPolygon(collinear);

    ASSERT_FALSE(repaired.empty());
    // Middle point should be merged
    EXPECT_LT(repaired.size(), collinear.size());
}
```

**Success Criteria**:
- Tutti i test passano
- Repair risolve casi comuni

---

## TESTING & VALIDATION STRATEGY

### Regression Testing Suite

Creare suite completa in `deepnest-cpp/tests/`:

1. **NFPRegressionTests.cpp** (Task 1.1.3)
2. **SpacingValidationTests.cpp** (Task 1.3.4)
3. **FitnessCalculationTests.cpp**
4. **PolygonRepairTests.cpp** (Task 3.3.1)
5. **ThreadSafetyTests.cpp**

### Performance Benchmarks

Creare `deepnest-cpp/tests/PerformanceBenchmarks.cpp`:

```cpp
TEST(PerformanceTest, NFPCalculation) {
    auto start = std::chrono::high_resolution_clock::now();

    // Calculate 100 NFPs
    for (int i = 0; i < 100; i++) {
        // ... calculate NFP ...
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "100 NFPs calculated in " << duration.count() << "ms" << std::endl;

    // Should be < 5 seconds
    EXPECT_LT(duration.count(), 5000);
}
```

### Integration Testing

Test completi end-to-end:

```cpp
TEST(IntegrationTest, FullNesting) {
    DeepNestConfig config;
    config.spacing = 5.0;
    config.rotations = 4;
    config.populationSize = 10;
    config.timeRatio = 0.5;

    NestingEngine engine(config);

    // Load parts and sheets
    std::vector<Polygon> parts = /* ... */;
    std::vector<int> quantities = /* ... */;
    std::vector<Polygon> sheets = /* ... */;
    std::vector<int> sheetQty = /* ... */;

    // Initialize
    engine.initialize(parts, quantities, sheets, sheetQty);

    // Run nesting
    engine.start(nullptr, nullptr, 5);  // 5 generations

    while (engine.step()) {
        // Wait for completion
    }

    // Verify results
    auto best = engine.getBestResult();
    ASSERT_NE(best, nullptr);
    EXPECT_GT(best->placements.size(), 0);

    // Check no exceptions occurred
}
```

---

## DELIVERABLES

### Phase 1 (Week 1)
- ✅ noFitPolygon() loop closure riabilitato
- ✅ timeRatio aggiunto a config e fitness
- ✅ Spacing validation e self-intersection detection
- ✅ Unit tests per tutti i fix
- ✅ Regression test suite

### Phase 2 (Weeks 2-3)
- ✅ searchStartPoint fallback rimosso
- ✅ Holes orientation corretto
- ✅ Merged length integrato in placement strategy
- ✅ Integration tests

### Phase 3 (Week 4)
- ✅ Thread synchronization robusta
- ✅ NFP output validation
- ✅ Polygon repair utility
- ✅ Performance benchmarks
- ✅ Full integration testing

### Documentation
- ✅ README.md aggiornato con timeRatio
- ✅ CHANGELOG.md con tutte le modifiche
- ✅ Testing guide in `tests/TESTING.md`
- ✅ Known issues documentati

---

## RISK MANAGEMENT

### Rischi Critici

| Rischio | Probabilità | Impatto | Mitigazione |
|---------|-------------|---------|-------------|
| Loop closure crea nuovi bug | ALTA | ALTO | Testing estensivo, fallback a MinkowskiSum |
| timeRatio value sbagliato | MEDIA | MEDIO | Test con vari valori, confronto con JS |
| Self-intersection detection lenta | MEDIA | MEDIO | Caching, ottimizzazione algoritmo |
| Race conditions persistono | BASSA | ALTO | Code review, thread sanitizer |

### Contingency Plans

**Se loop closure detection causa problemi**:
- Mantenere come opzione configurabile (flag debug)
- Usare MinkowskiSum come primary, noFitPolygon come fallback

**Se timeRatio non migliora risultati**:
- Testare con valori diversi (0.2, 0.5, 0.8)
- Parametro configurabile per utente

**Se self-intersection detection troppo lenta**:
- Usare solo per debug
- Implementare fast check (bounding box overlap)

**Se race conditions persistono**:
- Serializzare completamente il processing
- Rimuovere parallelizzazione temporaneamente

---

## TIMELINE & MILESTONES

### Week 1
- **Day 1-2**: Task 1.1 (noFitPolygon loop closure)
- **Day 3**: Task 1.2 (timeRatio)
- **Day 4-5**: Task 1.3 (spacing validation)

**Milestone 1**: Critical fixes completati, basic testing passed

### Week 2
- **Day 6-7**: Task 2.1 (searchStartPoint)
- **Day 8-9**: Task 2.2 (holes orientation)

**Milestone 2**: High priority fixes completati

### Week 3
- **Day 10-12**: Task 2.3 (merged length in strategy)

**Milestone 3**: Placement optimization completato

### Week 4
- **Day 13-15**: Task 3.1 (thread sync)
- **Day 16**: Task 3.2 (NFP validation)
- **Day 17**: Task 3.3 (polygon repair testing)

**Milestone 4**: All fixes completati

### Week 5 (Buffer)
- **Day 18-20**: Integration testing
- **Day 21**: Performance testing
- **Day 22**: Documentation

**Final Milestone**: Release candidate pronto

---

## ACCEPTANCE CRITERIA

### Functional
- ✅ noFitPolygon() genera NFP corretti al 100%
- ✅ Spacing non genera eccezioni Boost
- ✅ Fitness calculation include timeRatio
- ✅ Nessun crash durante restart

### Performance
- ✅ NFP calculation < 100ms per coppia (media)
- ✅ Nesting 10 parts su 1 sheet < 30 secondi
- ✅ Memory usage < 500MB per sessione

### Quality
- ✅ Unit test coverage > 80%
- ✅ Tutti i regression tests passano
- ✅ Nessun race condition rilevato (thread sanitizer)
- ✅ Valgrind no memory leaks

### Documentation
- ✅ README aggiornato
- ✅ CHANGELOG completo
- ✅ Testing guide presente
- ✅ Code comments per parti critiche

---

## CONCLUSIONE

Questo piano di sviluppo fornisce una roadmap dettagliata per risolvere tutti i problemi critici identificati nell'analisi. La prioritizzazione garantisce che i problemi più impattanti vengano risolti per primi, permettendo di avere una versione funzionante nel più breve tempo possibile.

**Stima Totale**: 15-22 giorni di sviluppo effettivo (3-4 settimane calendario)

**Team Richiesto**: 2 developers (1 senior, 1 mid-level)

**Prossimi Passi**:
1. Review del piano con team
2. Allocazione risorse
3. Setup ambiente di testing
4. Inizio implementazione Fase 1

---

**Fine Piano di Sviluppo**
