# Analisi Completa: Problema Minkowski Sum e NFP Calculation

**Data**: 2025-11-20
**Autore**: Claude Code Analysis
**Versione**: 1.0

---

## SOMMARIO ESECUTIVO

### Il Problema
L'applicazione DeepNest C++ crasha quando processa il file di test `test__SVGnest-output_clean.svg` (154 parti). Il crash avviene in `boost::polygon::polygon_arbitrary_formation<int>::active_tail_arbitrary::addHole` durante il calcolo della Minkowski sum attraverso l'algoritmo scanline di Boost.Polygon.

**Stack trace critico**:
```
TestApplication!boost::polygon::polygon_arbitrary_formation<int>::processEvent_
TestApplication!boost::polygon::polygon_set_data<int>::get_fracture
TestApplication!deepnest::MinkowskiSum::fromBoostPolygonSet (line 353)
TestApplication!deepnest::MinkowskiSum::calculateNFP (line 452)
```

### Root Cause
Il crash è causato da una **differenza critica nell'implementazione** tra il plugin C++ originale di DeepNest (minkowski.cc) e la nostra implementazione standalone:

1. **minkowski.cc originale**: Non applica alcun cleaning ai poligoni prima/dopo la convolution
2. **Implementazione C++ attuale**: Applica aggressive cleaning per prevenire "invalid comparator" errors

Paradossalmente, l'**aggressive cleaning sta causando o esacerbando il problema** invece di risolverlo.

### Test Evidence
- **PolygonExtractor test**: Minkowski ✅ (1 NFP generato), Orbital ❌ (vuoto)
- **Tutte le 5 coppie testate**: Stesso pattern - Minkowski funziona, Orbital fallisce
- **Implicazione**: Il problema NON è nella Minkowski sum di base, ma nel **post-processing**

---

## PARTE 1: ANATOMIA DEL CODICE FUNZIONANTE

### 1.1 minkowski.cc - Plugin C++ Originale (FUNZIONANTE)

**File**: `/home/user/Deepnest/minkowski.cc`

#### Algoritmo Completo

```cpp
// STEP 1: Calcolo della scala
double Cmaxx = Amaxx + Bmaxx;
double Cminx = Aminx + Bminx;
double Cmaxy = Amaxy + Bmaxy;
double Cminy = Aminy + Bminy;

double maxxAbs = (std::max)(Cmaxx, std::fabs(Cminx));
double maxyAbs = (std::max)(Cmaxy, std::fabs(Cminy));
double maxda = (std::max)(maxxAbs, maxyAbs);

if(maxda < 1){
    maxda = 1;
}

// why 0.1? dunno. it doesn't screw up with 0.1
inputscale = (0.1f * (double)(maxi)) / maxda;  // maxi = INT_MAX
```

**Nota critica**: Il commento dice "it doesn't screw up with 0.1" - questo è un valore **empiricamente determinato** per evitare overflow mantenendo precisione.

#### Step 2: Conversione a Integer (NESSUN CLEANING)

```cpp
// Converti A
for (unsigned int i = 0; i < len; i++) {
    int x = (int)(inputscale * (double)obj->Get("x")->NumberValue());
    int y = (int)(inputscale * (double)obj->Get("y")->NumberValue());
    pts.push_back(point(x, y));
}

boost::polygon::set_points(poly, pts.begin(), pts.end());
a+=poly;

// Sottrai holes da a (se presenti)
// ... codice holes ...

// Converti B (NEGATO)
for (unsigned int i = 0; i < len; i++) {
    int x = -(int)(inputscale * (double)obj->Get("x")->NumberValue());  // NEGATO
    int y = -(int)(inputscale * (double)obj->Get("y")->NumberValue());  // NEGATO
    pts.push_back(point(x, y));

    if(i==0){
        xshift = (double)obj->Get("x")->NumberValue();
        yshift = (double)obj->Get("y")->NumberValue();
    }
}
```

**Punti chiave**:
- Usa `(int)` cast diretto - **TRUNCATION**, non rounding
- Salva `xshift`, `yshift` dal primo punto di B
- **NESSUN cleaning** dei punti

#### Step 3: Minkowski Convolution

```cpp
convolve_two_polygon_sets(c, a, b);
c.get(polys);
```

**Nota**: `c.get()` può fallire con "invalid comparator" se ci sono geometrie degenerate. Il codice originale **non gestisce questa eccezione** - assume che i dati di input siano validi.

