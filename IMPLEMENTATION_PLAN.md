# Piano di Implementazione: Fix Minkowski Sum NFP Calculation

**Data**: 2025-11-20
**Basato su**: MINKOWSKI_NFP_ANALYSIS.md
**Stato**: Ready for Implementation

---

## PANORAMICA

Questo piano implementa le correzioni identificate nell'analisi per risolvere il crash di Minkowski sum e allineare l'implementazione C++ con il codice originale funzionante (minkowski.cc).

### Priorità delle Modifiche

```
┌─────────────────────────────────────────────────┐
│ PHASE 1: FIX CRITICI (MUST HAVE)               │
│ ⚠️ Risolve crash + allinea con originale        │
│ Tempo stimato: 1 giorno                         │
│ Rischio: BASSO (segue codice funzionante)      │
└─────────────────────────────────────────────────┘
         ↓
┌─────────────────────────────────────────────────┐
│ PHASE 2: ROBUSTEZZA (SHOULD HAVE)              │
│ 🛡️ Error handling + validazione                │
│ Tempo stimato: 1 giorno                         │
│ Rischio: BASSO (difensivo)                      │
└─────────────────────────────────────────────────┘
         ↓
┌─────────────────────────────────────────────────┐
│ PHASE 3: SIMPLIFICAZIONE (NICE TO HAVE)        │
│ 🎯 Ottimizzazione opzionale                     │
│ Tempo stimato: 0.5 giorni                       │
│ Rischio: MEDIO (nuovo algoritmo)                │
└─────────────────────────────────────────────────┘
         ↓
┌─────────────────────────────────────────────────┐
│ PHASE 4: TESTING (MUST HAVE)                   │
│ ✅ Verifica correzioni                          │
│ Tempo stimato: 0.5 giorni                       │
│ Rischio: BASSO                                  │
└─────────────────────────────────────────────────┘
         ↓
┌─────────────────────────────────────────────────┐
│ PHASE 5: DOCUMENTAZIONE (SHOULD HAVE)          │
│ 📝 Commenti + README                            │
│ Tempo stimato: 0.5 giorni                       │
│ Rischio: BASSO                                  │
└─────────────────────────────────────────────────┘

TOTALE: 3.5 giorni (minimo 2 giorni per Phase 1+2)
```

---

## PHASE 1: FIX CRITICI ⚠️ PRIORITÀ MASSIMA

### Obiettivo
Far funzionare il codice senza crash, allineando con minkowski.cc.

### Task 1.1: Rimuovi Aggressive Cleaning

**File**: `deepnest-cpp/src/nfp/MinkowskiSum.cpp`

**Locazione**: Linee 162-234 (funzione `toBoostIntPolygon`)

**Modifica 1**: Sostituisci la funzione `cleanPoints` complessa

**PRIMA** (linee 184-234):
```cpp
auto cleanPoints = [](std::vector<IntPoint>& pts) -> bool {
    if (pts.size() < 3) return false;

    std::vector<IntPoint> cleaned;
    cleaned.reserve(pts.size());

    // STEP 1: Remove exact duplicates only
    for (size_t i = 0; i < pts.size(); ++i) {
        size_t next = (i + 1) % pts.size();
        if (pts[i].x() != pts[next].x() || pts[i].y() != pts[next].y()) {
            cleaned.push_back(pts[i]);
        }
    }

    if (cleaned.size() < 3) return false;

    // STEP 2: Remove only truly collinear points
    std::vector<IntPoint> final;
    final.reserve(cleaned.size());

    for (size_t i = 0; i < cleaned.size(); ++i) {
        size_t prev = (i == 0) ? cleaned.size() - 1 : i - 1;
        size_t next = (i + 1) % cleaned.size();

        int dx1 = cleaned[i].x() - cleaned[prev].x();
        int dy1 = cleaned[i].y() - cleaned[prev].y();
        int dx2 = cleaned[next].x() - cleaned[i].x();
        int dy2 = cleaned[next].y() - cleaned[i].y();

        long long cross = static_cast<long long>(dx1) * dy2 -
                         static_cast<long long>(dy1) * dx2;

        if (cross != 0) {
            final.push_back(cleaned[i]);
        }
    }

    if (final.size() >= 3) {
        pts = std::move(final);
        return true;
    }

    return false;
};

// Clean and validate
if (!cleanPoints(points)) {
    return IntPolygonWithHoles();
}
```

**DOPO**:
```cpp
// MINIMAL CLEANING: Remove only consecutive exact duplicate points
// This matches minkowski.cc behavior (no cleaning at all)
auto removeConsecutiveDuplicates = [](std::vector<IntPoint>& pts) {
    if (pts.size() < 2) return;

    std::vector<IntPoint> cleaned;
    cleaned.reserve(pts.size());
    cleaned.push_back(pts[0]);

    for (size_t i = 1; i < pts.size(); ++i) {
        const IntPoint& prev = cleaned.back();
        const IntPoint& curr = pts[i];

        // Keep point if it's different from previous
        if (prev.x() != curr.x() || prev.y() != curr.y()) {
            cleaned.push_back(curr);
        }
    }

    // Check if last point equals first
    if (cleaned.size() > 1) {
        const IntPoint& first = cleaned.front();
        const IntPoint& last = cleaned.back();
        if (first.x() == last.x() && first.y() == last.y()) {
            cleaned.pop_back();
        }
    }

    pts = std::move(cleaned);
};

// Apply minimal cleaning
removeConsecutiveDuplicates(points);

// Check minimum points (but don't fail - let Boost handle it)
if (points.size() < 3) {
    std::cerr << "WARNING: Polygon has < 3 points after removing duplicates ("
              << points.size() << " points). This may cause issues.\n";
    // Continue anyway - Boost may handle it
}
```

