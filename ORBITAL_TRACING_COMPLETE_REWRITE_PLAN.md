# ORBITAL TRACING - PIANO COMPLETO DI REIMPLEMENTAZIONE

**Cliente**: Richiesto lavoro completo, no quick fix
**Obiettivo**: Implementazione completa e corretta dell'orbital tracing dal JavaScript
**Stima**: 3-4 ore di lavoro concentrato

---

## ANALISI COMPARATIVA: JAVASCRIPT vs C++ ATTUALE

### JAVASCRIPT (Completo - 290 righe)

**Struttura**:
1. **Start Point Selection** (inside vs outside logic)
2. **Touching Detection** (3 types):
   - Type 0: Vertex-to-vertex contact
   - Type 1: B vertex on A edge
   - Type 2: A vertex on B edge
3. **Vector Generation** (4-6 vectors per touching):
   - For vertex-to-vertex: 4 vectors (prev/next on both A and B)
   - For edge contacts: 2 vectors
4. **Vector Filtering**:
   - Skip zero vectors
   - Skip backtracking (dot product < 0)
   - Check cross product magnitude
5. **Slide Distance Calculation**:
   - Call polygonSlideDistance()
   - If null or too large → use vector length
6. **Vector Selection**:
   - Choose vector with **MAXIMUM** slide distance (not minimum!)
7. **Loop Closure Detection**:
   - Check return to start point
   - Check return to any previous NFP point
8. **Marked Vertices** (to avoid revisiting)

### C++ ATTUALE (Incompleto - 50 righe)

**Cosa manca**:
- ❌ No touching detection (3 types)
- ❌ No vector generation from touching
- ❌ No backtracking prevention
- ❌ No proper vector selection (uses wrong logic)
- ❌ No slide distance fallback
- ❌ No proper loop detection
- ❌ Uses wrong angle-based selection

**Root Problem**: Tentativo di semplificare troppo, perdendo la logica essenziale

---

## PIANO DI IMPLEMENTAZIONE COMPLETA

### FASE 1: Strutture Dati (30 min)

**File**: `deepnest-cpp/include/deepnest/geometry/OrbitalTypes.h` (NEW)

```cpp
namespace deepnest {

// Types of touching contact between polygons
enum class TouchingType {
    VERTEX_VERTEX = 0,  // Vertex of A touches vertex of B
    VERTEX_ON_EDGE_A = 1, // Vertex of B lies on edge of A
    VERTEX_ON_EDGE_B = 2  // Vertex of A lies on edge of B
};

// Represents a touching contact point
struct TouchingContact {
    TouchingType type;
    int indexA;    // Index in polygon A
    int indexB;    // Index in polygon B
};

// Represents a translation vector for sliding
struct TranslationVector {
    double x, y;
    Point* startVertex;  // Which vertex starts this edge
    Point* endVertex;    // Which vertex ends this edge

    double length() const {
        return std::sqrt(x*x + y*y);
    }

    void normalize() {
        double len = length();
        if (len > TOL) {
            x /= len;
            y /= len;
        }
    }
};

} // namespace deepnest
```

---

### FASE 2: Helper Functions (1 ora)

**File**: `deepnest-cpp/src/geometry/OrbitalHelpers.cpp` (NEW)

**Funzione 1: Detect Touching Contacts**
```cpp
std::vector<TouchingContact> findTouchingContacts(
    const std::vector<Point>& A,
    const std::vector<Point>& B,
    const Point& offsetB)
{
    std::vector<TouchingContact> touching;

    for (size_t i = 0; i < A.size(); i++) {
        size_t nexti = (i + 1) % A.size();

        for (size_t j = 0; j < B.size(); j++) {
            size_t nextj = (j + 1) % B.size();

            Point Bj_translated(B[j].x + offsetB.x, B[j].y + offsetB.y);
            Point Bnextj_translated(B[nextj].x + offsetB.x, B[nextj].y + offsetB.y);

            // Type 0: Vertex-to-vertex
            if (almostEqualPoints(A[i], Bj_translated)) {
                touching.push_back({TouchingType::VERTEX_VERTEX, (int)i, (int)j});
            }
            // Type 1: B vertex on A edge
            else if (onSegment(A[i], A[nexti], Bj_translated)) {
                touching.push_back({TouchingType::VERTEX_ON_EDGE_A, (int)nexti, (int)j});
            }
            // Type 2: A vertex on B edge
            else if (onSegment(Bj_translated, Bnextj_translated, A[i])) {
                touching.push_back({TouchingType::VERTEX_ON_EDGE_B, (int)i, (int)nextj});
            }
        }
    }

    return touching;
}
```

