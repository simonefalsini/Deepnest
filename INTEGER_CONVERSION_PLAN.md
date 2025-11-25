# Piano di Conversione DeepNest C++ a Matematica Intera

## Obiettivo
Convertire l'intera libreria DeepNest C++ da matematica in virgola mobile (double/float) a matematica intera (int64_t/int32_t). La scalatura avverrà solo in fase di import da SVG e QPainterPath e in fase di export per tornare ai valori originali.

## Strategia Generale
- **Tipo coordinata**: int64_t (per evitare overflow in calcoli intermedi)
- **Tipo boost**: boost::polygon::point_data<int64_t>
- **Tolleranze**: Tutte intere, default = 1
- **Scalatura**: Solo all'import/export tramite parametro `inputScale` in DeepNestConfig
- **Pulizia**: Rimozione funzione `noFitPolygon` e codice inutilizzato

---

## FASE 1: PREPARAZIONE E ANALISI

### Step 1.1: Backup e Documentazione (30 min)
**Obiettivo**: Creare backup e documentare lo stato attuale

**Azioni**:
1. Creare un branch git per la conversione
2. Documentare tutti i test attuali e i loro risultati
3. Creare una lista completa di tutti i file da modificare
4. Identificare tutti gli usi di double/float nella codebase

**File interessati**: Tutti
**Test**: N/A
**Verifica**: Branch creato, documentazione completa

---

### Step 1.2: Identificazione Funzioni Non Utilizzate (1 ora)
**Obiettivo**: Identificare tutte le funzioni non utilizzate da rimuovere

**Azioni**:
1. Cercare tutti gli utilizzi di `noFitPolygon` (non `noFitPolygonRectangle`)
2. Identificare funzioni helper usate solo da `noFitPolygon`
3. Identificare codice dead in GeometryUtil.cpp e GeometryUtilAdvanced.cpp
4. Creare lista di funzioni da rimuovere

**File da analizzare**:
- `deepnest-cpp/include/deepnest/geometry/GeometryUtil.h`
- `deepnest-cpp/src/geometry/GeometryUtil.cpp`
- `deepnest-cpp/include/deepnest/geometry/GeometryUtilAdvanced.h`
- `deepnest-cpp/src/geometry/GeometryUtilAdvanced.cpp`
- Tutti i file di test che usano queste funzioni

**Funzioni da rimuovere** (confermare con grep):
- `noFitPolygon()` - Funzione principale orbital NFP
- `searchStartPoint()` - Usata solo da noFitPolygon
- `polygonSlideDistance()` - Usata solo da noFitPolygon
- `polygonProjectionDistance()` - Usata solo da noFitPolygon
- `pointDistance()` - Usata solo da noFitPolygon
- `pointLineDistance()` - Usata solo da noFitPolygon
- `segmentDistance()` - Usata solo da noFitPolygon
- `polygonEdge()` - Usata solo da noFitPolygon
- `polygonHull()` - Se usata solo da noFitPolygon
- Funzioni in `OrbitalHelpers.cpp`: `findTouchingContacts`, `generateTranslationVectors`, `isBacktracking`

**Funzioni da MANTENERE** (anche se non usate ora):
- `noFitPolygonRectangle()` - Per ottimizzazioni future
- `isRectangle()` - Per ottimizzazioni future

**Test**: Verifica con grep che noFitPolygon non sia usato da MinkowskiSum
**Verifica**: Lista completa funzioni da rimuovere

---

## FASE 2: PULIZIA DEL CODICE

### Step 2.1: Rimozione Funzione noFitPolygon e Dipendenze (2 ore)
**Obiettivo**: Rimuovere completamente noFitPolygon e tutte le funzioni collegate

**Azioni**:
1. Rimuovere dichiarazione `noFitPolygon` da GeometryUtil.h
2. Rimuovere implementazione `noFitPolygon` da GeometryUtil.cpp
3. Rimuovere tutte le funzioni helper identificate in Step 1.2
4. Rimuovere file OrbitalTypes.h se contiene solo strutture per orbital
5. Rimuovere file OrbitalHelpers.cpp
6. Aggiornare CMakeLists.txt e .pro files se necessario
7. Rimuovere test che usano queste funzioni

**File da modificare**:
- `deepnest-cpp/include/deepnest/geometry/GeometryUtil.h`
- `deepnest-cpp/src/geometry/GeometryUtil.cpp`
- `deepnest-cpp/include/deepnest/geometry/GeometryUtilAdvanced.h`
- `deepnest-cpp/src/geometry/GeometryUtilAdvanced.cpp`
- `deepnest-cpp/include/deepnest/geometry/OrbitalTypes.h` (valutare rimozione)
- `deepnest-cpp/src/geometry/OrbitalHelpers.cpp` (rimuovere)
- `deepnest-cpp/tests/NFPDebug.cpp` (aggiornare)
- `deepnest-cpp/tests/NFPValidationTests.cpp` (aggiornare)
- CMakeLists.txt e *.pro files