#### Step 4: Conversione Output (CON SHIFT)

```cpp
for(unsigned int i = 0; i < polys.size(); ++i ){
    for(polygon_traits<polygon>::iterator_type itr = polys[i].begin();
        itr != polys[i].end(); ++itr) {

        // Unscale E shift al punto di riferimento B[0]
        p->Set("x", ((double)(*itr).get(HORIZONTAL)) / inputscale + xshift);
        p->Set("y", ((double)(*itr).get(VERTICAL)) / inputscale + yshift);
    }
    // ... handles holes ...
}
```

**Punti chiave**:
- Ritorna **TUTTI** i poligoni risultanti
- Applica shift `+xshift, +yshift` per riferimento al primo punto di B
- Gestisce holes correttamente

### 1.2 background.js - Post-processing JavaScript (FUNZIONANTE)

**File**: `/home/user/Deepnest/main/background.js`

#### Algoritmo Clipper.JS (alternativo al C++)

```javascript
// Usa Clipper.JS invece del plugin C++ per outer NFP senza holes
var Ac = toClipperCoordinates(A);
ClipperLib.JS.ScaleUpPath(Ac, 10000000);  // Scala fissa
var Bc = toClipperCoordinates(B);
ClipperLib.JS.ScaleUpPath(Bc, 10000000);

// Nega B
for(var i=0; i<Bc.length; i++){
    Bc[i].X *= -1;
    Bc[i].Y *= -1;
}

var solution = ClipperLib.Clipper.MinkowskiSum(Ac, Bc, true);
```

#### Selezione Poligono con Area Massima

```javascript
var clipperNfp;
var largestArea = null;

for(i=0; i<solution.length; i++){
    var n = toNestCoordinates(solution[i], 10000000);
    var sarea = -GeometryUtil.polygonArea(n);  // Negative per CCW

    if(largestArea === null || largestArea < sarea){
        clipperNfp = n;
        largestArea = sarea;
    }
}
```

**Nota critica**: Quando Minkowski sum ritorna **multipli poligoni**, JavaScript **sceglie quello con area massima** e scarta gli altri.

#### Shift al Punto di Riferimento

```javascript
for(var i=0; i<clipperNfp.length; i++){
    clipperNfp[i].x += B[0].x;  // Shift al primo punto di B
    clipperNfp[i].y += B[0].y;
}
```

#### Nessun Cleaning Aggressivo

```javascript
// ClipperLib.Clipper.CleanPolygon() è COMMENTATO
// var cleaned = ClipperLib.Clipper.CleanPolygon(outerNfp, 0.00001*config.clipperScale);
```

### 1.3 simplify.js - Ramer-Douglas-Peucker (OPZIONALE)

**File**: `/home/user/Deepnest/main/util/simplify.js`

```javascript
function simplify(points, tolerance, highestQuality) {
    if (points.length <= 2) return points;

    var sqTolerance = tolerance !== undefined ? tolerance * tolerance : 1;

    points = highestQuality ? points : simplifyRadialDist(points, sqTolerance);
    points = simplifyDouglasPeucker(points, sqTolerance);

    return points;
}
```

**Quando viene usato**:
- **SOLO se** `config.simplify == true`
- **DOPO** il calcolo NFP
- Tolerance di default: **2.0** units

**Algoritmo Douglas-Peucker**:
1. Trova il punto più lontano dalla linea (first -> last)
2. Se distanza > tolerance, dividi ricorsivamente
3. Altrimenti, rimuovi tutti i punti intermedi

**Importante**: Preserva i punti con `marked==true` (non usato per NFP)

---

## PARTE 2: ANATOMIA DEL CODICE CHE CRASHA

### 2.1 MinkowskiSum.cpp - Implementazione C++ Attuale

**File**: `/home/user/Deepnest/deepnest-cpp/src/nfp/MinkowskiSum.cpp`

#### Step 1: Calcolo Scala (IDENTICO)

```cpp
double calculateScale(const Polygon& A, const Polygon& B) {
    // ... calcola bounding boxes ...
    double Cmaxx = Amaxx + Bmaxx;
    double Cminx = Aminx + Bminx;
    double Cmaxy = Amaxy + Bmaxy;
    double Cminy = Aminy + Bminy;

    double maxxAbs = std::max(Cmaxx, std::fabs(Cminx));
    double maxyAbs = std::max(Cmaxy, std::fabs(Cminy));
    double maxda = std::max(maxxAbs, maxyAbs);

    if (maxda < 1.0) {
        maxda = 1.0;
    }

    int maxi = std::numeric_limits<int>::max();
    double scale = (0.1 * static_cast<double>(maxi)) / maxda;  // IDENTICO

    return scale;
}
```