**Funzione 2: Generate Translation Vectors**
```cpp
std::vector<TranslationVector> generateTranslationVectors(
    const TouchingContact& touch,
    const std::vector<Point>& A,
    const std::vector<Point>& B,
    const Point& offsetB)
{
    std::vector<TranslationVector> vectors;

    // Get adjacent vertices
    int prevAindex = (touch.indexA - 1 + A.size()) % A.size();
    int nextAindex = (touch.indexA + 1) % A.size();
    int prevBindex = (touch.indexB - 1 + B.size()) % B.size();
    int nextBindex = (touch.indexB + 1) % B.size();

    const Point& vertexA = A[touch.indexA];
    const Point& prevA = A[prevAindex];
    const Point& nextA = A[nextAindex];
    const Point& vertexB = B[touch.indexB];
    const Point& prevB = B[prevBindex];
    const Point& nextB = B[nextBindex];

    if (touch.type == TouchingType::VERTEX_VERTEX) {
        // Type 0: Generate 4 vectors

        // vA1: from vertexA to prevA
        vectors.push_back({
            prevA.x - vertexA.x,
            prevA.y - vertexA.y,
            &vertexA,
            &prevA
        });

        // vA2: from vertexA to nextA
        vectors.push_back({
            nextA.x - vertexA.x,
            nextA.y - vertexA.y,
            &vertexA,
            &nextA
        });

        // vB1: INVERTED (B moves opposite direction)
        vectors.push_back({
            vertexB.x - prevB.x,
            vertexB.y - prevB.y,
            &prevB,
            &vertexB
        });

        // vB2: INVERTED
        vectors.push_back({
            vertexB.x - nextB.x,
            vertexB.y - nextB.y,
            &nextB,
            &vertexB
        });
    }
    else if (touch.type == TouchingType::VERTEX_ON_EDGE_A) {
        // Type 1: B vertex on A edge, generate 2 vectors
        Point Bj_translated(vertexB.x + offsetB.x, vertexB.y + offsetB.y);

        vectors.push_back({
            vertexA.x - Bj_translated.x,
            vertexA.y - Bj_translated.y,
            &prevA,
            &vertexA
        });

        vectors.push_back({
            prevA.x - Bj_translated.x,
            prevA.y - Bj_translated.y,
            &vertexA,
            &prevA
        });
    }
    else if (touch.type == TouchingType::VERTEX_ON_EDGE_B) {
        // Type 2: A vertex on B edge, generate 2 vectors
        Point Bj_translated(vertexB.x + offsetB.x, vertexB.y + offsetB.y);
        Point prevB_translated(prevB.x + offsetB.x, prevB.y + offsetB.y);

        vectors.push_back({
            vertexA.x - Bj_translated.x,
            vertexA.y - Bj_translated.y,
            &prevB,
            &vertexB
        });

        vectors.push_back({
            vertexA.x - prevB_translated.x,
            vertexA.y - prevB_translated.y,
            &vertexB,
            &prevB
        });
    }

    return vectors;
}
```