**Test**: Build completa senza errori
**Verifica**: Codice compila, nessun riferimento a noFitPolygon rimane

---

### Step 2.2: Rimozione Altro Codice Non Utilizzato (1 ora)
**Obiettivo**: Rimuovere altro codice dead identificato

**Azioni**:
1. Rimuovere funzioni geometriche non utilizzate
2. Rimuovere include non necessari
3. Pulire commenti obsoleti
4. Verificare che ogni funzione rimasta sia effettivamente usata

**File da modificare**:
- `deepnest-cpp/include/deepnest/geometry/GeometryUtil.h`
- `deepnest-cpp/src/geometry/GeometryUtil.cpp`
- `deepnest-cpp/include/deepnest/geometry/GeometryUtilAdvanced.h`
- `deepnest-cpp/src/geometry/GeometryUtilAdvanced.cpp`

**Test**: Build completa, tutti i test passano
**Verifica**: Codice più pulito e compatto

---

## FASE 3: MODIFICA TIPI DI BASE

### Step 3.1: Aggiunta Parametro inputScale in DeepNestConfig (30 min)
**Obiettivo**: Aggiungere il parametro di scalatura alla configurazione

**Azioni**:
1. Aggiungere membro `double inputScale` a DeepNestConfig
2. Impostare valore default (es. 10000.0 o 1000000.0)
3. Aggiungere getter/setter con validazione
4. Aggiungere al JSON import/export
5. Documentare il parametro

**File da modificare**:
- `deepnest-cpp/include/deepnest/config/DeepNestConfig.h`
- `deepnest-cpp/src/config/DeepNestConfig.cpp`

**Codice da aggiungere in DeepNestConfig.h**:
```cpp
/**
 * @brief Input scale factor for integer conversion
 *
 * All floating point coordinates are multiplied by this factor
 * when converting from SVG/QPainterPath to internal representation.
 * Higher values = more precision but risk of overflow.
 *
 * Recommended values:
 * - 1000.0 for mm precision (0.001 mm resolution)
 * - 10000.0 for 0.1mm precision
 * - 100000.0 for high precision (0.00001 units)
 *
 * Default: 10000.0
 */
double inputScale;

double getInputScale() const { return inputScale; }
void setInputScale(double value);
```

**Test**: Config può essere creato e salvato/caricato
**Verifica**: inputScale presente e funzionante

---

### Step 3.2: Modifica Types.h - Definizione Tipi Base (1 ora)
**Obiettivo**: Cambiare i tipi di base da double a int64_t

**Azioni**:
1. Cambiare `BoostPoint = boost::polygon::point_data<int64_t>`
2. Cambiare `BoostPolygon = boost::polygon::polygon_data<int64_t>`
3. Cambiare `BoostPolygonWithHoles = boost::polygon::polygon_with_holes_data<int64_t>`
4. Cambiare `BoostPolygonSet = boost::polygon::polygon_set_data<int64_t>`
5. Cambiare `constexpr double TOL = 1e-9` in `constexpr int64_t TOL = 1`
6. Aggiungere typedef per tipo coordinata: `using CoordType = int64_t;`

**File da modificare**:
- `deepnest-cpp/include/deepnest/core/Types.h`

**Prima**:
```cpp
using BoostPoint = boost::polygon::point_data<double>;
using BoostPolygon = boost::polygon::polygon_data<double>;
using BoostPolygonWithHoles = boost::polygon::polygon_with_holes_data<double>;
using BoostPolygonSet = boost::polygon::polygon_set_data<double>;
constexpr double TOL = 1e-9;
```

**Dopo**:
```cpp
// Base coordinate type - int64_t for precision without overflow
using CoordType = int64_t;

// Boost.Polygon types using integer coordinates
using BoostPoint = boost::polygon::point_data<CoordType>;
using BoostPolygon = boost::polygon::polygon_data<CoordType>;
using BoostPolygonWithHoles = boost::polygon::polygon_with_holes_data<CoordType>;
using BoostPolygonSet = boost::polygon::polygon_set_data<CoordType>;

// Integer tolerance (1 = minimum distinguishable distance)
constexpr CoordType TOL = 1;
```

**Test**: Verifica che compili (ci saranno errori da risolvere nei prossimi step)
**Verifica**: Tipi base sono int64_t

---

### Step 3.3: Modifica Point.h - Struttura Point (2 ore)
**Obiettivo**: Convertire Point da double a int64_t