**Modifica 2**: Applica lo stesso per holes (linee 252-278)

**PRIMA**:
```cpp
if (cleanPoints(holePoints)) {
    try {
        IntPolygon holePoly;
        set_points(holePoly, holePoints.begin(), holePoints.end());
        holes.push_back(holePoly);
    }
    catch (...) {
        continue;
    }
}
```

**DOPO**:
```cpp
removeConsecutiveDuplicates(holePoints);

if (holePoints.size() >= 3) {
    try {
        IntPolygon holePoly;
        set_points(holePoly, holePoints.begin(), holePoints.end());
        holes.push_back(holePoly);
    }
    catch (...) {
        std::cerr << "WARNING: Failed to create hole polygon, skipping\n";
        continue;
    }
} else {
    std::cerr << "WARNING: Hole has < 3 points, skipping\n";
}
```

**Testing**:
```bash
# Compila
cd deepnest-cpp/build
cmake --build . --config Release

# Test con PolygonExtractor
./tests/Release/PolygonExtractor.exe ../tests/testdata/test__SVGnest-output_clean.svg

# Aspettativa: Minkowski dovrebbe funzionare per più coppie
```

---

### Task 1.2: Cambia da Rounding a Truncation

**File**: `deepnest-cpp/src/nfp/MinkowskiSum.cpp`

**Locazione**: Linee 171-172 (in `toBoostIntPolygon`)

**PRIMA**:
```cpp
for (const auto& p : poly.points) {
    // Use proper rounding instead of truncation to reduce quantization errors
    int x = static_cast<int>(std::round(scale * p.x));
    int y = static_cast<int>(std::round(scale * p.y));
    points.push_back(IntPoint(x, y));
}
```

**DOPO**:
```cpp
for (const auto& p : poly.points) {
    // CRITICAL: Use truncation (not rounding) to match minkowski.cc behavior
    // The original implementation uses (int) cast which truncates toward zero
    int x = static_cast<int>(scale * p.x);
    int y = static_cast<int>(scale * p.y);
    points.push_back(IntPoint(x, y));
}
```

**Stessa modifica per holes** (linee 261-262):

**PRIMA**:
```cpp
int x = static_cast<int>(std::round(scale * p.x));
int y = static_cast<int>(std::round(scale * p.y));
```

**DOPO**:
```cpp
// CRITICAL: Truncation, not rounding
int x = static_cast<int>(scale * p.x);
int y = static_cast<int>(scale * p.y);
```

**Testing**:
```bash
# Compila e testa
cmake --build . --config Release
./tests/Release/PolygonExtractor.exe ../tests/testdata/test__SVGnest-output_clean.svg

# Verifica output numerico è più vicino a minkowski.cc
```

---

### Task 1.3: Aggiungi Reference Point Shift

**File**: `deepnest-cpp/src/nfp/MinkowskiSum.cpp`

**Locazione**: Funzione `calculateNFP` (linee 388-505)

**Modifica 1**: Salva il punto di riferimento

**DOPO la linea 391** `if (A.points.empty() || B.points.empty()) ...`, aggiungi:

```cpp
// CRITICAL: Save reference point from B[0] for later shift
// This matches minkowski.cc lines 286-298 (xshift, yshift)
double xshift = 0.0;
double yshift = 0.0;
if (!B.points.empty()) {
    xshift = B.points[0].x;
    yshift = B.points[0].y;
}
```

**Modifica 2**: Applica shift ai risultati

**PRIMA della linea 502** `return nfps;`, aggiungi:

```cpp
// CRITICAL: Apply reference point shift to all NFP results
// This matches minkowski.cc lines 319-320, 480-481
// NFP coordinates are relative to B[0], not absolute
for (auto& nfp : nfps) {
    // Shift outer boundary
    for (auto& pt : nfp.points) {
        pt.x += xshift;
        pt.y += yshift;
    }

    // Shift holes (children)
    for (auto& child : nfp.children) {
        for (auto& pt : child.points) {
            pt.x += xshift;
            pt.y += yshift;
        }
    }
}
```

**Commento importante**:
```cpp
// NOTE: The shift is applied because Minkowski difference is computed as A ⊖ B,
// where B is negated (lines 409-425). The resulting NFP is in the coordinate
// system where B's origin is at (0,0). To place it correctly in the original
// coordinate system, we shift by B[0].
```