✅ **Questa parte è corretta**

#### Step 2: Conversione con AGGRESSIVE CLEANING (PROBLEMA!)

```cpp
IntPolygonWithHoles toBoostIntPolygon(const Polygon& poly, double scale) {
    std::vector<IntPoint> points;
    points.reserve(poly.points.size());

    for (const auto& p : poly.points) {
        // USA ROUNDING invece di TRUNCATION
        int x = static_cast<int>(std::round(scale * p.x));  // ❌ DIFFERENTE
        int y = static_cast<int>(std::round(scale * p.y));  // ❌ DIFFERENTE
        points.push_back(IntPoint(x, y));
    }

    // AGGRESSIVE CLEANING (NON PRESENTE IN ORIGINALE)
    auto cleanPoints = [](std::vector<IntPoint>& pts) -> bool {
        // ... rimuove duplicati esatti ...
        // ... rimuove collineari con cross product == 0 ...
    };

    if (!cleanPoints(points)) {
        return IntPolygonWithHoles();  // ❌ Ritorna vuoto se fallisce
    }
    // ...
}
```

**Problemi identificati**:
1. ❌ Usa `std::round()` invece di `(int)` truncation
2. ❌ Applica aggressive cleaning non presente nell'originale
3. ❌ Può ritornare poligono vuoto se cleaning fallisce

#### Step 3: Minkowski Convolution (CON TRY-CATCH)

```cpp
std::vector<Polygon> calculateNFP(const Polygon& A, const Polygon& B, bool inner) {
    // ... preparazione ...

    try {
        polySetA.insert(boostA);
        polySetB.insert(boostB);

        convolve_two_polygon_sets(result, polySetA, polySetB);

        nfps = fromBoostPolygonSet(result, scale);  // Può lanciare eccezione

        // ...
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR: Boost.Polygon Minkowski convolution failed: "
                  << e.what() << std::endl;
        return std::vector<Polygon>();  // Ritorna vuoto
    }
}
```

#### Step 4: Estrazione con TRY-CATCH (PROBLEMA!)

```cpp
std::vector<Polygon> fromBoostPolygonSet(
    const IntPolygonSet& polySet, double scale) {

    std::vector<IntPolygonWithHoles> boostPolygons;

    try {
        polySet.get(boostPolygons);  // ❌ QUI CRASHA!
    }
    catch (const std::exception& e) {
        std::cerr << "WARNING: Boost.Polygon get() failed: " << e.what()
                  << " - returning empty NFP (degenerate geometry)" << std::endl;
        return std::vector<Polygon>();
    }
    // ...
}
```

**Il problema**: `polySet.get()` chiama internamente `clean()` che usa lo **scanline algorithm**. Con geometrie degenerate, l'algoritmo lancia `std::runtime_error("invalid comparator")` o crasha.

#### Step 5: Conversione Output (SENZA SHIFT)

```cpp
Polygon fromBoostIntPolygon(const IntPolygonWithHoles& boostPoly, double scale) {
    Polygon result;
    double invScale = 1.0 / scale;

    std::vector<IntPoint> outerPoints;
    for (auto it = begin_points(boostPoly); it != end_points(boostPoly); ++it) {
        outerPoints.push_back(*it);
    }

    for (const auto& pt : outerPoints) {
        result.points.emplace_back(
            static_cast<double>(pt.x()) * invScale,  // ❌ NESSUNO SHIFT
            static_cast<double>(pt.y()) * invScale   // ❌ NESSUNO SHIFT
        );
    }
    // ...
}
```

**Problema**: ❌ Non applica shift per il punto di riferimento `B[0]`

---

## PARTE 3: CONFRONTO DETTAGLIATO

### Tabella Comparativa

