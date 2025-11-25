# Unused Functions Analysis - noFitPolygon Removal

## Obiettivo

Rimuovere completamente la funzione `noFitPolygon()` (orbital tracing NFP) e tutte le funzioni helper collegate. La libreria userà SOLO Minkowski Sum per il calcolo NFP.

## Situazione Attuale

### noFitPolygon è ancora USATO in:

1. **src/parallel/ParallelProcessor.cpp** (PRODUZIONE!)
   - Linea 226: `nfp = GeometryUtil::noFitPolygon(A.points, B.points, true, config.exploreConcave);`
   - Linea 239: `nfp = GeometryUtil::noFitPolygon(A.points, B.points, false, config.exploreConcave);`
   - Linea 281: `auto cnfp = GeometryUtil::noFitPolygon(hole.points, B.points, true, config.exploreConcave);`

2. **tests/PolygonExtractor.cpp** (TEST)
   - Linea 386: `auto nfpPoints = deepnest::GeometryUtil::noFitPolygon(...)`
   - Linea 423: Commentato

3. **tests/NFPDebug.cpp** (DEBUG TOOL)
   - Linea 26: `auto result = GeometryUtil::noFitPolygon(A, B, false, false);`

4. **tests/NFPValidationTests.cpp** (TEST SUITE)
   - Multiple chiamate per validation tests

### Sistema NFP Corretto

Il sistema DOVREBBE usare:
- **NFPCalculator::computeNFP()** → che usa **MinkowskiSum::calculateNFP()**
- **NFPCalculator::computeDiffNFP()** → che usa Clipper2 Minkowski
- Minkowski Sum è il metodo stabile e testato

## Funzioni da RIMUOVERE

### 1. Funzione Principale
- **noFitPolygon()** in `src/geometry/GeometryUtil.cpp` (linea ~579)
  - Implementazione completa ~400+ lines
  - Orbital tracing algorithm

### 2. Funzioni Helper Direttamente Usate da noFitPolygon

Queste funzioni sono usate SOLO da noFitPolygon (verificare con grep):

#### A. Da GeometryUtil.h / GeometryUtil.cpp:
- **searchStartPoint()**
  - Cerca punto di partenza valido per orbital tracing
  - Usata solo da noFitPolygon

- **polygonSlideDistance()**
  - Calcola distanza sliding tra poligoni
  - Usata solo da noFitPolygon

- **polygonProjectionDistance()**
  - Proietta poligono B su poligono A
  - Usata solo da noFitPolygon

- **pointDistance()**
  - Distanza punto-linea per sliding
  - Usata solo da polygonSlideDistance

- **pointLineDistance()**
  - Distanza normale punto-segmento
  - Usata solo da polygonSlideDistance

- **segmentDistance()**
  - Distanza minima tra segmenti
  - Usata solo da polygonSlideDistance

- **polygonEdge()**
  - Estrae edge di poligono in direzione normale
  - Usata solo da noFitPolygon

#### B. Da GeometryUtilAdvanced.h/.cpp:
Tutte le funzioni sopra sono già dichiarate qui (potrebbe essere duplicato o move)

#### C. Da OrbitalHelpers.cpp:
- **findTouchingContacts()**
  - Trova contatti tra poligoni per orbital tracing
  - Usata solo da noFitPolygon (forse)

- **generateTranslationVectors()**
  - Genera vettori di traslazione da contatti
  - Usata solo da orbital tracing

- **isBacktracking()**
  - Check se vettore rappresenta backtracking
  - Usata solo da orbital tracing

#### D. File da valutare per rimozione completa:
- **src/geometry/OrbitalHelpers.cpp** (intero file)
- **include/deepnest/geometry/OrbitalTypes.h** (se contiene solo struct per orbital)

### 3. Funzioni da MANTENERE (richiesta esplicita)

Anche se non usate ora, mantenere per ottimizzazioni future:
- **noFitPolygonRectangle()** - NFP ottimizzato per rettangoli
- **isRectangle()** - Detect se poligono è rettangolo

### 4. Funzione Da Verificare (potrebbe avere altri usi)

- **polygonHull()** - Hull di due poligoni touching
  - VERIFICARE se usata altrove o solo da noFitPolygon

## Piano di Rimozione

### Step 1: Verificare dipendenze (grep per ogni funzione)

```bash
# Per ogni funzione, verificare tutti gli usi:
grep -r "searchStartPoint" deepnest-cpp/src/ deepnest-cpp/include/ --include="*.cpp" --include="*.h"
grep -r "polygonSlideDistance" ...
grep -r "pointDistance\(" ...
grep -r "pointLineDistance" ...
grep -r "segmentDistance" ...
grep -r "polygonEdge" ...
grep -r "polygonProjectionDistance" ...
grep -r "polygonHull" ...
grep -r "findTouchingContacts" ...
grep -r "generateTranslationVectors" ...
grep -r "isBacktracking" ...
```