**Azioni**:
1. Cambiare membri `x`, `y` da `double` a `CoordType` (int64_t)
2. Aggiornare tutti i metodi matematici per lavorare con interi
3. Rimuovere metodi che perdono senso con interi (es. normalize deve gestire approssimazioni)
4. Aggiornare `almostEqual` per confronto interi (diventa semplice `abs(a-b) <= tolerance`)
5. Aggiornare operatori matematici
6. Modificare `distanceTo()` per restituire int64_t o double (valutare)
7. Rimuovere/modificare `rotate()` (la rotazione deve essere gestita diversamente)

**File da modificare**:
- `deepnest-cpp/include/deepnest/core/Point.h`
- `deepnest-cpp/src/core/Point.cpp`

**Modifiche principali**:
```cpp
struct Point {
    CoordType x;  // Era: double x
    CoordType y;  // Era: double y
    bool exact;
    bool marked;

    Point() : x(0), y(0), exact(true), marked(false) {}
    Point(CoordType x_, CoordType y_, bool exact_ = true)
        : x(x_), y(y_), exact(exact_), marked(false) {}

    // almostEqual diventa semplice confronto intero
    static bool almostEqual(CoordType a, CoordType b, CoordType tolerance = TOL) {
        return std::abs(a - b) <= tolerance;
    }

    // distanceTo - usare int64_t per evitare overflow
    int64_t distanceSquaredTo(const Point& other) const {
        int64_t dx = static_cast<int64_t>(x) - static_cast<int64_t>(other.x);
        int64_t dy = static_cast<int64_t>(y) - static_cast<int64_t>(other.y);
        return dx * dx + dy * dy;
    }

    // distanceTo - può restare double per sqrt
    double distanceTo(const Point& other) const {
        return std::sqrt(static_cast<double>(distanceSquaredTo(other)));
    }

    // RIMUOVERE: rotate, rotateAround, normalize (richiedono float)
    // La rotazione sarà gestita a livello Polygon con lookup table
};
```

**Attenzione**:
- Le operazioni di rotazione e normalizzazione devono essere gestite diversamente
- Valutare se mantenere alcune operazioni in double per risultati intermedi

**Test**: Point compila, test base passano
**Verifica**: Point usa int64_t internamente

---

### Step 3.4: Modifica BoundingBox (30 min)
**Obiettivo**: Convertire BoundingBox a coordinate intere

**Azioni**:
1. Cambiare membri da `double` a `CoordType`
2. Aggiornare metodi (width, height, area, etc.)

**File da modificare**:
- `deepnest-cpp/include/deepnest/core/BoundingBox.h`
- `deepnest-cpp/src/core/BoundingBox.cpp`

**Test**: BoundingBox compila e funziona
**Verifica**: Usa CoordType

---

## FASE 4: CONVERSIONI SVG E QPAINTERPATH

### Step 4.1: Modifica QtBoostConverter - Conversioni con Scalatura (3 ore)
**Obiettivo**: Implementare scalatura in/out nelle conversioni

**Azioni**:
1. Modificare `fromQPainterPath()` per scalare da double a int64_t
2. Modificare `toQPainterPath()` per scalare da int64_t a double
3. Aggiungere parametro `scale` a tutte le funzioni di conversione
4. Gestire arrotondamenti correttamente (round, non trunc)
5. Gestire curve Bezier e Arc con scalatura

**File da modificare**:
- `deepnest-cpp/include/deepnest/converters/QtBoostConverter.h`
- `deepnest-cpp/src/converters/QtBoostConverter.cpp`

**Nuove signature**:
```cpp
namespace QtBoostConverter {
    // Conversione da Qt (double) a interno (int64_t) - SCALE UP
    Polygon fromQPainterPath(const QPainterPath& path, double scale);
    BoostPolygon toBoostPolygon(const QPainterPath& path, double scale);
    Point fromQPointF(const QPointF& point, double scale);

    // Conversione da interno (int64_t) a Qt (double) - SCALE DOWN
    QPainterPath toQPainterPath(const Polygon& polygon, double scale);
    QPainterPath fromBoostPolygon(const BoostPolygon& poly, double scale);
    QPointF toQPointF(const Point& point, double scale);
}
```

**Implementazione esempio**:
```cpp
Point fromQPointF(const QPointF& point, double scale) {
    // Scala e arrotonda
    CoordType x = static_cast<CoordType>(std::round(point.x() * scale));
    CoordType y = static_cast<CoordType>(std::round(point.y() * scale));
    return Point(x, y, true);
}

QPointF toQPointF(const Point& point, double scale) {
    // Scala indietro a double
    double x = static_cast<double>(point.x) / scale;
    double y = static_cast<double>(point.y) / scale;
    return QPointF(x, y);
}
```

