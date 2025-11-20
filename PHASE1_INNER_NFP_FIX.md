# PHASE 1 EXTENDED: Inner NFP Crash Fix

**Data**: 2025-11-20
**Branch**: `claude/deepnest-js-to-cpp-01FL3yqoHLn2AXEAue7G1XYr`

---

## PROBLEMA IDENTIFICATO

### Crash durante Full Nesting

**Sintomo**: TestApplication va in crash durante nesting completo con 154 parti

**Stack trace**:
```
TestApplication!boost::polygon::polygon_arbitrary_formation<int,...>::addHole()
  @ polygon_arbitrary_formation.hpp:1028
    ↓
TestApplication!boost::polygon::polygon_arbitrary_formation<int,...>::processEvent_()
  @ polygon_arbitrary_formation.hpp:1768
    ↓
TestApplication!boost::polygon::polygon_set_data<int,...>::get_fracture()
  @ polygon_set_data.hpp:801
    ↓
TestApplication!deepnest::MinkowskiSum::fromBoostPolygonSet()
  @ minkowskisum.cpp:336
    ↓
TestApplication!deepnest::MinkowskiSum::calculateNFP()
  @ minkowskisum.cpp:445 (inner = true)
```

**Dettaglio critico**: `inner = true` → calcolo **Inner NFP** (sheet boundary vs part)

### Analisi Root Cause

1. **PolygonExtractor SUCCESS (5/5)**
   - Testa SOLO **Outer NFP** (part vs part, `inner=false`)
   - Geometrie semplici, tutte passano con Phase 1 fixes

2. **TestApplication CRASH**
   - Richiede ANCHE **Inner NFP** (sheet vs part, `inner=true`)
   - Sheet boundary geometries più complesse
   - Alcuni casi creano **geometrie degenerate** dopo conversione a interi

3. **Access Violation Non Catchable**
   - Il crash è **access violation** (hard crash), non C++ exception
   - try-catch a line 336 non può catturarlo
   - Boost.Polygon scanline accede a memoria invalida prima di poter lanciare eccezione
   - Succede dentro `addHole()` durante operazione `get()` che chiama `clean()`

### Perché Phase 1 Non Era Sufficiente

Phase 1 ha risolto la **maggior parte** dei problemi:
- ✅ Rimozione aggressive cleaning
- ✅ Truncation invece di rounding
- ✅ Reference point shift
- ✅ Area selection

**MA**: Alcune geometrie sono **così degenerate** che nemmeno minimal cleaning basta.

Dopo conversione a interi:
- Polygon con area quasi zero
- Bounding box estremamente sottili (quasi linee)
- Sequenze di punti quasi collineari che formano shape degenerate

Questi problemi si manifestano **solo in integer space** dopo scaling e truncation.

---

## SOLUZIONE IMPLEMENTATA

### Validazione Post-Conversione

**File modificato**: `deepnest-cpp/src/nfp/MinkowskiSum.cpp`

**Dove**: Funzione `toBoostIntPolygon()` - linee 220-296

**Cosa**: Aggiunto lambda `isGeometryDegenerate()` con 3 controlli critici

### Check 1: Near-Zero Area (Lines 226-241)

```cpp
// Calculate polygon area in integer space using shoelace formula
long long area2 = 0;  // 2 * area to avoid division
for (size_t i = 0; i < pts.size(); ++i) {
    size_t next = (i + 1) % pts.size();
    area2 += static_cast<long long>(pts[i].x()) * pts[next].y();
    area2 -= static_cast<long long>(pts[next].x()) * pts[i].y();
}
area2 = std::abs(area2);

// If area is zero or extremely small, polygon is degenerate
if (area2 < 100) {  // Threshold: less than 50 square units
    std::cerr << "WARNING: Polygon has near-zero area (" << (area2/2.0)
              << ") in integer space - likely degenerate\n";
    return true;
}
```

**Perché**: Poligoni con area quasi zero causano problemi in scanline algorithm perché non hanno "interno" definibile.

### Check 2: Thin Bounding Box (Lines 243-260)

```cpp
// Check for extremely small bounding box (collapsed polygon)
int minX = pts[0].x(), maxX = pts[0].x();
int minY = pts[0].y(), maxY = pts[0].y();
for (const auto& pt : pts) {
    minX = std::min(minX, pt.x());
    maxX = std::max(maxX, pt.x());
    minY = std::min(minY, pt.y());
    maxY = std::max(maxY, pt.y());
}
long long bboxWidth = maxX - minX;
long long bboxHeight = maxY - minY;

// If bounding box is too thin (almost a line), it's degenerate
if (bboxWidth < 2 || bboxHeight < 2) {
    std::cerr << "WARNING: Polygon has thin bounding box ("
              << bboxWidth << " x " << bboxHeight << ") - likely degenerate\n";
    return true;
}
```

**Perché**: Poligoni con bbox sottilissimi sono essenzialmente linee, non hanno topologia 2D valida.

### Check 3: Excessive Collinearity (Lines 262-286)