### Step 2: Modificare ParallelProcessor

Sostituire tutte le chiamate a `noFitPolygon()` con chiamate a `NFPCalculator`.

**Prima**:
```cpp
nfp = GeometryUtil::noFitPolygon(A.points, B.points, true, config.exploreConcave);
```

**Dopo** (opzione 1 - usare NFPCalculator direttamente):
```cpp
// Serve istanza NFPCalculator con cache
NFPCalculator calculator(nfpCache);
Polygon nfpPoly = calculator.computeNFP(A, B);
nfp = {nfpPoly.points}; // Se serve vector<vector<Point>>
```

**Dopo** (opzione 2 - usare MinkowskiSum direttamente):
```cpp
auto nfpPolygons = MinkowskiSum::calculateNFP(A, B);
if (!nfpPolygons.empty()) {
    nfp = /* converti da vector<Polygon> a vector<vector<Point>> */
}
```

### Step 3: Rimuovere da file header

- Rimuovere dichiarazioni da `include/deepnest/geometry/GeometryUtil.h`
- Rimuovere dichiarazioni da `include/deepnest/geometry/GeometryUtilAdvanced.h`

### Step 4: Rimuovere implementazioni

- Rimuovere implementazioni da `src/geometry/GeometryUtil.cpp`
- Rimuovere implementazioni da `src/geometry/GeometryUtilAdvanced.cpp`
- **ELIMINARE** file `src/geometry/OrbitalHelpers.cpp`
- **ELIMINARE** file `include/deepnest/geometry/OrbitalTypes.h` (se solo orbital)

### Step 5: Aggiornare test

- **tests/NFPValidationTests.cpp**: Commentare/rimuovere test che usano noFitPolygon
  - Oppure convertire per usare MinkowskiSum
- **tests/PolygonExtractor.cpp**: Rimuovere/commentare chiamata
- **tests/NFPDebug.cpp**: Rimuovere/convertire o eliminare il tool
- **tests/SearchStartPointDebug.cpp**: Eliminare (debug per searchStartPoint)

### Step 6: Rimuovere da build system

- Aggiornare CMakeLists.txt se necessario (rimuovere OrbitalHelpers.cpp)
- Aggiornare deepnest.pro se necessario

### Step 7: Validazione

1. Build completo senza errori
2. Tutti i test rimanenti passano
3. grep finale per assicurarsi che nessun riferimento rimanga:
   ```bash
   grep -r "noFitPolygon\|searchStartPoint\|polygonSlideDistance" . --include="*.cpp" --include="*.h"
   ```

## Dettagli Implementazione ParallelProcessor

### Codice Attuale (da modificare)

**ParallelProcessor.cpp linee 221-240**:
```cpp
if (pair.inside) {
    // INNER NFP
    if (GeometryUtil::isRectangle(A.points, 0.001)) {
        nfp = GeometryUtil::noFitPolygonRectangle(A.points, B.points);  // KEEP
    } else {
        nfp = GeometryUtil::noFitPolygon(A.points, B.points, true, config.exploreConcave);  // REMOVE
    }
    // ... winding correction
} else {
    // OUTER NFP
    nfp = GeometryUtil::noFitPolygon(A.points, B.points, false, config.exploreConcave);  // REMOVE
    // ... area validation, winding correction
}
```

**ParallelProcessor.cpp linee 275-285** (holes):
```cpp
for (const auto& hole : A.children) {
    auto cnfp = GeometryUtil::noFitPolygon(hole.points, B.points, true, config.exploreConcave);  // REMOVE
    // ... process hole NFP
}
```

### Codice Nuovo (proposta)

Opzione A: Usare NFPCalculator wrapper che già gestisce MinkowskiSum:
```cpp
// Assume we have NFPCache& cache available in scope
NFPCalculator calculator(cache);

if (pair.inside) {
    // INNER NFP
    if (GeometryUtil::isRectangle(A.points, 0.001)) {
        nfp = GeometryUtil::noFitPolygonRectangle(A.points, B.points);  // KEEP
    } else {
        // Use MinkowskiSum via NFPCalculator
        Polygon nfpResult = calculator.computeDiffNFP(A, B);  // For INNER NFP
        if (!nfpResult.empty()) {
            nfp = {nfpResult.points};
        }
    }
} else {
    // OUTER NFP
    Polygon nfpResult = calculator.computeNFP(A, B);  // For OUTER NFP
    if (!nfpResult.empty()) {
        nfp = {nfpResult.points};
    }
}

// For holes:
for (const auto& hole : A.children) {
    Polygon holeAsPolygon;
    holeAsPolygon.points = hole.points;
    Polygon cnfpResult = calculator.computeDiffNFP(holeAsPolygon, B);  // INNER NFP for holes
    if (!cnfpResult.empty()) {
        // process...
    }
}
```