**Funzione 3: Filter Backtracking Vectors**
```cpp
bool isBacktracking(
    const TranslationVector& vec,
    const TranslationVector* prevVector)
{
    if (!prevVector) return false;

    // Skip zero vectors
    if (almostEqual(vec.x, 0.0) && almostEqual(vec.y, 0.0)) {
        return true;
    }

    // Check if pointing backwards: dot product < 0
    double dotProduct = vec.x * prevVector->x + vec.y * prevVector->y;
    if (dotProduct < 0) {
        // Normalize to unit vectors
        double vecLen = vec.length();
        double prevLen = prevVector->length();

        if (vecLen < TOL || prevLen < TOL) return false;

        TranslationVector unitVec = vec;
        unitVec.x /= vecLen;
        unitVec.y /= vecLen;

        TranslationVector unitPrev = *prevVector;
        unitPrev.x /= prevLen;
        unitPrev.y /= prevLen;

        // Check cross product magnitude
        double crossMag = std::abs(unitVec.x * unitPrev.y - unitVec.y * unitPrev.x);
        if (crossMag < 0.0001) {
            return true;  // Vectors are collinear and opposite
        }
    }

    return false;
}
```

---

### FASE 3: Main Orbital Loop Rewrite (1.5 ore)

**File**: `deepnest-cpp/src/geometry/GeometryUtil.cpp`

**Completamente riscrivere `noFitPolygon()`**:

```cpp
std::vector<std::vector<Point>> noFitPolygon(
    const std::vector<Point>& A,
    const std::vector<Point>& B,
    bool inside,
    bool searchEdges)
{
    if (A.size() < 3 || B.size() < 3) {
        return {};
    }

    std::vector<std::vector<Point>> nfpList;

    // Get initial start point
    std::optional<Point> startOpt;

    if (!inside) {
        // OUTSIDE: Use heuristic - bottom of A to top of B
        size_t minAindex = 0;
        double minAy = A[0].y;
        for (size_t i = 1; i < A.size(); i++) {
            if (A[i].y < minAy) {
                minAy = A[i].y;
                minAindex = i;
            }
        }

        size_t maxBindex = 0;
        double maxBy = B[0].y;
        for (size_t i = 1; i < B.size(); i++) {
            if (B[i].y > maxBy) {
                maxBy = B[i].y;
                maxBindex = i;
            }
        }

        startOpt = Point(
            A[minAindex].x - B[maxBindex].x,
            A[minAindex].y - B[maxBindex].y
        );
    }
    else {
        // INSIDE: No reliable heuristic, use search
        startOpt = searchStartPoint(A, B, true, {});
    }

    // Main loop: find all NFPs starting from different points
    while (startOpt.has_value()) {
        Point offsetB = startOpt.value();

        std::vector<Point> nfp;

        // Reference point: B[0] translated by offset
        Point reference(B[0].x + offsetB.x, B[0].y + offsetB.y);
        Point startPoint = reference;

        nfp.push_back(reference);

        TranslationVector* prevVector = nullptr;
        int counter = 0;
        int maxIterations = 10 * (A.size() + B.size());

        while (counter < maxIterations) {
            counter++;

            // STEP 1: Find all touching contacts
            auto touchingList = findTouchingContacts(A, B, offsetB);

            if (touchingList.empty()) {
                LOG("No touching contacts found, breaking");
                break;
            }

            // STEP 2: Generate translation vectors from all touches
            std::vector<TranslationVector> allVectors;
            for (const auto& touch : touchingList) {
                auto vectors = generateTranslationVectors(touch, A, B, offsetB);
                allVectors.insert(allVectors.end(), vectors.begin(), vectors.end());
            }

            // STEP 3: Filter and select best vector
            TranslationVector* bestVector = nullptr;
            double maxDistance = 0.0;

            for (auto& vec : allVectors) {
                // Skip backtracking vectors
                if (isBacktracking(vec, prevVector)) {
                    continue;
                }

                // Calculate slide distance
                auto slideOpt = polygonSlideDistance(A, B, Point(vec.x, vec.y), true);
                double slideDistance;

                double vecLength2 = vec.x * vec.x + vec.y * vec.y;

                if (!slideOpt.has_value() || slideOpt.value() * slideOpt.value() > vecLength2) {
                    // Use vector length if slide returns null or too large
                    slideDistance = std::sqrt(vecLength2);
                }
                else {
                    slideDistance = slideOpt.value();
                }

                // Select vector with MAXIMUM distance
                if (slideDistance > maxDistance) {
                    maxDistance = slideDistance;
                    bestVector = &vec;
                }
            }

            if (!bestVector || almostEqual(maxDistance, 0.0)) {
                LOG("No valid translation vector found, breaking");
                nfp.clear();  // Invalid NFP
                break;
            }

            // Mark vertices as used
            bestVector->startVertex->marked = true;
            bestVector->endVertex->marked = true;

            prevVector = bestVector;

            // STEP 4: Trim vector if needed
            double vecLength2 = bestVector->x * bestVector->x + bestVector->y * bestVector->y;
            if (maxDistance * maxDistance < vecLength2 &&
                !almostEqual(maxDistance * maxDistance, vecLength2)) {
                double scale = maxDistance / std::sqrt(vecLength2);
                bestVector->x *= scale;
                bestVector->y *= scale;
            }

            // STEP 5: Move reference point
            reference.x += bestVector->x;
            reference.y += bestVector->y;

            // STEP 6: Check loop closure
            if (almostEqualPoints(reference, startPoint)) {
                break;  // Completed loop
            }

            // Check if we've returned to any previous point
            bool looped = false;
            for (size_t i = 0; i < nfp.size() - 1; i++) {
                if (almostEqualPoints(reference, nfp[i])) {
                    looped = true;
                    break;
                }
            }

            if (looped) {
                break;  // Completed loop
            }

            // Add point to NFP
            nfp.push_back(reference);

            // Update offset for next iteration
            offsetB.x += bestVector->x;
            offsetB.y += bestVector->y;
        }

        // Add NFP if valid
        if (nfp.size() >= 3) {
            nfpList.push_back(nfp);
        }

        if (!searchEdges) {
            break;  // Only get first NFP
        }

        // Search for next start point
        startOpt = searchStartPoint(A, B, inside, nfpList);
    }

    return nfpList;
}
```