**Testing**:
```bash
# Compila
cmake --build . --config Release

# Testa con visualizzazione
./tests/Release/PolygonExtractor.exe ../tests/testdata/test__SVGnest-output_clean.svg

# Verifica SVG output: il poligono NFP dovrebbe essere posizionato correttamente
# rispetto al poligono B (non centrato su origine)
```

---

### Task 1.4: Selezione Area Massima (Multiple NFPs)

**File**: `deepnest-cpp/src/nfp/MinkowskiSum.cpp`

**Locazione**: Prima del `return nfps;` (linea 502)

**AGGIUNGI** dopo la sezione shift (Task 1.3), prima del return:

```cpp
// CRITICAL: If multiple NFPs are returned, choose the one with largest area
// This matches background.js lines 666-673 (largest area selection)
if (nfps.size() > 1) {
    size_t maxIndex = 0;
    double maxArea = 0.0;

    for (size_t i = 0; i < nfps.size(); ++i) {
        // Calculate absolute area (use std::abs for signed area)
        double area = 0.0;
        const auto& pts = nfps[i].points;
        for (size_t j = 0; j < pts.size(); ++j) {
            size_t next = (j + 1) % pts.size();
            area += (pts[j].x + pts[next].x) * (pts[j].y - pts[next].y);
        }
        area = std::abs(area * 0.5);

        if (area > maxArea) {
            maxArea = area;
            maxIndex = i;
        }
    }

    // Return only the largest
    std::cerr << "INFO: Multiple NFPs generated (" << nfps.size()
              << "), selecting largest with area " << maxArea << "\n";

    Polygon largestNfp = nfps[maxIndex];
    return {largestNfp};
}
```

**Testing**:
```bash
# Compila
cmake --build . --config Release

# Testa con poligoni che generano multipli NFP
./tests/Release/PolygonExtractor.exe ../tests/testdata/test__SVGnest-output_clean.svg

# Aspettativa: Log dovrebbe mostrare "selecting largest" quando applicabile
```

---

### Task 1.5: Verifica Compilation

**Test di compilazione completa**:

```bash
cd deepnest-cpp/build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Se ci sono errori di compilazione, correggili prima di procedere
```

**Output atteso**:
```
Building CXX object src/CMakeFiles/deepnest.dir/nfp/MinkowskiSum.cpp.obj
...
[100%] Built target deepnest
[100%] Built target TestApplication
[100%] Built target PolygonExtractor
```

---

### Task 1.6: Testing Funzionalità Base

**Test 1**: PolygonExtractor con 5 coppie note

```bash
cd deepnest-cpp/build/tests/Release
./PolygonExtractor.exe ../../testdata/test__SVGnest-output_clean.svg
```

**Aspettativa**:
```
Testing pair: A(id=X) vs B(id=Y)
=== Testing Minkowski Sum ===
✅ SUCCESS: 1 NFP polygon(s) generated
   NFP[0]: N points
   Saved visualization: polygon_pair_X_Y_minkowski.svg

=== Testing Orbital Tracing ===
❌ FAILED: Orbital tracing returned empty result
```

**Orbital può ancora fallire** - va bene per ora. L'importante è:
- ✅ Minkowski sum funziona
- ✅ Genera almeno 1 NFP
- ✅ Non crasha

**Test 2**: TestApplication con nesting completo

```bash
cd deepnest-cpp/build/tests/Release
./TestApplication.exe --test ../../testdata/test__SVGnest-output_clean.svg --generations 5
```

**Aspettativa**:
- ✅ Non crasha
- ⚠️ Può esserci qualche warning "Boost.Polygon get() failed" (accettabile)
- ✅ Placement completo con parti posizionate su sheet

---

## PHASE 2: ROBUSTEZZA 🛡️

### Obiettivo
Gestire gracefully i fallimenti e fornire diagnostiche utili.

### Task 2.1: Migliora Error Handling

**File**: `deepnest-cpp/src/nfp/MinkowskiSum.cpp`

**Locazione 1**: Funzione `fromBoostPolygonSet` (linee 343-367)

**SOSTITUISCI** il try-catch esistente:

```cpp
std::vector<Polygon> fromBoostPolygonSet(
    const IntPolygonSet& polySet, double scale) {

    std::vector<IntPolygonWithHoles> boostPolygons;

    try {
        polySet.get(boostPolygons);
    }
    catch (const std::runtime_error& e) {
        // Handle specific "invalid comparator" error from Boost scanline
        std::string msg(e.what());
        if (msg.find("invalid comparator") != std::string::npos) {
            std::cerr << "\n=== BOOST POLYGON SCANLINE FAILURE ===\n";
            std::cerr << "Boost.Polygon's scanline algorithm failed with 'invalid comparator'.\n";
            std::cerr << "This is a known issue with degenerate or near-degenerate geometries.\n";
            std::cerr << "The NFP for this polygon pair will be EMPTY.\n";
            std::cerr << "Impact: This part pair cannot be placed adjacent during nesting.\n";
            std::cerr << "Workaround: Simplify polygon geometry or use larger scale factor.\n";
            std::cerr << "=====================================\n\n";
        } else {
            std::cerr << "ERROR: Boost.Polygon get() failed: " << e.what() << "\n";
        }
        return std::vector<Polygon>();
    }
    catch (const std::logic_error& e) {
        std::cerr << "ERROR: Logic error in Boost.Polygon: " << e.what() << "\n";
        return std::vector<Polygon>();
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR: Exception in Boost.Polygon get(): " << e.what() << "\n";
        return std::vector<Polygon>();
    }
    catch (...) {
        std::cerr << "ERROR: Unknown exception in Boost.Polygon get()\n";
        return std::vector<Polygon>();
    }

    // ... resto della funzione ...
}
```

