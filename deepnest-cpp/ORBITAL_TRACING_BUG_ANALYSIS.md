# Orbital Tracing Bug Analysis - CRITICAL ISSUE

## Data: 2025-11-21
## Status: ✅ ROOT CAUSE IDENTIFIED
## Fix Status: ⚠️ IN PROGRESS

---

## ✅ ROOT CAUSE FOUND

**BUG CRITICO**: `polygonSlideDistance()` in `GeometryUtilAdvanced.cpp:391` restituisce **SEMPRE NULL** per tutti i vettori di traduzione!

### Sintomo
```
[SLIDE] Vector (8.877, 7.081) slideOpt is NULL → using vecLength=11.3552
[SLIDE] Vector (-11.066, 2.529) slideOpt is NULL → using vecLength=11.3513
[SLIDE] Vector (11.064, 2.528) slideOpt is NULL → using vecLength=11.3491
```

### Conseguenza
L'algoritmo orbital tracing **seleziona sempre il vettore più lungo** invece del vettore con la slide distance corretta, causando loop prematuri e NFP distorte.

### Causa Probabile
La funzione `segmentDistance()` (chiamata da `polygonSlideDistance`) restituisce sempre `std::nullopt`, probabilmente a causa di:
1. Condizioni di early-return troppo restrittive (righe 260-267)
2. Possibile differenza nella gestione degli offset tra JavaScript e C++
3. Possibile bug nel calcolo geometrico dei segmenti

### Next Step
Debuggare `segmentDistance()` in `GeometryUtilAdvanced.cpp:229` per capire quale condizione causa sempre il return null.

---

## PROBLEMA IDENTIFICATO

L'algoritmo orbital tracing (`noFitPolygon()`) genera NFP **distorte e incomplete**:

| Test Case | Minkowski Sum | Orbital Tracing | Errore |
|-----------|---------------|-----------------|--------|
| Coppia 47-52 (no rotation) | 15 punti ✅ | 7 punti ❌ | **-53% vertices** |
| Coppia 47-52 (rot 180°) | 15 punti ✅ | 4 punti ❌ | **-73% vertices** |

Il risultato è un **loop interno prematuro** invece del perimetro esterno completo.

---

## ROOT CAUSE INVESTIGATION

### Test Case Analizzato
```bash
./bin/PolygonExtractor test.svg --test-pair 47 52 --rotA 180
```

### Traccia Esecuzione (con DEBUG_NFP=1)

```
[NFP] === ORBITAL TRACING START ===
[NFP]   Area A: -468.221 (winding: CW) → Corrected to CCW
[NFP]   Area B: -468.212 (winding: CW) → Corrected to CCW
[NFP]   Start point (heuristic): (-713.079, -391.534)
[NFP]   Initial reference: (-339.894, -180.050)  // B[0] + offsetB

Iteration 0:
  Touch: A[2] vs B[6]
  Best vector: (8.877, 7.081) from A
  New reference: (-331.017, -172.969) ✅

Iteration 1:
  Touch: A[1] vs B[6]
  Best vector: (-8.878, 7.078) from B
  New reference: (-339.895, -165.891) ✅

Iteration 2:
  Touch: A[1] vs B[0], A[1] vs B[7]
  Best vector: (-8.877, -7.081) from A
  New reference: (-348.772, -172.972) ✅

Iteration 3:
  Touch: A[2] vs B[0], A[2] vs B[7]
  Filtered: 4 backtracking vectors
  Best vector: (8.878, -7.078) from B
  New reference: (-339.894, -180.050) ⚠️ MATCHES START!

[NFP]     Loop closed: returned to start point
[NFP]   ✓ Generated NFP with 4 points
```

**PROBLEMA**: Dopo solo 4 iterazioni, l'algoritmo ritorna al punto di partenza, chiudendo un piccolo loop invece di esplorare tutto il perimetro.

---

## DIFFERENZE RISPETTO AL CODICE JAVASCRIPT

### ✅ Implementazioni CORRETTE

1. **Loop Closure Detection**: ✅ Riattivato e funzionante
2. **Start Point Heuristic**: ✅ Matching JavaScript (solo heuristic per OUTSIDE)
3. **Backtracking Detection**: ✅ Cross product check implementato
4. **Vector Selection**: ✅ Seleziona vettore con maxDistance
5. **Vector Trimming**: ✅ Scale by polygonSlideDistance

### ⚠️ AREE SOSPETTE

#### 1. `findTouchingContacts()` - Possibili Touch Mancanti?

**Ipotesi**: Non trova TUTTI i touching contacts, causando percorsi incompleti.

**Test necessario**:
```cpp
// Confrontare touching contacts tra JavaScript e C++ per stesso input
// JavaScript: righe 1504-1520 in geometryutil.js
// C++: OrbitalHelpers.cpp
```

#### 2. `generateTranslationVectors()` - Vettori Incompleti?

**Ipotesi**: Non genera TUTTI i vettori corretti per ogni touch type.

**Test necessario**:
```cpp
// Verificare che per VERTEX_VERTEX generi esattamente 4 vettori
// Per VERTEX_ON_EDGE generi esattamente 2 vettori
// Confrontare con JavaScript righe 1552-1614
```

#### 3. `polygonSlideDistance()` - Calcolo Distanza Scorretto?

**Ipotesi**: Calcola distanze di slide errate, influenzando la selezione del vettore.

**Test necessario**:
```cpp
// Confrontare slideDistance calcolate per ogni vettore candidato// C++: GeometryUtilAdvanced.cpp:391
// JavaScript: geometryutil.js:1122-1239
```