**Test**: Conversioni roundtrip (Qt->int64->Qt) preservano valori entro tolleranza
**Verifica**: Scalatura funziona correttamente

---

### Step 4.2: Modifica SVGLoader (2 ore)
**Obiettivo**: SVGLoader usa scalatura all'import

**Azioni**:
1. Aggiornare SVGLoader per accettare parametro scale
2. Passare scale a tutte le conversioni QPainterPath->Polygon
3. Gestire transform SVG con scalatura

**File da modificare**:
- `deepnest-cpp/tests/SVGLoader.h`
- `deepnest-cpp/tests/SVGLoader.cpp`

**Test**: SVG caricati hanno coordinate intere corrette
**Verifica**: Forme caricate da SVG sono scalate correttamente

---

### Step 4.3: Modifica Polygon - Conversioni (2 ore)
**Obiettivo**: Aggiornare metodi di conversione di Polygon

**Azioni**:
1. Aggiornare `fromQPainterPath()` per usare scale
2. Aggiornare `toQPainterPath()` per usare scale
3. Aggiornare `fromBoostPolygon()` e `toBoostPolygon()`
4. Gestire metadata (rotation deve considerare che ora lavoriamo in int)

**File da modificare**:
- `deepnest-cpp/include/deepnest/core/Polygon.h`
- `deepnest-cpp/src/core/Polygon.cpp`

**Test**: Conversioni Polygon funzionano
**Verifica**: Roundtrip preserva forma

---

## FASE 5: GEOMETRIA DI BASE

### Step 5.1: Modifica GeometryUtil - Funzioni Base (3 ore)
**Obiettivo**: Convertire funzioni geometriche base a matematica intera

**Azioni**:
1. Aggiornare `almostEqual()` per usare tolleranza intera
2. Aggiornare `withinDistance()` per confronti interi
3. Aggiornare `onSegment()` per matematica intera
4. Aggiornare `lineIntersect()` per calcoli interi (cross product, etc.)
5. Aggiornare `polygonArea()` per restituire int64_t
6. Aggiornare `pointInPolygon()` per matematica intera
7. Aggiornare `isRectangle()` per tolleranza intera
8. Aggiornare funzioni Bezier per linearizzazione con coordinate intere
9. Aggiornare funzioni Arc per linearizzazione con coordinate intere

**File da modificare**:
- `deepnest-cpp/include/deepnest/geometry/GeometryUtil.h`
- `deepnest-cpp/src/geometry/GeometryUtil.cpp`

**Attenzione speciale**:
- **lineIntersect**: usare cross product con int64_t, attenzione overflow
- **Bezier/Arc linearization**: tolleranza ora è intera
- **polygonArea**: risultato può essere molto grande, usare int64_t

**Esempio lineIntersect**:
```cpp
std::optional<Point> lineIntersect(
    const Point& A, const Point& B,
    const Point& E, const Point& F,
    bool infinite)
{
    // Calcoli con int64_t per evitare overflow
    int64_t a1 = static_cast<int64_t>(B.y) - static_cast<int64_t>(A.y);
    int64_t b1 = static_cast<int64_t>(A.x) - static_cast<int64_t>(B.x);
    int64_t c1 = a1 * A.x + b1 * A.y;

    int64_t a2 = static_cast<int64_t>(F.y) - static_cast<int64_t>(E.y);
    int64_t b2 = static_cast<int64_t>(E.x) - static_cast<int64_t>(F.x);
    int64_t c2 = a2 * E.x + b2 * E.y;

    int64_t det = a1 * b2 - a2 * b1;

    if (det == 0) {
        return std::nullopt;  // Parallele
    }

    int64_t x = (b2 * c1 - b1 * c2) / det;
    int64_t y = (a1 * c2 - a2 * c1) / det;

    // Controllo che sia nell'intervallo del segmento
    // ...

    return Point(static_cast<CoordType>(x), static_cast<CoordType>(y));
}
```

**Test**: Tutti i test geometrici base passano
**Verifica**: Funzioni geometriche lavorano correttamente con interi

---

### Step 5.2: Modifica ConvexHull (1 ora)
**Obiettivo**: Algoritmo Graham Scan con matematica intera

**Azioni**:
1. Aggiornare calcoli angolari per usare cross product intero invece di atan2
2. Aggiornare confronti per usare tolleranza intera

**File da modificare**:
- `deepnest-cpp/include/deepnest/geometry/ConvexHull.h`
- `deepnest-cpp/src/geometry/ConvexHull.cpp`

**Test**: Convex hull corretto per vari poligoni
**Verifica**: Algoritmo usa solo operazioni intere

---