**Locazione 2**: Funzione `calculateNFP` (linee 459-500)

**MIGLIORA** il catch esistente:

```cpp
catch (const std::exception& e) {
    std::cerr << "\n=== MINKOWSKI SUM CALCULATION FAILED ===\n";
    std::cerr << "Error: " << e.what() << "\n";
    std::cerr << "Polygon A (id=" << A.id << "): " << A.points.size() << " points\n";
    std::cerr << "Polygon B (id=" << B.id << "): " << B.points.size() << " points\n";
    std::cerr << "Scale factor: " << scale << "\n";

    // Print first few points for debugging
    std::cerr << "A points (first 3): ";
    for (size_t i = 0; i < std::min(size_t(3), A.points.size()); ++i) {
        std::cerr << "(" << A.points[i].x << "," << A.points[i].y << ") ";
    }
    std::cerr << "\n";

    std::cerr << "B points (first 3): ";
    for (size_t i = 0; i < std::min(size_t(3), B.points.size()); ++i) {
        std::cerr << "(" << B.points[i].x << "," << B.points[i].y << ") ";
    }
    std::cerr << "\n";

    std::cerr << "Returning empty NFP - placement will skip this pair.\n";
    std::cerr << "========================================\n\n";

    return std::vector<Polygon>();
}
```

---

### Task 2.2: Pre-Validation Input

**File nuovo**: `deepnest-cpp/include/deepnest/nfp/PolygonValidator.h`

```cpp
#ifndef DEEPNEST_POLYGON_VALIDATOR_H
#define DEEPNEST_POLYGON_VALIDATOR_H

#include "../core/Polygon.h"
#include <string>
#include <limits>
#include <cmath>

namespace deepnest {

class PolygonValidator {
public:
    struct ValidationResult {
        bool valid;
        std::string message;

        ValidationResult(bool v, const std::string& msg = "")
            : valid(v), message(msg) {}

        operator bool() const { return valid; }
    };

    /**
     * Validate polygon before passing to Boost.Polygon
     */
    static ValidationResult validate(const Polygon& poly) {
        // Check minimum points
        if (poly.points.size() < 3) {
            return {false, "Polygon has < 3 points"};
        }

        // Check for infinity or NaN
        for (size_t i = 0; i < poly.points.size(); ++i) {
            const auto& p = poly.points[i];
            if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
                return {false, "Point " + std::to_string(i) +
                               " has non-finite coordinates (" +
                               std::to_string(p.x) + ", " +
                               std::to_string(p.y) + ")"};
            }
        }

        // Check for extremely large coordinates
        const double MAX_COORD = 1e6;  // 1 million units
        for (size_t i = 0; i < poly.points.size(); ++i) {
            const auto& p = poly.points[i];
            if (std::abs(p.x) > MAX_COORD || std::abs(p.y) > MAX_COORD) {
                return {false, "Point " + std::to_string(i) +
                               " has extremely large coordinates (" +
                               std::to_string(p.x) + ", " +
                               std::to_string(p.y) + ") > " +
                               std::to_string(MAX_COORD)};
            }
        }

        // Check for zero area
        double area = 0.0;
        for (size_t i = 0; i < poly.points.size(); ++i) {
            size_t j = (i + 1) % poly.points.size();
            area += (poly.points[i].x + poly.points[j].x) *
                    (poly.points[i].y - poly.points[j].y);
        }
        area = std::abs(area * 0.5);

        if (area < 1e-6) {  // Essentially zero
            return {false, "Polygon has near-zero area: " + std::to_string(area)};
        }

        return {true, "Valid"};
    }
};

} // namespace deepnest

#endif // DEEPNEST_POLYGON_VALIDATOR_H
```

**Integrazione in MinkowskiSum.cpp**:

```cpp
#include "../../include/deepnest/nfp/PolygonValidator.h"

std::vector<Polygon> MinkowskiSum::calculateNFP(
    const Polygon& A,
    const Polygon& B,
    bool inner) {

    // Validate input BEFORE processing
    auto validA = PolygonValidator::validate(A);
    if (!validA) {
        std::cerr << "ERROR: Invalid polygon A (id=" << A.id << "): "
                  << validA.message << "\n";
        return std::vector<Polygon>();
    }

    auto validB = PolygonValidator::validate(B);
    if (!validB) {
        std::cerr << "ERROR: Invalid polygon B (id=" << B.id << "): "
                  << validB.message << "\n";
        return std::vector<Polygon>();
    }

    // ... resto del codice ...
}
```

---

### Task 2.3: Post-Validation Output

**File**: `deepnest-cpp/src/nfp/MinkowskiSum.cpp`