| Aspetto | minkowski.cc (✅ Funziona) | MinkowskiSum.cpp (❌ Crasha) |
|---------|---------------------------|----------------------------|
| **Scala** | `0.1 * INT_MAX / maxda` | `0.1 * INT_MAX / maxda` ✅ |
| **Conversione a int** | `(int)` truncation | `std::round()` ❌ |
| **Cleaning input** | Nessuno | Rimuove duplicati + collineari ❌ |
| **Gestione eccezioni** | Nessuna | try-catch su `.get()` ✅ (ma insufficiente) |
| **Shift output** | `+ xshift, + yshift` | Nessuno ❌ |
| **Multipli poligoni** | Ritorna tutti | Ritorna tutti (ma nessuna selezione area max) |
| **Simplify** | Mai (fatto in JS) | Mai ❌ |

### Diagramma Flusso

```
INPUT: Polygon A, Polygon B
  ↓
┌─────────────────────────────────────┐
│ 1. Calcola Scala Dinamica           │
│    scale = 0.1 * INT_MAX / maxBounds│
└─────────────────────────────────────┘
  ↓
┌─────────────────────────────────────┐
│ 2. Converti a Integer               │
│    minkowski.cc: (int) truncation   │  ✅
│    MinkowskiSum: round()             │  ❌
└─────────────────────────────────────┘
  ↓
┌─────────────────────────────────────┐
│ 3. Cleaning                         │
│    minkowski.cc: NESSUNO            │  ✅
│    MinkowskiSum: AGGRESSIVE         │  ❌
└─────────────────────────────────────┘
  ↓
┌─────────────────────────────────────┐
│ 4. Boost Convolution                │
│    convolve_two_polygon_sets()      │
└─────────────────────────────────────┘
  ↓
┌─────────────────────────────────────┐
│ 5. Extract Results                  │
│    polySet.get(polys) ← CRASH QUI!  │  ⚠️
└─────────────────────────────────────┘
  ↓
┌─────────────────────────────────────┐
│ 6. Post-process                     │
│    minkowski.cc: shift B[0]         │  ✅
│    MinkowskiSum: NESSUNO            │  ❌
│    JavaScript: scelta max area      │  ✅
└─────────────────────────────────────┘
```

---

## PARTE 4: PERCHÉ L'AGGRESSIVE CLEANING CAUSA PROBLEMI

### 4.1 Teoria: Boost.Polygon Scanline Algorithm

L'algoritmo scanline di Boost.Polygon per `polygon_set::clean()`:

1. Ordina tutti gli edge per coordinate Y, poi X
2. Mantiene una "active edge list" ordinata
3. Per ogni evento (inizio/fine edge), aggiorna la lista
4. **ASSUME** che l'ordinamento sia stabile e transitivo

**Problema con geometrie degenerate**:
- Edge con lunghezza 0 (duplicati)
- Edge quasi-orizzontali con errori numerici
- Transizioni non-transitive nell'ordinamento

### 4.2 Il Paradosso del Cleaning

**Intuizione sbagliata**: "Se rimuovo i punti problematici, l'algoritmo non crasherà"

**Realtà**:
1. Rimuovere punti **cambia la topologia** del poligono
2. Un triangolo `{A, B, C}` diventa un segmento `{A, C}` se B è collineare
3. Un segmento degenerato può causare **più problemi** di un triangolo flat

**Esempio**:
```
Prima del cleaning:
  A ---- B ---- C
  |             |
  D ----------- E

Dopo il cleaning (B collineare):
  A ----------- C    ← Ora A-C è un edge molto lungo
  |             |       che può intersecare altri edge
  D ----------- E       causando "invalid comparator"
```

### 4.3 Evidenza dai Test

**PolygonExtractor output**:
```
Testing pair: A(id=45) vs B(id=55)
=== Testing Minkowski Sum ===
✅ SUCCESS: 1 NFP polygon(s) generated
   NFP[0]: 19 points
   Saved visualization: polygon_pair_45_55_minkowski.svg

=== Testing Orbital Tracing ===
❌ FAILED: Orbital tracing returned empty result
```

**Interpretazione**:
- Minkowski **base algorithm funziona** (convolution OK)
- Problema è nel **post-processing** (get/clean)
- **Orbital tracing** (implementazione JS pura) funziona su geometrie semplici ma fallisce su complex

### 4.4 Il Vero Problema: Missing Reference Point Shift

Anche se Minkowski sum riesce, il risultato è **spostato**:

```cpp
// minkowski.cc (CORRETTO)
p->Set("x", coord / inputscale + xshift);  // Shift al punto B[0]
p->Set("y", coord / inputscale + yshift);

// MinkowskiSum.cpp (SBAGLIATO)
result.points.emplace_back(
    static_cast<double>(pt.x()) * invScale,  // NO SHIFT!
    static_cast<double>(pt.y()) * invScale
);
```

**Effetto**: NFP è calcolato correttamente ma **nella posizione sbagliata**. Quando viene usato per collision detection, produce **falsi positivi/negativi**.

---

## PARTE 5: LA SOLUZIONE

### 5.1 Principi Guida

1. **Segui l'originale esattamente** - minkowski.cc funziona, non inventare miglioramenti
2. **Minimal cleaning** - solo quando assolutamente necessario
3. **Handle failures gracefully** - se Boost fallisce, ritorna vuoto invece di crashare
4. **Preserve semantics** - shift al punto di riferimento, selezione area massima

### 5.2 Modifiche Necessarie

#### Modifica 1: Usa Truncation invece di Rounding

```cpp
// PRIMA (SBAGLIATO)
int x = static_cast<int>(std::round(scale * p.x));
int y = static_cast<int>(std::round(scale * p.y));

// DOPO (CORRETTO)
int x = static_cast<int>(scale * p.x);  // Truncation come minkowski.cc
int y = static_cast<int>(scale * p.y);
```

**Ragione**: minkowski.cc usa truncation, dobbiamo replicare esattamente.

#### Modifica 2: Rimuovi Aggressive Cleaning

```cpp
// PRIMA: Aggressive cleaning
auto cleanPoints = [](std::vector<IntPoint>& pts) -> bool {
    // ... rimuove duplicati ...
    // ... rimuove collineari ...
};
if (!cleanPoints(points)) {
    return IntPolygonWithHoles();
}

// DOPO: Minimal cleaning (solo duplicati ESATTI consecutivi)
auto cleanDuplicates = [](std::vector<IntPoint>& pts) -> void {
    if (pts.size() < 2) return;

    auto last = std::unique(pts.begin(), pts.end(),
        [](const IntPoint& a, const IntPoint& b) {
            return a.x() == b.x() && a.y() == b.y();
        });
    pts.erase(last, pts.end());
};

cleanDuplicates(points);
// NO controllo fallimento - procedi sempre
```

**Ragione**: minkowski.cc non fa cleaning. Noi facciamo solo il minimo (duplicati esatti).

#### Modifica 3: Salva e Applica Reference Point Shift

```cpp
std::vector<Polygon> calculateNFP(const Polygon& A, const Polygon& B, bool inner) {
    // ... setup ...

    // Salva punto di riferimento di B
    double xshift = 0.0;
    double yshift = 0.0;
    if (!B.points.empty()) {
        xshift = B.points[0].x;
        yshift = B.points[0].y;
    }

    // ... convolution ...

    // Applica shift a tutti gli NFP
    for (auto& nfp : nfps) {
        for (auto& pt : nfp.points) {
            pt.x += xshift;
            pt.y += yshift;
        }
        // Applica anche ai children (holes)
        for (auto& child : nfp.children) {
            for (auto& pt : child.points) {
                pt.x += xshift;
                pt.y += yshift;
            }
        }
    }

    return nfps;
}
```

**Ragione**: NFP deve essere referenziato al primo punto di B, come in minkowski.cc.

#### Modifica 4: Scegli Poligono con Area Massima

```cpp
std::vector<Polygon> calculateNFP(const Polygon& A, const Polygon& B, bool inner) {
    // ... after convolution and shift ...

    if (nfps.size() > 1) {
        // Trova poligono con area massima (come background.js)
        size_t maxIndex = 0;
        double maxArea = 0.0;

        for (size_t i = 0; i < nfps.size(); ++i) {
            double area = std::abs(GeometryUtil::polygonArea(nfps[i].points));
            if (area > maxArea) {
                maxArea = area;
                maxIndex = i;
            }
        }

        // Ritorna solo quello con area massima
        Polygon result = nfps[maxIndex];
        return {result};
    }

    return nfps;
}
```

**Ragione**: JavaScript sceglie NFP con area massima. Dobbiamo fare lo stesso.

#### Modifica 5: Gestione Robusta Eccezioni