### Step 5.3: Modifica Transformation (2 ore)
**Obiettivo**: Gestire trasformazioni con matematica intera

**Azioni**:
1. Modificare rappresentazione matrice per preservare precisione
2. Implementare rotazione con lookup table o mantissa-esponente
3. Implementare traslazione (semplice con interi)
4. Implementare scala (attenzione a overflow)

**File da modificare**:
- `deepnest-cpp/include/deepnest/geometry/Transformation.h`
- `deepnest-cpp/src/geometry/Transformation.cpp`

**Strategia rotazione**:
Opzione A: Usare fixed-point arithmetic (mantissa + esponente)
Opzione B: Usare lookup table per angoli comuni (0°, 90°, 180°, 270°)
Opzione C: Calcolare in double e arrotondare a int64_t

**Raccomandazione**: Opzione C per semplicità, con cache dei risultati

**Test**: Trasformazioni preservano forma entro tolleranza
**Verifica**: Rotazioni funzionano correttamente

---

## FASE 6: OPERAZIONI SUI POLIGONI

### Step 6.1: Aggiornamento Clipper2 Usage (2 ore)
**Obiettivo**: Verificare che Clipper2 supporti int64_t e aggiornare chiamate

**Azioni**:
1. Verificare versione Clipper2 supporta int64_t (dovrebbe)
2. Aggiornare conversioni Polygon <-> Clipper2 paths
3. Rimuovere clipperScale da config (non più necessario!)
4. Aggiornare tutti gli usi di Clipper2

**File da modificare**:
- `deepnest-cpp/include/deepnest/geometry/PolygonOperations.h`
- `deepnest-cpp/src/geometry/PolygonOperations.cpp`
- `deepnest-cpp/include/deepnest/config/DeepNestConfig.h` (rimuovere clipperScale)

**Note**: Clipper2 lavora nativamente con int64_t, quindi sarà più semplice!

**Test**: Operazioni booleane funzionano correttamente
**Verifica**: Offset, union, intersection funzionano

---

### Step 6.2: Modifica PolygonOperations (2 ore)
**Obiettivo**: Tutte le operazioni sui poligoni usano matematica intera

**Azioni**:
1. Aggiornare `offset()` per tolleranza intera
2. Aggiornare `cleanPolygon()` per tolleranza intera
3. Aggiornare `simplifyPolygon()` per tolleranza intera
4. Rimuovere conversioni double non necessarie

**File da modificare**:
- `deepnest-cpp/include/deepnest/geometry/PolygonOperations.h`
- `deepnest-cpp/src/geometry/PolygonOperations.cpp`

**Test**: Offset e semplificazione funzionano
**Verifica**: No conversioni double inutili

---

### Step 6.3: Modifica Polygon - Trasformazioni (2 ore)
**Obiettivo**: Metodi di trasformazione di Polygon usano interi

**Azioni**:
1. Aggiornare `rotate()` per usare nuova Transformation
2. Aggiornare `translate()` (semplice con interi)
3. Aggiornare `scale()` (con attenzione a overflow)
4. Aggiornare `offset()` per usare PolygonOperations aggiornate
5. Gestire campo `rotation` (può rimanere double per metadata)

**File da modificare**:
- `deepnest-cpp/include/deepnest/core/Polygon.h`
- `deepnest-cpp/src/core/Polygon.cpp`

**Test**: Trasformazioni preservano forma
**Verifica**: Coordinate rimangono intere

---

## FASE 7: CALCOLO NFP

### Step 7.1: Modifica MinkowskiSum (3 ore)
**Obiettivo**: Minkowski sum con coordinate intere

**Azioni**:
1. Verificare che boost::polygon::convolve supporti int64_t
2. Aggiornare calcoli per usare coordinate intere
3. Rimuovere `calculateScale` (non più necessario!)
4. Aggiornare conversioni

**File da modificare**:
- `deepnest-cpp/include/deepnest/nfp/MinkowskiSum.h`
- `deepnest-cpp/src/nfp/MinkowskiSum.cpp`

**Test**: NFP Minkowski corretti per vari poligoni
**Verifica**: Usa solo int64_t internamente

---

### Step 7.2: Modifica NFPCalculator (2 ore)
**Obiettivo**: Calculator usa coordinate intere

**Azioni**:
1. Aggiornare `getOuterNFP()` per coordinate intere
2. Aggiornare `getInnerNFP()` per coordinate intere
3. Aggiornare `getFrame()` per coordinate intere
4. Verificare che noFitPolygonRectangle usi matematica intera

**File da modificare**:
- `deepnest-cpp/include/deepnest/nfp/NFPCalculator.h`
- `deepnest-cpp/src/nfp/NFPCalculator.cpp`