**Aggiungi alla fine di `calculateNFP`, prima del return finale**:

```cpp
// VALIDATION: Check NFP results are valid
for (size_t i = 0; i < nfps.size(); ++i) {
    auto valid = PolygonValidator::validate(nfps[i]);
    if (!valid) {
        std::cerr << "WARNING: Generated NFP[" << i << "] is invalid: "
                  << valid.message << "\n";
        std::cerr << "  This NFP will be removed from results.\n";
        // Remove invalid NFP
        nfps.erase(nfps.begin() + i);
        --i;  // Adjust index
    }
}

if (nfps.empty()) {
    std::cerr << "WARNING: All generated NFPs were invalid. Returning empty.\n";
}

return nfps;
```

---

## PHASE 3: SIMPLIFICAZIONE (OPZIONALE) 🎯

### Obiettivo
Implementare Douglas-Peucker polygon simplification (come simplify.js).

### Task 3.1: Implementa Douglas-Peucker

**File nuovo**: `deepnest-cpp/include/deepnest/geometry/PolygonSimplifier.h`

```cpp
#ifndef DEEPNEST_POLYGON_SIMPLIFIER_H
#define DEEPNEST_POLYGON_SIMPLIFIER_H

#include "../core/Polygon.h"
#include <vector>
#include <cmath>

namespace deepnest {

/**
 * Ramer-Douglas-Peucker polygon simplification
 * Based on simplify.js from original DeepNest
 */
class PolygonSimplifier {
public:
    /**
     * Simplify polygon using Douglas-Peucker algorithm
     *
     * @param poly Polygon to simplify
     * @param tolerance Distance tolerance (default 2.0 units, as in DeepNest)
     * @param highQuality If true, skip radial distance pre-pass
     * @return Simplified polygon
     */
    static Polygon simplify(const Polygon& poly,
                           double tolerance = 2.0,
                           bool highQuality = false);

private:
    // Square distance between two points
    static double getSqDist(const Point& p1, const Point& p2) {
        double dx = p1.x - p2.x;
        double dy = p1.y - p2.y;
        return dx * dx + dy * dy;
    }

    // Square distance from point to segment
    static double getSqSegDist(const Point& p,
                              const Point& p1,
                              const Point& p2);

    // Radial distance simplification
    static std::vector<Point> simplifyRadialDist(
        const std::vector<Point>& points,
        double sqTolerance);

    // Douglas-Peucker recursive step
    static void simplifyDPStep(const std::vector<Point>& points,
                              size_t first,
                              size_t last,
                              double sqTolerance,
                              std::vector<Point>& simplified);

    // Douglas-Peucker main
    static std::vector<Point> simplifyDouglasPeucker(
        const std::vector<Point>& points,
        double sqTolerance);
};

} // namespace deepnest

#endif // DEEPNEST_POLYGON_SIMPLIFIER_H
```

**File nuovo**: `deepnest-cpp/src/geometry/PolygonSimplifier.cpp`

```cpp
#include "../../include/deepnest/geometry/PolygonSimplifier.h"
#include <algorithm>

namespace deepnest {

double PolygonSimplifier::getSqSegDist(const Point& p,
                                      const Point& p1,
                                      const Point& p2) {
    double x = p1.x;
    double y = p1.y;
    double dx = p2.x - x;
    double dy = p2.y - y;

    if (dx != 0.0 || dy != 0.0) {
        double t = ((p.x - x) * dx + (p.y - y) * dy) / (dx * dx + dy * dy);

        if (t > 1.0) {
            x = p2.x;
            y = p2.y;
        } else if (t > 0.0) {
            x += dx * t;
            y += dy * t;
        }
    }

    dx = p.x - x;
    dy = p.y - y;

    return dx * dx + dy * dy;
}

std::vector<Point> PolygonSimplifier::simplifyRadialDist(
    const std::vector<Point>& points,
    double sqTolerance) {

    if (points.size() <= 2) return points;

    Point prevPoint = points[0];
    std::vector<Point> newPoints;
    newPoints.push_back(prevPoint);

    for (size_t i = 1; i < points.size(); ++i) {
        const Point& point = points[i];

        if (getSqDist(point, prevPoint) > sqTolerance) {
            newPoints.push_back(point);
            prevPoint = point;
        }
    }

    // Always keep last point
    if (prevPoint.x != points.back().x || prevPoint.y != points.back().y) {
        newPoints.push_back(points.back());
    }

    return newPoints;
}

void PolygonSimplifier::simplifyDPStep(const std::vector<Point>& points,
                                      size_t first,
                                      size_t last,
                                      double sqTolerance,
                                      std::vector<Point>& simplified) {
    double maxSqDist = sqTolerance;
    size_t index = first;  // Will be updated if a point is found

    for (size_t i = first + 1; i < last; ++i) {
        double sqDist = getSqSegDist(points[i], points[first], points[last]);

        if (sqDist > maxSqDist) {
            index = i;
            maxSqDist = sqDist;
        }
    }

    if (maxSqDist > sqTolerance) {
        if (index - first > 1) {
            simplifyDPStep(points, first, index, sqTolerance, simplified);
        }
        simplified.push_back(points[index]);
        if (last - index > 1) {
            simplifyDPStep(points, index, last, sqTolerance, simplified);
        }
    }
}

std::vector<Point> PolygonSimplifier::simplifyDouglasPeucker(
    const std::vector<Point>& points,
    double sqTolerance) {

    if (points.size() <= 2) return points;

    size_t last = points.size() - 1;
    std::vector<Point> simplified;
    simplified.push_back(points[0]);

    simplifyDPStep(points, 0, last, sqTolerance, simplified);

    simplified.push_back(points[last]);

    return simplified;
}

Polygon PolygonSimplifier::simplify(const Polygon& poly,
                                   double tolerance,
                                   bool highQuality) {
    if (poly.points.size() <= 2) {
        return poly;
    }

    double sqTolerance = tolerance * tolerance;

    // Apply radial distance first (unless high quality)
    std::vector<Point> points = poly.points;
    if (!highQuality) {
        points = simplifyRadialDist(points, sqTolerance);
    }

    // Apply Douglas-Peucker
    points = simplifyDouglasPeucker(points, sqTolerance);

    // Create result polygon
    Polygon result;
    result.id = poly.id;
    result.points = points;
    result.children = poly.children;  // Keep children unchanged

    return result;
}

} // namespace deepnest
```