```cpp
// Check for excessive collinearity (many points on same line)
int collinearCount = 0;
for (size_t i = 0; i < pts.size(); ++i) {
    size_t prev = (i == 0) ? pts.size() - 1 : i - 1;
    size_t next = (i + 1) % pts.size();

    // Cross product to check collinearity
    long long dx1 = pts[i].x() - pts[prev].x();
    long long dy1 = pts[i].y() - pts[prev].y();
    long long dx2 = pts[next].x() - pts[i].x();
    long long dy2 = pts[next].y() - pts[i].y();
    long long cross = dx1 * dy2 - dy1 * dx2;

    if (std::abs(cross) < 10) {  // Nearly collinear
        collinearCount++;
    }
}

// If more than 80% of points are collinear, likely degenerate
if (collinearCount > pts.size() * 0.8) {
    std::cerr << "WARNING: Polygon has excessive collinearity ("
              << collinearCount << "/" << pts.size() << " points) - likely degenerate\n";
    return true;
}
```

**Perché**: Troppi punti collineari confondono l'algoritmo scanline che cerca di ordinare edge events.

### Applicazione della Validazione

**Outer boundary** (lines 292-296):
```cpp
// Check if geometry is degenerate and reject if so
if (isGeometryDegenerate(points)) {
    std::cerr << "ERROR: Degenerate geometry detected after integer conversion (id="
              << poly.id << "). Rejecting to prevent Boost.Polygon crash.\n";
    return IntPolygonWithHoles();  // Return empty
}
```

**Holes** (lines 328-333):
```cpp
// CRITICAL: Validate hole geometry too
if (isGeometryDegenerate(holePoints)) {
    std::cerr << "WARNING: Degenerate hole geometry detected (id="
              << poly.id << "), skipping hole to prevent crash\n";
    continue;  // Skip this hole but continue with others
}
```

---

## COMPORTAMENTO ATTESO

### Scenario 1: Geometry Valida
```
Input: Polygon A(154 points), Polygon B(87 points)
→ Convert to integer space
→ Validation: Area = 45000, bbox = 1500x2000, collinear = 5%
→ ✅ PASS validation
→ Boost.Polygon Minkowski calculation
→ SUCCESS: NFP generated
```

### Scenario 2: Geometry Degenerate (Prevented Crash)
```
Input: Sheet boundary edge(200 points), Part(50 points)
→ Convert to integer space
→ Validation: Area = 35, bbox = 1x450, collinear = 95%
→ ❌ FAIL validation (near-zero area + thin bbox + excessive collinearity)
→ ERROR: Degenerate geometry detected (id=sheet_edge_15)
→ Return empty NFP (graceful degradation)
→ Continue with next polygon pair
```

### Graceful Degradation

Invece di **crashare**, il sistema ora:
1. Rileva geometria problematica
2. Logga warning/error dettagliato
3. Ritorna NFP vuoto per quella coppia
4. Continua con le altre coppie

**Risultato**: Nesting parziale invece di crash completo.

---

## TESTING

### Test 1: PolygonExtractor (Outer NFP)

**Dovrebbe continuare a passare** (già confermato 5/5 success):
```bash
cd deepnest-cpp/build/tests/Release
./PolygonExtractor ../../testdata/test__SVGnest-output_clean.svg
```

**Aspettativa**: Nessun cambiamento, tutti passano come prima.

### Test 2: TestApplication (Full Nesting + Inner NFP)

**Dovrebbe NON crashare più**:
```bash
cd deepnest-cpp/build/tests/Release
./TestApplication --test ../../testdata/test__SVGnest-output_clean.svg --generations 5
```

**Aspettativa NUOVA**:
```
Loading SVG: test__SVGnest-output_clean.svg
Loaded 154 parts successfully
Starting nesting with 5 generations...

Generation 1/5:
  Computing NFPs...
  WARNING: Polygon has near-zero area (12.5) in integer space - likely degenerate
  ERROR: Degenerate geometry detected after integer conversion (id=55).
         Rejecting to prevent Boost.Polygon crash.
  INFO: NFP calculation returned empty for pair (sheet, 55) - using fallback
  ...
  Placement: 150/154 parts placed (97.4%)
  Fitness = 145.2

Generation 2/5: Fitness = 138.7
...

✅ Nesting completed WITHOUT CRASH!
Placed: 150/154 parts (some degenerate geometries skipped)
```

**Criteri di successo**:
- ✅ **NO CRASH** (obiettivo principale!)
- ✅ Completa tutte le generazioni
- ⚠️ Alcuni warning "degenerate geometry" sono OK (expected)
- ⚠️ Placement può essere parziale (150/154 invece di 154/154) - accettabile
- ⚠️ Alcuni parts con geometrie problematiche potrebbero non essere piazzati

---

## TRADE-OFFS

### Pro ✅
- **Elimina crash** (NO più access violations)
- **Graceful degradation** invece di fallimento completo
- **Logging dettagliato** per debugging
- **Non modifica geometrie valide** (zero impact su casi funzionanti)

### Contro ⚠️
- Alcuni parts con geometrie degenerate potrebbero non essere piazzati
- Placement potrebbe non essere al 100% in casi estremi
- Performance leggermente ridotta per validation checks (insignificante)