---

### FASE 4: Helper Implementations (30 min)

**Funzione `onSegment`** (se non esiste già):
```cpp
bool onSegment(const Point& p1, const Point& p2, const Point& p) {
    // Check if p is on segment p1-p2
    double cross = (p.y - p1.y) * (p2.x - p1.x) - (p.x - p1.x) * (p2.y - p1.y);

    if (!almostEqual(cross, 0.0)) {
        return false;  // Not collinear
    }

    double dotProduct = (p.x - p1.x) * (p2.x - p1.x) + (p.y - p1.y) * (p2.y - p1.y);
    double squaredLength = (p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y);

    return dotProduct >= -TOL && dotProduct <= squaredLength + TOL;
}
```

---

### FASE 5: Testing & Debug (30 min)

**Test 1**: Simple rectangles
**Test 2**: Polygon pairs from PolygonExtractor
**Test 3**: Full 154-part nesting

**Debug Output**:
- Numero di touching contacts
- Numero di vettori generati
- Vettore selezionato e distanza
- Punti NFP generati
- Motivo di uscita dal loop

---

## STIMA TEMPORALE TOTALE

| Fase | Descrizione | Tempo | Priorità |
|------|-------------|-------|----------|
| 1 | Strutture dati | 30 min | ALTA |
| 2 | Helper functions | 60 min | ALTA |
| 3 | Main orbital loop | 90 min | CRITICA |
| 4 | Helper implementations | 30 min | MEDIA |
| 5 | Testing & debug | 30 min | ALTA |

**TOTALE**: ~4 ore di lavoro concentrato

---

## DELIVERABLES

1. ✅ Implementazione completa orbital tracing
2. ✅ 100% fedele al JavaScript originale
3. ✅ Test completi con polygon pairs
4. ✅ Documentazione del codice
5. ✅ Log dettagliati per debug
6. ✅ Commit organizzati per fase

---

## INIZIO IMPLEMENTAZIONE

Iniziamo con la FASE 1: Strutture dati.

Procedo?