---

### Task 3.2: Integra con NFPCalculator

**File**: `deepnest-cpp/include/deepnest/nfp/NFPCalculator.h`

**Aggiungi parametro**:

```cpp
class NFPCalculator {
public:
    /**
     * @param simplify If true, simplify NFP after calculation
     * @param simplifyTolerance Tolerance for simplification (default 2.0)
     */
    static std::vector<Polygon> computeNFP(
        Polygon* A,
        Polygon* B,
        bool inside = false,
        bool simplify = false,
        double simplifyTolerance = 2.0);

    // ... altri metodi ...
};
```

**File**: `deepnest-cpp/src/nfp/NFPCalculator.cpp`

**Modifica `computeNFP`**:

```cpp
#include "../../include/deepnest/geometry/PolygonSimplifier.h"

std::vector<Polygon> NFPCalculator::computeNFP(
    Polygon* A,
    Polygon* B,
    bool inside,
    bool simplify,
    double simplifyTolerance) {

    // ... calcolo NFP normale ...

    std::vector<Polygon> result = MinkowskiSum::calculateNFP(*A, *B, inside);

    // Apply simplification if requested
    if (simplify && !result.empty()) {
        for (auto& nfp : result) {
            nfp = PolygonSimplifier::simplify(nfp, simplifyTolerance);
        }
    }

    return result;
}
```

---

### Task 3.3: Configura da DeepNestConfig

**File**: `deepnest-cpp/include/deepnest/config/DeepNestConfig.h`

**Aggiungi membri**:

```cpp
class DeepNestConfig {
public:
    // ... membri esistenti ...

    /**
     * If true, simplify NFP polygons using Douglas-Peucker algorithm
     * This reduces polygon complexity but may affect placement accuracy
     * Default: false (as in original DeepNest)
     */
    bool simplifyNFP = false;

    /**
     * Tolerance for NFP simplification (in units)
     * Larger value = more aggressive simplification
     * Default: 2.0 (as in original DeepNest simplify.js)
     */
    double nfpSimplifyTolerance = 2.0;
};
```

---

## PHASE 4: TESTING ✅

### Task 4.1: Unit Tests

**File nuovo**: `deepnest-cpp/tests/MinkowskiSumTests.cpp`

```cpp
#include <gtest/gtest.h>
#include "../include/deepnest/nfp/MinkowskiSum.h"
#include "../include/deepnest/core/Polygon.h"

using namespace deepnest;

TEST(MinkowskiSumTest, SimpleSquares) {
    // Two squares: A=100x100, B=50x50
    Polygon A;
    A.id = 1;
    A.points = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};

    Polygon B;
    B.id = 2;
    B.points = {{0, 0}, {50, 0}, {50, 50}, {0, 50}};

    auto nfps = MinkowskiSum::calculateNFP(A, B, false);

    ASSERT_EQ(nfps.size(), 1) << "Should return exactly 1 NFP";
    EXPECT_GE(nfps[0].points.size(), 3) << "NFP should have at least 3 points";

    // NFP should be a rectangle larger than A
    double area = 0.0;
    const auto& pts = nfps[0].points;
    for (size_t i = 0; i < pts.size(); ++i) {
        size_t j = (i + 1) % pts.size();
        area += (pts[i].x + pts[j].x) * (pts[i].y - pts[j].y);
    }
    area = std::abs(area * 0.5);

    EXPECT_GT(area, 10000.0) << "NFP area should be > 10000";
}

TEST(MinkowskiSumTest, ReferencePointShift) {
    // Test that NFP is shifted by B[0]
    Polygon A;
    A.points = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};

    Polygon B;
    B.points = {{10, 20}, {60, 20}, {60, 70}, {10, 70}};

    auto nfps = MinkowskiSum::calculateNFP(A, B, false);

    ASSERT_FALSE(nfps.empty());

    // Find bounding box of NFP
    double minX = nfps[0].points[0].x;
    double minY = nfps[0].points[0].y;

    for (const auto& p : nfps[0].points) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
    }

    // NFP should be shifted by approximately (10, 20)
    EXPECT_NEAR(minX, 10.0, 5.0) << "NFP should be shifted in X by ~10";
    EXPECT_NEAR(minY, 20.0, 5.0) << "NFP should be shifted in Y by ~20";
}

TEST(MinkowskiSumTest, EmptyInput) {
    Polygon A;  // Empty
    Polygon B;
    B.points = {{0, 0}, {50, 0}, {50, 50}};

    auto nfps = MinkowskiSum::calculateNFP(A, B, false);

    EXPECT_TRUE(nfps.empty()) << "Empty input should return empty NFP";
}

TEST(MinkowskiSumTest, TooFewPoints) {
    Polygon A;
    A.points = {{0, 0}, {100, 0}};  // Only 2 points

    Polygon B;
    B.points = {{0, 0}, {50, 0}, {50, 50}};

    auto nfps = MinkowskiSum::calculateNFP(A, B, false);

    EXPECT_TRUE(nfps.empty()) << "< 3 points should return empty NFP";
}
```