**Test**: NFP calculation corretta
**Verifica**: Tutti i path NFP usano interi

---

### Step 7.3: Modifica NFPCache (30 min)
**Obiettivo**: Cache usa coordinate intere

**Azioni**:
1. Verificare che key generation funzioni con nuovi tipi
2. Aggiornare serializzazione se necessario

**File da modificare**:
- `deepnest-cpp/include/deepnest/nfp/NFPCache.h`
- `deepnest-cpp/src/nfp/NFPCache.cpp`

**Test**: Cache funziona correttamente
**Verifica**: Key generation corretta

---

## FASE 8: PLACEMENT E ALGORITMO GENETICO

### Step 8.1: Modifica PlacementStrategy (2 ore)
**Obiettivo**: Strategie di placement con coordinate intere

**Azioni**:
1. Aggiornare calcolo fitness per usare int64_t
2. Aggiornare GravityPlacement per calcoli interi
3. Aggiornare BoundingBoxPlacement per calcoli interi
4. Aggiornare ConvexHullPlacement per calcoli interi
5. Gestire tolleranze intere

**File da modificare**:
- `deepnest-cpp/include/deepnest/placement/PlacementStrategy.h`
- `deepnest-cpp/src/placement/PlacementStrategy.cpp`

**Nota**: Fitness può rimanere double per convenienza (è solo un numero di merito)

**Test**: Placement strategies danno risultati corretti
**Verifica**: Calcoli interni usano interi

---

### Step 8.2: Modifica MergeDetection (2 ore)
**Obiettivo**: Merge detection con matematica intera

**Azioni**:
1. Aggiornare `calculateMergedLength()` per tolleranza intera
2. Aggiornare confronti distanze per usare interi
3. Aggiornare thresholds per essere interi

**File da modificare**:
- `deepnest-cpp/include/deepnest/placement/MergeDetection.h`
- `deepnest-cpp/src/placement/MergeDetection.cpp`

**Test**: Merge detection funziona
**Verifica**: Usa tolleranze intere

---

### Step 8.3: Modifica PlacementWorker (2 ore)
**Obiettivo**: Worker usa coordinate intere

**Azioni**:
1. Aggiornare `placeParts()` per gestire interi
2. Aggiornare calcoli fitness (può rimanere double)
3. Aggiornare gestione overlap con tolleranza intera

**File da modificare**:
- `deepnest-cpp/include/deepnest/placement/PlacementWorker.h`
- `deepnest-cpp/src/placement/PlacementWorker.cpp`

**Test**: Placement completo funziona
**Verifica**: Usa coordinate intere

---

### Step 8.4: Modifica Individual e Population (1 ora)
**Obiettivo**: Algoritmo genetico con nuovi tipi

**Azioni**:
1. Verificare che Individual funzioni con Polygon a coordinate intere
2. Aggiornare mutation se necessario
3. Aggiornare crossover se necessario

**File da modificare**:
- `deepnest-cpp/include/deepnest/algorithm/Individual.h`
- `deepnest-cpp/src/algorithm/Individual.cpp`
- `deepnest-cpp/include/deepnest/algorithm/Population.h`
- `deepnest-cpp/src/algorithm/Population.cpp`

**Test**: GA funziona correttamente
**Verifica**: Nessun problema con nuovi tipi

---

### Step 8.5: Modifica GeneticAlgorithm (1 ora)
**Obiettivo**: GA orchestrator con nuovi tipi

**Azioni**:
1. Verificare compatibilità con nuovi tipi
2. Aggiornare statistiche se necessario

**File da modificare**:
- `deepnest-cpp/include/deepnest/algorithm/GeneticAlgorithm.h`
- `deepnest-cpp/src/algorithm/GeneticAlgorithm.cpp`

**Test**: GA completo funziona
**Verifica**: Generazioni corrette

---

## FASE 9: ENGINE E INTERFACCE

### Step 9.1: Modifica NestingEngine (2 ore)
**Obiettivo**: Engine usa coordinate intere

**Azioni**:
1. Passare inputScale a tutte le conversioni
2. Aggiornare gestione spacing (ora intero)
3. Aggiornare callbacks per gestire scalatura in output

**File da modificare**:
- `deepnest-cpp/include/deepnest/engine/NestingEngine.h`
- `deepnest-cpp/src/engine/NestingEngine.cpp`

**Test**: Engine completo funziona
**Verifica**: Usa inputScale correttamente

---

### Step 9.2: Modifica DeepNestSolver (2 ore)
**Obiettivo**: Interfaccia pubblica con scalatura

**Azioni**:
1. Aggiornare metodi pubblici per accettare/restituire double
2. Applicare scalatura all'input
3. Applicare de-scalatura all'output
4. Documentare comportamento