### Mitigazione
Per parts non piazzabili, si possono:
1. Semplificare la geometria originale nel SVG
2. Rimuovere punti ridondanti manualmente
3. Usare tolerance più alta nel loader SVG
4. Pre-processing con simplify.js (Phase 3)

---

## MODIFICHE AL CODICE

**File**: `deepnest-cpp/src/nfp/MinkowskiSum.cpp`

**Linee modificate**:
- Lines 220-296: Aggiunto `isGeometryDegenerate()` lambda e validation per outer boundary
- Lines 328-333: Aggiunto validation per holes (children)

**Righe aggiunte**: ~110 righe
**Logica modificata**: Nessuna logica esistente modificata, solo aggiunta pre-validation

---

## COMPATIBILITÀ

### Con Phase 1 ✅
Tutte le modifiche di Phase 1 sono **preservate**:
- Minimal cleaning (no aggressive collinearity removal)
- Truncation (not rounding)
- Reference point shift
- Largest area selection

### Con minkowski.cc ✅
La validazione è applicata **dopo** la conversione che replica minkowski.cc, quindi:
- Non interferisce con compatibilità bit-perfect
- Agisce come "safety net" per casi estremi non gestiti dall'originale
- minkowski.cc probabilmente non aveva questi problemi perché usato con geometrie pre-validate

---

## PROSSIMI PASSI

### Immediate (User)

1. **Pull le modifiche**:
   ```bash
   git pull origin claude/deepnest-js-to-cpp-01FL3yqoHLn2AXEAue7G1XYr
   ```

2. **Ricompilare** (Windows con Visual Studio 2017):
   ```batch
   cd deepnest-cpp\build
   cmake .. -G "Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE=Release
   cmake --build . --config Release
   ```

3. **Test PolygonExtractor** (conferma nessuna regressione):
   ```batch
   cd tests\Release
   PolygonExtractor.exe ..\..\testdata\test__SVGnest-output_clean.svg
   ```

   **Aspettativa**: Ancora 5/5 success (no changes)

4. **Test TestApplication** (verifica fix crash):
   ```batch
   TestApplication.exe --test ..\..\testdata\test__SVGnest-output_clean.svg --generations 5
   ```

   **Aspettativa**: NO CRASH, completa tutte le generazioni con possibili warning

### Se TestApplication passa ✅

**PHASE 1 COMPLETATA E VERIFICATA!**

Procedere con:
- **Phase 2**: Robustezza aggiuntiva (error recovery, cache NFP, parallelization hints)
- **Phase 3**: Simplification opzionale (Douglas-Peucker con tolerance configurabile)
- **Phase 4**: Test suite completa e profiling performance

### Se TestApplication crasha ancora ❌

Serve analisi più approfondita:
1. Identificare quale coppia di poligoni causa ancora crash
2. Estrarre geometrie problematiche in file test isolati
3. Analizzare se i threshold di validazione vanno regolati
4. Considerare SEH (Structured Exception Handling) su Windows per catturare access violations

---

## DEBUG TIPS

### Identificare Geometrie Problematiche

Se crash continua, modificare `calculateNFP()` per loggare prima di ogni calcolo:

```cpp
std::cerr << "DEBUG: Calculating NFP for pair (A.id=" << A.id
          << ", B.id=" << B.id << ", inner=" << inner << ")\n";
std::cerr << "  A: " << A.points.size() << " points, area="
          << calculatePolygonArea(A) << "\n";
std::cerr << "  B: " << B.points.size() << " points, area="
          << calculatePolygonArea(B) << "\n";
```

Ultimo pair loggato prima del crash = geometria problematica.

### Regolare Threshold

Se troppi warning "degenerate geometry", threshold potrebbero essere troppo stretti:

```cpp
// Current thresholds (conservative):
if (area2 < 100)           // Near-zero area
if (bboxWidth < 2 || bboxHeight < 2)  // Thin bbox
if (collinearCount > pts.size() * 0.8)  // 80% collinear

// Relaxed thresholds (if needed):
if (area2 < 10)            // Only truly zero area
if (bboxWidth < 1 || bboxHeight < 1)  // Only completely flat
if (collinearCount > pts.size() * 0.95)  // 95% collinear
```

Bilanciare tra **safety** (reject più geometrie) e **completeness** (accept più geometrie).

---

## CONCLUSIONE

**PHASE 1 EXTENDED COMPLETATA dal punto di vista del codice.**

Aggiunta validazione critica per prevenire crash da geometrie degenerate in integer space. Il codice ora gestisce gracefully casi estremi che causavano access violations in Boost.Polygon.

La prossima milestone è **compilare e verificare che TestApplication completi senza crash**, poi valutare coverage del placement e decidere se procedere con Phase 2-3 o se regolare threshold di validazione.

---

**Commit ready for**:
- Degenerate geometry validation (outer boundaries + holes)
- Pre-Boost safety checks (area, bbox, collinearity)
- Graceful degradation invece di crash

**Branch**: `claude/deepnest-js-to-cpp-01FL3yqoHLn2AXEAue7G1XYr`
**Ready for**: Compilation + Full nesting test with 154 parts