#### 4. Criterio di Selezione - Logica Diversa?

**C++** (GeometryUtil.cpp:759-769):
```cpp
if (!bestVector || d > maxDistance) {
    bestVector = &vec;
    maxDistance = d;
}
```

**JavaScript** (geometryutil.js:1653-1656):
```javascript
if(d !== null && d > maxd){
    maxd = d;
    translate = vectors[i];
}
```

Sembrano identici, ma verificare edge cases (d=null, d=0, etc).

---

## DEBUGGING STEPS RACCOMANDATI

### Step 1: Dump Completo dei Dati per Confronto

Modifica `GeometryUtil.cpp` per stampare TUTTI i dati ad ogni iterazione:

```cpp
#if DEBUG_NFP
    std::cerr << "\n=== ITERATION " << counter << " FULL DUMP ===" << std::endl;

    // Dump touching contacts
    for (const auto& touch : touchingList) {
        std::cerr << "  Touch: type=" << (int)touch.type
                  << " A[" << touch.indexA << "]=" << A[touch.indexA]
                  << " B[" << touch.indexB << "]=" << B[touch.indexB]
                  << " offsetB=" << offsetB << std::endl;
    }

    // Dump ALL candidate vectors BEFORE filtering
    for (size_t i = 0; i < allVectors.size(); i++) {
        auto slideOpt = polygonSlideDistance(A, B, allVectors[i], true);
        double slide = slideOpt.value_or(allVectors[i].length());
        bool isBack = isBacktracking(allVectors[i], prevVector);

        std::cerr << "  Vector[" << i << "]: "
                  << "(" << allVectors[i].x << ", " << allVectors[i].y << ") "
                  << "polygon=" << allVectors[i].polygon << " "
                  << "slide=" << slide << " "
                  << "backtrack=" << isBack << " "
                  << "start=" << allVectors[i].startIndex << " "
                  << "end=" << allVectors[i].endIndex << std::endl;
    }

    // Dump selected vector
    if (bestVector) {
        std::cerr << "  SELECTED: (" << bestVector->x << ", " << bestVector->y << ") "
                  << "maxDist=" << maxDistance << " polygon=" << bestVector->polygon << std::endl;
    }

    // Dump current NFP polygon
    std::cerr << "  Current NFP points: " << nfp.size() << std::endl;
    for (size_t i = 0; i < nfp.size(); i++) {
        std::cerr << "    [" << i << "]: (" << nfp[i].x << ", " << nfp[i].y << ")" << std::endl;
    }
#endif
```

### Step 2: Confrontare con JavaScript

Eseguire lo STESSO test case nel JavaScript originale con logging completo e confrontare:
1. Numero di iterazioni
2. Touching contacts ad ogni iterazione
3. Vettori generati ad ogni iterazione
4. Vettore selezionato ad ogni iterazione
5. NFP points generati

### Step 3: Identificare la Divergenza

Trovare la PRIMA iterazione dove JavaScript e C++ divergono e analizzare:
- Perché JavaScript trova più touching contacts?
- Perché JavaScript genera vettori diversi?
- Perché JavaScript seleziona un vettore diverso?

---

## WORKAROUND TEMPORANEO

Fino alla risoluzione del bug, il **Minkowski Sum è l'unico metodo affidabile**.

**Raccomandazione**: Disabilitare l'orbital tracing come fallback in produzione:

```cpp
// In NFPCalculator::computeNFP()
if (nfps.empty()) {
    // DON'T use broken orbital tracing
    // Use conservative bounding box instead
    return createConservativeBoundingBoxNFP(A, B, inside);
}
```

---

## IMPATTO

**Alta Priorità** ❌

- **Nesting Quality**: NFP distorte causano piazzamenti sub-ottimali
- **Collision Detection**: Poligoni potrebbero sovrapporsi
- **Placement Errors**: Parti posizionate incorrettamente

**Il sistema è attualmente UTILIZZABILE** perché:
- ✅ Minkowski Sum funziona perfettamente
- ✅ Minkowski Sum viene usato come metodo primario
- ⚠️ Orbital tracing viene usato solo come fallback (raro)

Ma l'orbital tracing VA RISOLTO per completezza.

---

## NEXT STEPS

1. ✅ Documentare il problema
2. ✅ Implementare debug logging dettagliato (`DEBUG_NFP=1`)
3. ✅ Identificare root cause: `polygonSlideDistance()` restituisce sempre NULL
4. ✅ Correggere PolygonExtractor per applicare traslazione B[0] (matching NFPCalculator)
5. ⬜ **FIX CRITICO**: Debuggare e correggere `segmentDistance()` in GeometryUtilAdvanced.cpp:229
   - Aggiungere logging per capire quale condizione restituisce sempre null
   - Confrontare con JavaScript geometryutil.js:958
   - Verificare gestione degli offset e calcoli geometrici
6. ⬜ Ricompilare e testare con fix implementato
7. ⬜ Verificare che NFP orbital === NFP Minkowski (overlap 99%)

---

## RIFERIMENTI

- **JavaScript Originale**: `main/util/geometryutil.js:1437-1727` (noFitPolygon)
- **C++ Implementation**: `deepnest-cpp/src/geometry/GeometryUtil.cpp:561-879`
- **Helpers**: `deepnest-cpp/src/geometry/OrbitalHelpers.cpp`
- **Test Tool**: `deepnest-cpp/tests/PolygonExtractor.cpp`

---

## AUTORE

Analisi iniziale: Claude (Anthropic)
Data: 2025-11-21