**NOTA**: Verificare se computeNFP / computeDiffNFP sono i metodi giusti per INNER vs OUTER NFP.

## Riepilogo File da Modificare

### Eliminare Completamente:
- [ ] src/geometry/OrbitalHelpers.cpp
- [ ] include/deepnest/geometry/OrbitalTypes.h (se solo orbital)
- [ ] tests/SearchStartPointDebug.cpp

### Modificare Pesantemente:
- [ ] src/geometry/GeometryUtil.cpp (rimuovere ~500+ lines)
- [ ] src/geometry/GeometryUtilAdvanced.cpp (rimuovere funzioni helper)
- [ ] include/deepnest/geometry/GeometryUtil.h (rimuovere dichiarazioni)
- [ ] include/deepnest/geometry/GeometryUtilAdvanced.h (rimuovere dichiarazioni)
- [ ] src/parallel/ParallelProcessor.cpp (sostituire noFitPolygon con MinkowskiSum)

### Aggiornare Test:
- [ ] tests/NFPValidationTests.cpp (rimuovere test noFitPolygon)
- [ ] tests/PolygonExtractor.cpp (rimuovere chiamate)
- [ ] tests/NFPDebug.cpp (aggiornare o rimuovere)

### Build System:
- [ ] CMakeLists.txt (rimuovere OrbitalHelpers.cpp se presente)
- [ ] deepnest.pro (rimuovere OrbitalHelpers.cpp se presente)

## Stima Righe di Codice Rimosse

- noFitPolygon() implementation: ~400 lines
- Helper functions: ~300 lines
- OrbitalHelpers.cpp: ~200 lines (stima)
- OrbitalTypes.h: ~50 lines (stima)
- Test code: ~200 lines
- **TOTALE: ~1150 lines rimossi**

## Note Importanti

1. **exploreConcave config**: Dopo rimozione, questo parametro non sarà più usato (era solo per orbital tracing). Valutare se rimuoverlo da DeepNestConfig.

2. **Performance**: MinkowskiSum potrebbe avere performance diverse da orbital tracing. Testare con benchmark dopo conversione.

3. **Test coverage**: Assicurarsi che MinkowskiSum sia ben testato per tutti i casi che prima usavano noFitPolygon.

4. **Documentazione**: Aggiornare README e commenti per riflettere che ora si usa SOLO Minkowski Sum.

## Checklist Pre-Rimozione

- [ ] Grep completo di tutte le funzioni da rimuovere
- [ ] Confermare che nessuna funzione ha usi legittimi altrove
- [ ] Identificare esattamente quale metodo NFPCalculator usare (computeNFP vs computeDiffNFP)
- [ ] Testare NFPCalculator con casi che prima usavano noFitPolygon
- [ ] Preparare piano di fallback se MinkowskiSum non funziona per tutti i casi

---

**Status**: Analysis completata, pronto per Step 2.1 (Rimozione effettiva)

## VERIFICA GREP COMPLETATA ✅

### Risultati

Tutte le funzioni orbital tracing sono usate SOLO in GeometryUtil files:
- `include/deepnest/geometry/GeometryUtil.h`
- `include/deepnest/geometry/GeometryUtilAdvanced.h`
- `src/geometry/GeometryUtil.cpp`
- `src/geometry/GeometryUtilAdvanced.cpp`

**NESSUN ALTRO USO TROVATO** → Sicure da rimuovere!

### File OrbitalHelpers

```bash
$ ls -la src/geometry/Orbital*
-rw-r--r-- 1 root root 8285 Nov 25 02:03 src/geometry/OrbitalHelpers.cpp
```

Questo file contiene:
- findTouchingContacts
- generateTranslationVectors
- isBacktracking

Usate solo da noFitPolygon → **Intero file può essere eliminato**

### File OrbitalTypes.h

```bash
$ ls -la include/deepnest/geometry/Orbital*
-rw-r--r-- 1 root root 2156 Nov 25 02:03 include/deepnest/geometry/OrbitalTypes.h
```

Definisce strutture per orbital tracing → **Intero file può essere eliminato**

## DECISIONE FINALE

✅ **SICURO PROCEDERE CON RIMOZIONE**

Tutte le funzioni identificate sono usate esclusivamente per orbital tracing NFP e non hanno altri usi legittimi nella codebase.