**File da modificare**:
- `deepnest-cpp/include/deepnest/DeepNestSolver.h`
- `deepnest-cpp/src/deepnest/DeepNestSolver.cpp`

**Interfaccia**:
```cpp
class DeepNestSolver {
public:
    // Input: QPainterPath con double (unità utente)
    // Internamente: converti con inputScale
    void addPart(const QPainterPath& path, int quantity, const QString& name);
    void addSheet(const QPainterPath& path, int quantity, const QString& name);

    // Output: Risultati con double (unità utente)
    // Internamente: de-scala prima di restituire
    const NestResult* getBestResult() const;
};
```

**Test**: API pubblica funziona con double
**Verifica**: Scalatura trasparente all'utente

---

## FASE 10: AGGIORNAMENTO TEST E VALIDAZIONE

### Step 10.1: Aggiornamento RandomShapeGenerator (1 ora)
**Obiettivo**: Generator crea forme con coordinate intere

**Azioni**:
1. Generare forme già in coordinate intere
2. O generare in double e scalare

**File da modificare**:
- `deepnest-cpp/tests/RandomShapeGenerator.h`
- `deepnest-cpp/tests/RandomShapeGenerator.cpp`

**Test**: Forme generate corrette
**Verifica**: Coordinate intere

---

### Step 10.2: Aggiornamento TestApplication (2 ore)
**Obiettivo**: GUI test app funziona con nuova libreria

**Azioni**:
1. Aggiornare caricamento SVG per usare inputScale
2. Aggiornare visualizzazione (de-scalare per display)
3. Aggiornare configurazione per mostrare inputScale
4. Testare roundtrip completo

**File da modificare**:
- `deepnest-cpp/tests/TestApplication.h`
- `deepnest-cpp/tests/TestApplication.cpp`
- `deepnest-cpp/tests/ConfigDialog.cpp`

**Test**: GUI carica, visualizza, nesta correttamente
**Verifica**: Scalatura invisibile all'utente

---

### Step 10.3: Aggiornamento Tutti i Test (4 ore)
**Obiettivo**: Tutti i test passano con nuova implementazione

**Azioni**:
Per ogni file di test:
1. Aggiornare per usare inputScale dove necessario
2. Aggiornare tolleranze da double a int
3. Aggiornare aspettative per matematica intera
4. Verificare che test passino

**File da modificare**:
- `deepnest-cpp/tests/StepVerificationTests.cpp`
- `deepnest-cpp/tests/GeometryValidationTests.cpp`
- `deepnest-cpp/tests/NFPValidationTests.cpp`
- `deepnest-cpp/tests/MinkowskiSumComparisonTest.cpp`
- `deepnest-cpp/tests/FitnessTests.cpp`
- `deepnest-cpp/tests/GeneticAlgorithmTests.cpp`
- `deepnest-cpp/tests/OverlapDetectionTest.cpp`
- `deepnest-cpp/tests/DeepnestLibTests.cpp`
- Tutti gli altri test

**Test**: Tutti i test passano
**Verifica**: 100% test success

---

### Step 10.4: Test di Regressione (2 ore)
**Obiettivo**: Verificare che risultati siano comparabili con versione double

**Azioni**:
1. Eseguire test con forme di riferimento
2. Confrontare risultati con versione precedente
3. Verificare che qualità nesting sia mantenuta
4. Verificare che non ci siano regressioni prestazionali

**Test**: Nesting quality >= versione precedente
**Verifica**: Performance accettabili

---

### Step 10.5: Documentazione (2 ore)
**Obiettivo**: Documentare la conversione

**Azioni**:
1. Aggiornare README.md
2. Aggiornare commenti nei file
3. Creare guida migrazione
4. Documentare scelta inputScale

**File da modificare**:
- `deepnest-cpp/README.md`
- Questo file (INTEGER_CONVERSION_PLAN.md)
- Eventuali guide utente

**Deliverable**: Documentazione completa

---

## FASE 11: OTTIMIZZAZIONE E FINALIZZAZIONE

### Step 11.1: Ottimizzazione Performance (3 ore)
**Obiettivo**: Ottimizzare per performance

**Azioni**:
1. Profilare codice per trovare bottleneck
2. Ottimizzare calcoli interi (usare shift invece di divisione per potenze di 2)
3. Verificare assenza overflow
4. Ottimizzare memory layout
5. Parallelizzazione se necessario

**Test**: Benchmark performance
**Verifica**: Performance >= versione double

---

### Step 11.2: Gestione Edge Cases (2 ore)
**Obiettivo**: Gestire casi limite