**Integrazione con CMake**:

Aggiungi in `tests/CMakeLists.txt`:
```cmake
# Google Test per Unit Tests
find_package(GTest)
if(GTest_FOUND)
    add_executable(MinkowskiSumTests MinkowskiSumTests.cpp)
    target_link_libraries(MinkowskiSumTests PRIVATE deepnest GTest::GTest GTest::Main)

    # Register as CTest
    include(GoogleTest)
    gtest_discover_tests(MinkowskiSumTests)
endif()
```

**Esecuzione**:
```bash
cd deepnest-cpp/build
ctest --output-on-failure
```

---

### Task 4.2: Integration Test

**Test con file SVG completo**:

```bash
cd deepnest-cpp/build/tests/Release

# Test 1: PolygonExtractor (targeted test)
./PolygonExtractor.exe ../../testdata/test__SVGnest-output_clean.svg

# Aspettativa:
# - Minkowski ✅ per tutte le 5 coppie (o almeno 80%)
# - Orbital può fallire (OK per ora)

# Test 2: TestApplication (full nesting)
./TestApplication.exe --test ../../testdata/test__SVGnest-output_clean.svg --generations 5

# Aspettativa:
# - NO CRASH (critico!)
# - Placement completo
# - Può avere warnings per alcuni NFP
# - Fitness ragionevole (< 1.5 * numero parti)
```

---

### Task 4.3: Visual Verification

**Verifica output SVG**:

1. Apri i file SVG generati da PolygonExtractor:
   ```
   deepnest-cpp/build/tests/Release/polygon_pair_X_Y_minkowski.svg
   ```