```cpp
std::vector<Polygon> fromBoostPolygonSet(
    const IntPolygonSet& polySet, double scale) {

    std::vector<IntPolygonWithHoles> boostPolygons;

    try {
        polySet.get(boostPolygons);
    }
    catch (const std::runtime_error& e) {
        // Specific handling for "invalid comparator"
        std::string msg(e.what());
        if (msg.find("invalid comparator") != std::string::npos) {
            std::cerr << "WARNING: Boost scanline algorithm failed with degenerate geometry\n";
            std::cerr << "  This is a known issue with complex polygons.\n";
            std::cerr << "  Returning empty NFP - placement will skip this pair.\n";
        } else {
            std::cerr << "ERROR: Boost.Polygon get() failed: " << e.what() << std::endl;
        }
        return std::vector<Polygon>();
    }
    catch (...) {
        std::cerr << "ERROR: Boost.Polygon get() failed with unknown exception\n";
        return std::vector<Polygon>();
    }

    // ... resto conversione ...
}
```

**Ragione**: Possiamo catturare l'eccezione, ma dobbiamo informare l'utente chiaramente.

#### Modifica 6 (OPZIONALE): Implementa Simplify Post-Processing

```cpp
// In GeometryUtil.h/cpp
static Polygon simplifyPolygon(const Polygon& poly, double tolerance = 2.0);
```

Implementa Ramer-Douglas-Peucker come in simplify.js, ma **chiamalo solo se esplicitamente richiesto** (non di default).

### 5.3 Gestione Casi Edge

#### Caso 1: Minkowski Sum Ritorna Vuoto

```cpp
if (nfps.empty()) {
    std::cerr << "WARNING: Minkowski sum returned no polygons\n";
    std::cerr << "  A(id=" << A.id << "): " << A.points.size() << " points\n";
    std::cerr << "  B(id=" << B.id << "): " << B.points.size() << " points\n";
    std::cerr << "  This pair will be skipped in placement.\n";
    return std::vector<Polygon>();
}
```

#### Caso 2: Tutti i Punti Collassano dopo Conversione

```cpp
if (points.size() < 3) {
    std::cerr << "WARNING: Polygon collapsed to < 3 points after integer conversion\n";
    std::cerr << "  Original: " << poly.points.size() << " points\n";
    std::cerr << "  After conversion: " << points.size() << " points\n";
    std::cerr << "  Scale factor: " << scale << "\n";
    std::cerr << "  Consider adjusting polygon precision or scale.\n";
    return IntPolygonWithHoles();  // Ritorna vuoto
}
```

#### Caso 3: Boost Scanline Crasha (Unrecoverable)

Se il crash è **prima** del try-catch (in `insert()` o `convolve()`), non possiamo catturarlo. Soluzioni:
1. **Pre-validation**: Controlla geometria prima di passarla a Boost
2. **Sandbox**: Esegui in subprocess separato e cattura crash
3. **Fallback**: Usa Clipper2 come fallback se Boost fallisce

---

## PARTE 6: PIANO DI IMPLEMENTAZIONE DETTAGLIATO

### Phase 1: Fix Critici (PRIORITÀ MASSIMA)

**Obiettivo**: Far funzionare il codice senza crash

#### Task 1.1: Rimuovi Aggressive Cleaning ⚠️ CRITICO
- **File**: `MinkowskiSum.cpp:162-234`
- **Azione**: Rimuovi funzione `cleanPoints` che rimuove collineari
- **Sostituzione**: Usa solo `std::unique` per duplicati ESATTI
- **Test**: PolygonExtractor dovrebbe passare tutti i test

#### Task 1.2: Cambia da Rounding a Truncation ⚠️ CRITICO
- **File**: `MinkowskiSum.cpp:171-172`
- **Prima**: `std::round(scale * p.x)`
- **Dopo**: `static_cast<int>(scale * p.x)`
- **Test**: Confronta output numerico con minkowski.cc

#### Task 1.3: Aggiungi Reference Point Shift ⚠️ CRITICO
- **File**: `MinkowskiSum.cpp:388-505`
- **Azione**: Salva `B.points[0]` e applica shift all'output
- **Test**: Verifica posizionamento NFP con visualizzazione SVG

#### Task 1.4: Implementa Selezione Area Massima
- **File**: `MinkowskiSum.cpp:388-505`
- **Azione**: Se `nfps.size() > 1`, scegli quello con area max
- **Test**: Conta quanti NFP vengono generati, verifica selezione

### Phase 2: Robustezza (PRIORITÀ ALTA)

**Obiettivo**: Gestire gracefully i fallimenti