**Azioni**:
1. Testare con inputScale molto grandi/piccoli
2. Testare con forme molto grandi
3. Testare con forme molto piccole
4. Testare overflow handling
5. Aggiungere asserts/validazione

**Test**: Edge cases gestiti correttamente
**Verifica**: Nessun crash o comportamento indefinito

---

### Step 11.3: Code Review e Cleanup (2 ore)
**Obiettivo**: Codice pulito e manutenibile

**Azioni**:
1. Review completo del codice
2. Rimuovere commenti obsoleti
3. Standardizzare stile
4. Verificare naming conventions
5. Verificare documentazione inline

**Deliverable**: Codice production-ready

---

## RIEPILOGO TEMPISTICHE

| Fase | Descrizione | Tempo Stimato |
|------|-------------|---------------|
| 1 | Preparazione e Analisi | 1.5 ore |
| 2 | Pulizia del Codice | 3 ore |
| 3 | Modifica Tipi di Base | 4 ore |
| 4 | Conversioni SVG/QPainterPath | 7 ore |
| 5 | Geometria di Base | 6 ore |
| 6 | Operazioni sui Poligoni | 6 ore |
| 7 | Calcolo NFP | 5.5 ore |
| 8 | Placement e GA | 8 ore |
| 9 | Engine e Interfacce | 4 ore |
| 10 | Test e Validazione | 11 ore |
| 11 | Ottimizzazione | 7 ore |
| **TOTALE** | | **~63 ore** |

Stima: **8-10 giorni lavorativi** (8 ore/giorno)

---

## NOTE TECNICHE IMPORTANTI

### Gestione Overflow
Con int64_t, overflow è possibile in:
- `x * x + y * y` (distanza al quadrato)
- Area del poligono
- Cross product

**Soluzione**: Cast a int64_t prima di moltiplicazioni, verificare range

### Scelta inputScale
Valori raccomandati:
- **1000.0**: Per mm con precisione 0.001 mm (1 micron)
- **10000.0**: Per mm con precisione 0.0001 mm (0.1 micron) - **RACCOMANDATO**
- **100000.0**: Per alta precisione, rischio overflow

Con int64_t:
- Max valore: 9,223,372,036,854,775,807
- Con scale=10000 e coordinate max=922,337,203 unità utente (~922km)
- Con scale=10000 e forme max=10m, ampio margine per calcoli

### Rotazioni
Opzioni:
1. **Lookup table**: Per angoli 0°, 90°, 180°, 270° (esatti)
2. **Fixed-point**: Per angoli arbitrari
3. **Double intermediario**: Calcolare in double, arrotondare a int64_t

**Raccomandazione**: Lookup table per angoli comuni + double per altri

### Tolleranze
Tutte le tolleranze diventano intere relative a scala:
- `TOL = 1` (1 unità intera = 0.0001 unità utente con scale=10000)
- `spacing` in unità intere
- `curveTolerance` in unità intere

---

## CHECKLIST FINALE

Prima di considerare la conversione completa, verificare:

- [ ] Tutti i file compilano senza warning
- [ ] Tutti i test passano (100%)
- [ ] Nessun uso di double/float nelle coordinate (eccetto conversioni I/O)
- [ ] inputScale funziona correttamente
- [ ] Scalatura è trasparente all'utente
- [ ] noFitPolygon rimosso completamente
- [ ] Codice non utilizzato rimosso
- [ ] Performance >= versione precedente
- [ ] Qualità nesting >= versione precedente
- [ ] Documentazione aggiornata
- [ ] README aggiornato
- [ ] Esempio funzionante
- [ ] GUI test app funziona
- [ ] Nessun overflow in casi normali
- [ ] Edge cases gestiti
- [ ] Code review completato

---

## PRIORITÀ E ORDINE DI ESECUZIONE

Seguire l'ordine degli step per minimizzare problemi di dipendenze.

**Critico**: Non saltare step, procedere sequenzialmente.

**Blockers**:
- Step 3 blocca tutto (tipi base)
- Step 4 blocca I/O
- Step 5 blocca geometria
- Step 7 blocca NFP
- Step 8 blocca placement

---

## RISCHI E MITIGAZIONI

| Rischio | Probabilità | Impatto | Mitigazione |
|---------|-------------|---------|-------------|
| Overflow in calcoli | Media | Alto | Usare int64_t, validazione input, test |
| Perdita precisione | Bassa | Medio | inputScale appropriato (10000) |
| Regressione performance | Bassa | Medio | Profiling, ottimizzazione |
| Regressione qualità | Bassa | Alto | Test di confronto, validazione |
| Rotazioni imprecise | Media | Medio | Lookup table, test specifici |
| Test non aggiornati | Alta | Medio | Aggiornare sistematicamente |

---

Fine del piano dettagliato.