2. Controlla visualmente:
   - ✅ NFP è un poligono chiuso
   - ✅ NFP circonda il poligono B
   - ✅ NFP è posizionato correttamente (non sull'origine)
   - ✅ Nessuna self-intersection evidente

3. Confronta con output di DeepNest JavaScript (se disponibile)

---

## PHASE 5: DOCUMENTAZIONE 📝

### Task 5.1: Inline Comments

**Aggiungi commenti dettagliati al codice modificato**:

```cpp
// CRITICAL SECTION: Minkowski Sum Calculation
// This implementation follows minkowski.cc exactly to ensure
// compatibility with the proven-working original code.
//
// Key differences from typical implementations:
// 1. Uses TRUNCATION not ROUNDING for integer conversion
// 2. Uses MINIMAL CLEANING (only exact duplicates)
// 3. Applies REFERENCE POINT SHIFT to output
// 4. Selects LARGEST AREA when multiple NFPs generated
//
// DO NOT modify this section without consulting MINKOWSKI_NFP_ANALYSIS.md
```

---

### Task 5.2: README Update

**File**: `deepnest-cpp/README.md`

**Aggiungi sezione dopo "Building"**:

```markdown
## NFP Calculation Implementation

DeepNest C++ uses Boost.Polygon for Minkowski sum calculation to compute
No-Fit Polygons (NFPs) for part placement optimization.

### Algorithm Overview

1. **Dynamic scaling**: Coordinates are scaled to integers using `scale = 0.1 * INT_MAX / maxBounds`
2. **Integer conversion**: Uses truncation (`(int)` cast) not rounding to match original
3. **Minimal cleaning**: Removes only consecutive exact duplicate points
4. **Boost convolution**: Calls `convolve_two_polygon_sets()` for Minkowski sum
5. **Result extraction**: Handles Boost scanline failures gracefully
6. **Reference shift**: Applies offset by B[0] to position NFP correctly
7. **Area selection**: If multiple NFPs, selects largest by area

### Known Limitations

- **Boost scanline failures**: ~10% of polygon pairs may fail with "invalid comparator"
  error due to degenerate geometries. These return empty NFP and are skipped.

- **No simplification by default**: NFPs are not simplified unless explicitly enabled
  via `config.simplifyNFP = true`.

- **Integer precision**: Very small features may be lost in integer conversion.
  Minimum feature size ≈ 1 / scale ≈ 0.0001 units.

### Troubleshooting

**Problem**: High failure rate (> 20%) of NFP calculations

**Solutions**:
1. Check polygon validity (min 3 points, no NaN, reasonable coordinates)
2. Simplify input polygons before nesting
3. Enable NFP simplification: `config.simplifyNFP = true`

**Problem**: NFPs positioned incorrectly

**Solution**: Verify reference point shift is applied (should match B[0])

**Problem**: Self-intersecting NFPs

**Solution**: This indicates degenerate input geometry. Simplify inputs.

### References

- Implementation based on: `minkowski.cc` (original DeepNest C++ plugin)
- Analysis document: `MINKOWSKI_NFP_ANALYSIS.md`
- Boost.Polygon documentation: https://www.boost.org/doc/libs/release/libs/polygon/
```

---

### Task 5.3: Update Analysis Document

**File**: `MINKOWSKI_NFP_ANALYSIS.md`

**Aggiungi sezione finale**:

```markdown
## IMPLEMENTATION STATUS

**Date**: [DATA]

### Completed Tasks

- [x] Phase 1: Fix Critici
  - [x] Task 1.1: Rimozione aggressive cleaning
  - [x] Task 1.2: Truncation invece di rounding
  - [x] Task 1.3: Reference point shift
  - [x] Task 1.4: Selezione area massima
  - [x] Task 1.5: Compilation verificata
  - [x] Task 1.6: Testing funzionalità base

- [ ] Phase 2: Robustezza
  - [ ] Task 2.1: Improved error handling
  - [ ] Task 2.2: Pre-validation
  - [ ] Task 2.3: Post-validation

- [ ] Phase 3: Simplificazione (opzionale)
- [ ] Phase 4: Testing completo
- [ ] Phase 5: Documentazione

### Test Results

**PolygonExtractor** (5 test pairs):
- Minkowski Success Rate: X/5 (X%)
- Orbital Success Rate: Y/5 (Y%)
- Visual Inspection: [PASS/FAIL]

**TestApplication** (full nesting, 154 parts):
- Crash: [YES/NO]
- NFP Failure Rate: X/Y (X%)
- Placement Success: [PASS/FAIL]
- Fitness: [VALUE]

### Known Issues

1. [Issue description]
2. [Issue description]

### Next Steps

1. [Next action]
2. [Next action]
```

---

## APPENDICE: TROUBLESHOOTING

### Problema 1: Compilation Errors

**Error**: `'PolygonValidator' is not a member of 'deepnest'`

**Solution**: Assicurati di aver aggiunto `PolygonValidator.h` e incluso:
```cpp
#include "../../include/deepnest/nfp/PolygonValidator.h"
```

---

### Problema 2: Link Errors

**Error**: `undefined reference to PolygonSimplifier::simplify`

**Solution**: Aggiungi `PolygonSimplifier.cpp` a `src/CMakeLists.txt`:
```cmake
add_library(deepnest
    # ... existing files ...
    geometry/PolygonSimplifier.cpp
)
```

---

### Problema 3: Test Failures

**Test fails**: `SimpleSquares test fails with empty NFP`

**Debug**:
1. Abilita verbose logging in `calculateNFP`
2. Controlla se `polySet.get()` lancia eccezione
3. Verifica scale factor non è troppo grande/piccolo
4. Visualizza input polygons come SVG

---

### Problema 4: High NFP Failure Rate

**Observation**: > 30% of NFP calculations fail

**Possible causes**:
1. Input polygons have degenerate geometry (zero-length edges, etc.)
2. Scale factor calculation issues
3. Boost.Polygon version incompatibility

**Solutions**:
1. Pre-process inputs with simplification (tolerance ~0.1)
2. Adjust scale factor formula
3. Verify Boost version matches original (1.89+)

---

## METRICHE DI SUCCESSO

### Criteri di Accettazione - Phase 1

✅ **PASS** se:
- [ ] Compilation success (no errors, no warnings)
- [ ] PolygonExtractor: Minkowski ≥ 80% success rate (4/5)
- [ ] TestApplication: NO CRASH su full nesting
- [ ] TestApplication: NFP failure rate < 20%
- [ ] Visual verification: NFPs positioned correctly

❌ **FAIL** se:
- [ ] Compilation fails
- [ ] Crash durante nesting
- [ ] Minkowski success rate < 50%

### Criteri di Accettazione - Phase 1+2

✅ **PASS** se:
- [ ] Tutti i criteri Phase 1
- [ ] Error messages informativi (no silent failures)
- [ ] Pre-validation cattura invalid inputs
- [ ] Post-validation rimuove invalid NFPs

### Criteri di Accettazione - Completo

✅ **PASS** se:
- [ ] Tutti i criteri Phase 1+2
- [ ] Unit tests: 100% pass
- [ ] Integration tests: 100% pass
- [ ] Documentation completa
- [ ] Code review pass

---

**Fine Piano - Ready for Implementation - 2025-11-20**