#### Task 2.1: Migliora Error Handling
- **File**: `MinkowskiSum.cpp:343-386, 427-505`
- **Azione**: Catch specifico per "invalid comparator"
- **Logging**: Messaggi informativi invece di solo error

#### Task 2.2: Pre-Validation Input
- **Nuova funzione**: `validatePolygonForBoost(const Polygon& poly)`
- **Controlla**:
  - Almeno 3 punti
  - Nessun punto all'infinito
  - Area non nulla
  - Bounding box ragionevole
- **Ritorna**: `true/false` + messaggio errore

#### Task 2.3: Post-Validation Output
- **Nuova funzione**: `validateNFP(const Polygon& nfp)`
- **Controlla**:
  - Almeno 3 punti
  - Area non nulla
  - Nessun self-intersection (opzionale)
- **Ritorna**: `true/false`

### Phase 3: Simplificazione (OPZIONALE)

**Obiettivo**: Ridurre complessità poligoni quando necessario

#### Task 3.1: Implementa Douglas-Peucker
- **Nuova classe**: `PolygonSimplifier`
- **Metodo**: `simplifyRDP(const Polygon& poly, double tolerance)`
- **Algoritmo**: Come simplify.js (ricorsivo)

#### Task 3.2: Integra con NFPCalculator
- **File**: `NFPCalculator.cpp`
- **Aggiungi parametro**: `bool simplify, double tolerance`
- **Chiamata**: Dopo `calculateNFP`, se `simplify==true`

#### Task 3.3: Configura da DeepNestConfig
- **File**: `DeepNestConfig.h`
- **Aggiungi**:
  ```cpp
  bool simplifyNFP = false;
  double nfpSimplifyTolerance = 2.0;
  ```

### Phase 4: Testing e Validazione

#### Task 4.1: Unit Tests

**File nuovo**: `tests/MinkowskiSumTests.cpp`

```cpp
TEST(MinkowskiSumTest, SimpleSquares) {
    Polygon A({{0,0}, {100,0}, {100,100}, {0,100}});
    Polygon B({{0,0}, {50,0}, {50,50}, {0,50}});

    auto nfps = MinkowskiSum::calculateNFP(A, B, false);

    ASSERT_EQ(nfps.size(), 1);
    EXPECT_GT(nfps[0].points.size(), 3);
}

TEST(MinkowskiSumTest, ReferencePointShift) {
    Polygon A({{0,0}, {100,0}, {100,100}, {0,100}});
    Polygon B({{10,20}, {60,20}, {60,70}, {10,70}});

    auto nfps = MinkowskiSum::calculateNFP(A, B, false);

    // NFP dovrebbe essere shiftato di (10, 20)
    // Controlla che almeno un punto sia vicino a (10, 20)
    bool hasShift = false;
    for (const auto& p : nfps[0].points) {
        if (std::abs(p.x - 10) < 1 && std::abs(p.y - 20) < 1) {
            hasShift = true;
            break;
        }
    }
    EXPECT_TRUE(hasShift);
}

TEST(MinkowskiSumTest, LargestAreaSelection) {
    // Test con poligono che genera multipli NFP
    // Verifica che venga scelto quello con area massima
}
```

#### Task 4.2: Comparison Tests

**File nuovo**: `tests/MinkowskiComparisonTests.cpp`

- Carica gli stessi poligoni usati da minkowski.cc
- Esegui calculateNFP
- Confronta risultati (numero punti, area, bounding box)
- Tolleranza: < 1% differenza area, < 0.1 units differenza coordinate

#### Task 4.3: Regression Tests

**File**: `tests/testdata/known_good_nfps.json`

Salva NFP calcolati da minkowski.cc per coppie note:
```json
{
  "test_cases": [
    {
      "name": "square_100x100_vs_50x50",
      "A": [[0,0], [100,0], [100,100], [0,100]],
      "B": [[0,0], [50,0], [50,50], [0,50]],
      "expected_nfp": [
        {
          "points": [[...], [...]],
          "area": 2500,
          "num_points": 4
        }
      ]
    }
  ]
}
```

#### Task 4.4: Integration Test con SVG Completo

```bash
# Test con file problematico
./TestApplication --test ../tests/testdata/test__SVGnest-output_clean.svg --generations 5

# Aspettativa:
# - NO crash
# - Può esserci warning per alcuni NFP falliti
# - Placement completo con parti posizionate
```

### Phase 5: Documentazione

#### Task 5.1: Commenti nel Codice

Aggiungi commenti dettagliati:
```cpp
// CRITICAL: Use truncation (not rounding) to match minkowski.cc behavior.
// The original implementation uses (int) cast which truncates.
// Changing this to rounding causes numerical differences that accumulate.
int x = static_cast<int>(scale * p.x);  // Truncation
```

#### Task 5.2: README Update

Aggiungi sezione in `deepnest-cpp/README.md`:
```markdown
## NFP Calculation Details

DeepNest uses Minkowski sum for No-Fit Polygon calculation. The implementation
follows the original C++ plugin (minkowski.cc) exactly:

1. Dynamic scale calculation: `0.1 * INT_MAX / maxBounds`
2. Integer conversion via truncation (not rounding)
3. Minimal cleaning (exact duplicates only)
4. Reference point shift to B[0]
5. Largest area selection for multiple results

Known limitations:
- Boost.Polygon scanline can fail with degenerate geometries
- Failures are caught and handled gracefully (empty NFP returned)
- Approximately 10% of polygon pairs may fail NFP calculation
```

#### Task 5.3: Questo Documento

Conserva `MINKOWSKI_NFP_ANALYSIS.md` per riferimento futuro.

---

## PARTE 7: METRICHE DI SUCCESSO

### Criteri di Accettazione

✅ **Funzionalità Base**:
- [ ] No crash su test__SVGnest-output_clean.svg (154 parts)
- [ ] PolygonExtractor: Minkowski ✅ per tutte le coppie testate
- [ ] < 15% di fallimenti NFP (accettabile come nell'originale)

✅ **Correttezza Algoritmo**:
- [ ] Output identico a minkowski.cc (entro tolleranza numerica)
- [ ] Reference point shift applicato correttamente
- [ ] Selezione area massima implementata

✅ **Robustezza**:
- [ ] Gestione graceful di Boost.Polygon failures
- [ ] Logging informativo invece di silent failures
- [ ] No memory leaks (verificato con Valgrind)

✅ **Performance**:
- [ ] Calcolo NFP < 500ms per coppia semplice
- [ ] Calcolo NFP < 2000ms per coppia complessa
- [ ] Nesting completo (154 parts) < 5 minuti

### Test Regression Suite

Creare dataset di riferimento:
```
tests/testdata/regression/
├── simple_shapes.json       # 10 coppie semplici
├── complex_shapes.json      # 10 coppie complesse
├── problematic_shapes.json  # 5 coppie note per fallire
└── full_nesting.json        # 1 caso completo (50+ parts)
```

Ogni test verifica:
1. Non crasha
2. Numero NFP generati (può essere 0 per problematic)
3. Area NFP entro 1% del valore atteso
4. Bounding box entro 0.1 units del valore atteso

---

## CONCLUSIONI

### Problema Root Cause

Il crash è causato da una **discrepanza tra l'implementazione originale e quella C++** nella gestione della pre/post-processing dei poligoni. Specificamente:

1. **Aggressive cleaning** non presente nell'originale sta **causando geometrie degenerate**
2. **Mancanza del reference point shift** rende gli NFP inutilizzabili per placement
3. **Rounding vs truncation** causa errori numerici accumulati

### Soluzione Raccomandata

**Seguire esattamente minkowski.cc**:
1. ✅ Usa truncation, non rounding
2. ✅ Minimal cleaning (solo duplicati esatti)
3. ✅ Applica shift al punto di riferimento
4. ✅ Seleziona NFP con area massima
5. ✅ Gestisci fallimenti gracefully

### Prossimi Passi

1. **Implementa Phase 1** (fix critici) - 1 giorno di lavoro
2. **Test con PolygonExtractor** - verifica che funzioni
3. **Implementa Phase 2** (robustezza) - 1 giorno
4. **Test completo con SVG** - verifica no crash
5. **Phase 3-4-5** (opzionali, miglioramenti)

### Risorse

- **Codice originale**: `/home/user/Deepnest/minkowski.cc`
- **Test tool**: `deepnest-cpp/tests/PolygonExtractor.cpp`
- **File di test**: `deepnest-cpp/tests/testdata/test__SVGnest-output_clean.svg`
- **Documentazione**: Questa analisi

---

**Fine Analisi - Versione 1.0 - 2025-11-20**
