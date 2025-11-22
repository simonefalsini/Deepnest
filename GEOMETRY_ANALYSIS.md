# Analisi Funzioni Geometria - DeepNest C++

## Scopo
Analisi sistematica delle funzioni di geometria computazionale in `deepnest-cpp/include/deepnest/geometry` per identificare opportunità di sostituzione con librerie consolidate (Clipper2, Boost.Geometry, OpenCV) e pianificare la validazione del codice esistente.

## Librerie Disponibili
- **Clipper2**: Già integrata - operazioni booleane, offset, semplificazione poligoni
- **Boost**: thread, system (possibilità di aggiungere Boost.Geometry)
- **Qt5**: Core, Gui, Widgets
- **OpenCV**: Non ancora integrato (ultima risorsa)

---

## Categorizzazione Funzioni

### CATEGORIA 1: Funzioni Base (Alta priorità per sostituzione)

#### 1.1 ConvexHull.h/cpp - **SOSTITUIBILE**
**Funzioni attuali:**
- `computeHull()` - Graham's Scan algorithm
- `computeHullFromPolygon()`
- Helper: `findAnchorPoint()`, `findPolarAngle()`, `crossProduct()`, `isCCW()`

**Sostituzione proposta:**
- **Boost.Geometry**: `boost::geometry::convex_hull()`
- **Clipper2**: Non offre convex hull diretto
- **OpenCV**: `cv::convexHull()` (se necessario)

**Priorità**: ALTA - Funzionalità standard ben testata in librerie mature

**Azioni richieste:**
1. Creare test di validazione con casi noti
2. Confrontare risultati tra implementazione attuale e Boost.Geometry
3. Sostituire con Boost.Geometry se i risultati corrispondono

---

#### 1.2 Transformation.h/cpp - **MANTENERE (ma validare)**
**Funzioni attuali:**
- Matrice affine 2D completa
- `translate()`, `scale()`, `rotate()`, `skewX()`, `skewY()`
- `combine()` - composizione trasformazioni
- `apply()` - applicazione a punti/poligoni

**Valutazione:**
- Implementazione custom ben strutturata
- Boost non ha equivalente diretto semplice
- OpenCV ha `cv::Mat` affine transforms ma dipendenza pesante
- **DECISIONE**: MANTENERE implementazione custom

**Azioni richieste:**
1. Test unitari per ogni operazione
2. Test composizione trasformazioni multiple
3. Validazione numerica (precisione, stabilità)

---

### CATEGORIA 2: Operazioni Poligoni (Parzialmente sostituibili)

#### 2.1 PolygonOperations.h/cpp - **GIÀ USA CLIPPER2**
**Funzioni attuali:**
- `offset()` - **USA Clipper2** ✓
- `cleanPolygon()` - **USA Clipper2** ✓
- `simplifyPolygon()` - **USA Clipper2** ✓
- `unionPolygons()` - **USA Clipper2** ✓
- `intersectPolygons()` - **USA Clipper2** ✓
- `differencePolygons()` - **USA Clipper2** ✓
- `area()` - **USA Clipper2** ✓
- `reversePolygon()` - semplice reverse

**Valutazione**: **OTTIMO** - Già usa libreria consolidata

**Azioni richieste:**
1. Test di regressione per tutte le operazioni
2. Test casi edge (poligoni degeneri, self-intersecting)
3. Benchmark performance

---

#### 2.2 GeometryUtil.h - Funzioni Poligono

**2.2.1 SOSTITUIBILI:**

- `polygonArea()` → **Boost.Geometry**: `boost::geometry::area()`
  - Priorità: MEDIA (già funzionante, sostituzione non urgente)

- `pointInPolygon()` → **Boost.Geometry**: `boost::geometry::within()`
  - Priorità: ALTA (funzione critica, libreria più robusta)

**2.2.2 CUSTOM (validare):**

- `getPolygonBounds()` - Semplice, probabilmente efficiente
  - **Boost alternativa**: `boost::geometry::envelope()`
  - Priorità: BASSA (funzione semplice)

- `intersect()` - Check intersezione due poligoni
  - **Boost alternativa**: `boost::geometry::intersects()`
  - Priorità: ALTA (funzione critica)

- `isRectangle()` - Check se poligono è rettangolo
  - Custom logica, mantenere
  - Priorità: Test di validazione

- `rotatePolygon()` - Rotazione poligono
  - Può usare `Transformation` class
  - Priorità: MEDIA (refactor per usare Transformation)

---

### CATEGORIA 3: Funzioni Geometriche Base (Mantenere e validare)

#### 3.1 Funzioni Punto/Segmento - **MANTENERE**

**GeometryUtil.h:**
- `almostEqual()`, `almostEqualPoints()`, `withinDistance()`
  - Funzioni di tolleranza numerica - **CRITICHE**
  - **MANTENERE** - logica custom necessaria per coerenza

- `normalizeVector()`
  - Semplice, efficiente
  - **Boost alternativa**: `boost::geometry::normalize()`
  - Priorità: BASSA

- `onSegment()` - Punto su segmento
  - Logica custom con tolleranza
  - **MANTENERE** e validare

- `lineIntersect()` - Intersezione segmenti
  - **Boost alternativa**: `boost::geometry::intersection()`
  - Priorità: MEDIA (validare contro Boost)

**Azioni richieste:**
1. Test unitari con casi noti
2. Test casi edge (segmenti collineari, paralleli, coincidenti)
3. Confronto con Boost.Geometry dove applicabile

---

### CATEGORIA 4: Curve (Bezier, Arc) - **MANTENERE E VALIDARE**

#### 4.1 GeometryUtil.h - QuadraticBezier namespace
- `isFlat()` - Roger Willcocks criterion
- `linearize()` - Conversione a segmenti
- `subdivide()` - Suddivisione curva

#### 4.2 GeometryUtil.h - CubicBezier namespace
- `isFlat()`
- `linearize()`
- `subdivide()`

#### 4.3 GeometryUtil.h - Arc namespace
- `linearize()` - Arc → segmenti
- `centerToSvg()`, `svgToCenter()` - Conversioni formato

**Valutazione:**
- Nessuna libreria tra Clipper2/Boost offre queste funzionalità
- OpenCV ha limitate funzionalità per curve
- **DECISIONE**: MANTENERE implementazione custom

**Azioni richieste:**
1. Test con curve SVG note
2. Validazione contro riferimenti SVG standard
3. Test accuratezza linearizzazione
4. Test conversioni arc format

---

### CATEGORIA 5: Funzioni NFP Avanzate - **CORE BUSINESS - VALIDARE ESTENSIVAMENTE**

#### 5.1 GeometryUtil.h + GeometryUtilAdvanced.h - Funzioni NFP

**Funzioni critiche (NO sostituzione, MASSIMA validazione):**

- `polygonEdge()` - Estrazione bordo poligono in direzione
- `pointLineDistance()` - Distanza punto-linea con normale
- `pointDistance()` - Distanza con segno per sliding
- `segmentDistance()` - Distanza minima tra segmenti
- `polygonSlideDistance()` - **CRITICA** - Sliding tra poligoni
- `polygonProjectionDistance()` - Proiezione poligoni
- `searchStartPoint()` - **CRITICA** - Punto inizio NFP
- `noFitPolygon()` - **FUNZIONE PRINCIPALE** - Calcolo NFP orbitale
- `noFitPolygonRectangle()` - NFP ottimizzato per rettangoli
- `polygonHull()` - Hull combinato di poligoni touching

**Valutazione:**
- **NESSUNA libreria** offre queste funzionalità
- Sono il **CORE BUSINESS** dell'algoritmo DeepNest
- Convertite da Java, necessitano validazione estensiva

**Azioni richieste (PRIORITÀ MASSIMA):**
1. ✅ **GIÀ FATTO**: Test orbital tracing in PolygonExtractor
2. Creare suite di test con:
   - Casi semplici noti (triangolo vs triangolo, quadrato vs quadrato)
   - Casi complessi (poligoni concavi, con holes)
   - Casi edge (poligoni touching, overlapping)
   - Confronto con risultati JavaScript originale
3. Validazione numerica (stabilità, precisione)
4. Test performance e ottimizzazione
5. Documentazione algoritmi e invarianti

---

#### 5.2 OrbitalHelpers.cpp - Helper per orbital tracing

**Funzioni:**
- `findTouchingContacts()` - Trova contatti tra poligoni
- `generateTranslationVectors()` - Genera vettori di slide
- `isBacktracking()` - Verifica backtracking

**Valutazione:**
- Helper per `noFitPolygon()`
- **MANTENERE** - parte integrante algoritmo

**Azioni richieste:**
1. Test unitari per ogni helper
2. Validazione logica touching detection
3. Test generazione vettori corretti

---

### CATEGORIA 6: Semplificazione Poligoni

#### 6.1 GeometryUtil.h - Simplify functions

**Funzioni:**
- `simplifyPolygon()` - Ramer-Douglas-Peucker + radial
- `simplifyRadialDistance()` - Primo pass radiale
- `simplifyDouglasPeucker()` - Secondo pass RDP

**Valutazione:**
- **Clipper2** ha `SimplifyPolygon()` (rimuove self-intersections)
- **Boost.Geometry** ha `boost::geometry::simplify()` (Douglas-Peucker)
- Implementazione attuale usa approccio two-pass

**Sostituzione proposta:**
- Confrontare con `boost::geometry::simplify()`
- Se equivalente, sostituire
- Priorità: MEDIA

**Azioni richieste:**
1. Test confronto risultati attuali vs Boost
2. Test qualità semplificazione (shape fidelity)
3. Benchmark performance

---

## Piano di Test Dettagliato

### FASE 1: Test Funzioni Base (Settimana 1)

**1.1 ConvexHull**
```cpp
// Test cases:
- Quadrato → convex hull identico
- Poligono concavo → hull corretto
- Punti collineari
- Punti duplicati
- < 3 punti (edge case)
Confrontare con Boost.Geometry
```

**1.2 Transformation**
```cpp
// Test cases:
- Identità
- Singole trasformazioni (translate, rotate, scale, skew)
- Composizione trasformazioni
- Inversione trasformazioni
- Precisione numerica (1000 trasformazioni composte)
```

**1.3 Funzioni Punto/Segmento**
```cpp
// Test cases:
- lineIntersect: parallele, coincidenti, perpendicolari, oblique
- onSegment: su segmento, fuori, endpoint
- almostEqual: tolleranza numerica
```

---

### FASE 2: Test Operazioni Poligoni (Settimana 2)

**2.1 PolygonOperations (regressione)**
```cpp
// Test cases per ogni operazione:
- offset: positivo, negativo, zero
- union: poligoni disgiunti, overlapping, touching
- intersection: nessuna, parziale, completa
- difference: A-B, B-A
- cleanPolygon: self-intersecting, già pulito
- area: CW vs CCW, zero area
```

**2.2 Polygon utility (Boost comparison)**
```cpp
// Confrontare:
- polygonArea vs boost::geometry::area
- pointInPolygon vs boost::geometry::within
- intersect vs boost::geometry::intersects
- getPolygonBounds vs boost::geometry::envelope
```

---

### FASE 3: Test Curve (Settimana 3)

**3.1 Bezier Curves**
```cpp
// Test linearizzazione:
- Quadratiche: linea retta, curva semplice, curva complessa
- Cubiche: S-curve, loop, cusps
- Tolleranza: confrontare diversi livelli
- Validare contro path SVG standard
```

**3.2 Arc Curves**
```cpp
// Test:
- Conversioni centerArc ↔ svgArc (round-trip)
- Linearizzazione archi
- Archi degeneri (rx=0, ry=0)
- Large arc vs small arc flag
```

---

### FASE 4: Test NFP Avanzate - **PRIORITÀ MASSIMA** (Settimana 4-6)

**4.1 Funzioni di Distanza**
```cpp
// Test specifici:
pointLineDistance:
  - Punto perpendicolare a segmento
  - Punto fuori proiezione
  - Tolleranze endpoint

segmentDistance:
  - Segmenti paralleli
  - Segmenti che si intersecano
  - Segmenti distanti

polygonSlideDistance:
  - Slide semplice (quadrato verso quadrato)
  - Slide complessa (concavi)
  - No collision case
  - Validare contro casi JavaScript noti
```

**4.2 noFitPolygon - Test Sistematici**

**Casi Semplici:**
```cpp
1. Quadrato vs Quadrato (OUTER)
   - NFP atteso: quadrato 2x più grande

2. Triangolo vs Triangolo (OUTER)
   - NFP atteso: esagono

3. Rettangolo vs Punto (OUTER)
   - NFP atteso: rettangolo stesso

4. Test INNER (B inside A)
   - Quadrato grande con quadrato piccolo
   - NFP atteso: quadrato (differenza aree)
```

**Casi Complessi:**
```cpp
5. Poligoni concavi complessi
   - Testare vs JavaScript originale
   - Verificare tutti i loop NFP trovati

6. Poligoni con holes
   - Gestione corretta dei buchi

7. searchEdges=true
   - Trovare tutti gli NFP possibili
```

**Test di Robustezza:**
```cpp
8. Poligoni quasi-touching
9. Poligoni con edge collineari
10. Poligoni degenerati (area ~0)
11. Tolerance numerica critica
```

**4.3 Orbital Tracing Helpers**
```cpp
// Test unitari:
findTouchingContacts:
  - Vertex-vertex touch
  - Vertex on edge
  - Multiple touches
  - No touches

generateTranslationVectors:
  - Vettori generati corretti
  - Lunghezze corrette
  - Polygon marking corretto

isBacktracking:
  - Rilevamento backtrack corretto
  - Edge cases (vettori opposti, perpendicolari)
```

---

### FASE 5: Test Semplificazione (Settimana 7)

**5.1 Polygon Simplification**
```cpp
// Test confronto:
- simplifyPolygon vs boost::geometry::simplify
- Diverse tolleranze
- Shape fidelity (quanta differenza tra originale e semplificato)
- Performance benchmark
```

---

## Strategia di Sostituzione

### Librerie da Integrare

**1. Boost.Geometry (ALTA PRIORITÀ)**
```cmake
# Aggiungere a CMakeLists.txt:
find_package(Boost REQUIRED COMPONENTS thread system geometry)
```

**Funzioni da sostituire:**
- ConvexHull → `boost::geometry::convex_hull()`
- pointInPolygon → `boost::geometry::within()`
- polygonArea → `boost::geometry::area()`
- intersect → `boost::geometry::intersects()`
- lineIntersect → `boost::geometry::intersection()`
- simplifyPolygon → `boost::geometry::simplify()`

**2. OpenCV (BASSA PRIORITÀ - solo se necessario)**
- Solo se Boost.Geometry non sufficiente
- Dipendenza pesante

---

## Programma di Lavoro Proposto

### Settimana 1-2: Setup e Funzioni Base
1. ✅ Analisi codice (COMPLETATO)
2. Integrare Boost.Geometry
3. Creare test framework GeometryTests.cpp
4. Test + sostituzione ConvexHull
5. Test Transformation
6. Test funzioni punto/segmento

### Settimana 3-4: Poligoni e Curve
7. Test regressione PolygonOperations
8. Confronto con Boost.Geometry per funzioni poligono
9. Test curve Bezier e Arc
10. Validazione conversioni SVG

### Settimana 5-7: NFP - CORE (PRIORITÀ MASSIMA)
11. Test sistematici noFitPolygon
12. Confronto con JavaScript originale
13. Test robustezza casi edge
14. Test orbital helpers
15. Performance optimization
16. Documentazione algoritmi

### Settimana 8: Integrazione e Cleanup
17. Sostituzione funzioni validate
18. Refactoring codice
19. Documentazione finale
20. Code review

---

## Metriche di Successo

### Test Coverage
- **Target**: >95% coverage funzioni geometry
- **Critico**: 100% coverage funzioni NFP

### Validazione
- Tutti i test geometrici base passano
- Risultati NFP identici a JavaScript (o migliorati)
- Nessuna regressione in test esistenti

### Performance
- Nessun degrado performance dopo sostituzione
- Possibile miglioramento con librerie ottimizzate

### Codice
- Riduzione LOC con uso librerie standard
- Maggiore manutenibilità
- Migliore documentazione

---

## Rischi e Mitigazioni

### Rischio 1: Incompatibilità Boost.Geometry
**Mitigazione**: Mantenere implementazioni attuali come fallback

### Rischio 2: Regressioni NFP
**Mitigazione**: Test estensivi contro JavaScript + casi noti

### Rischio 3: Performance degradation
**Mitigazione**: Benchmark continui, rollback se necessario

### Rischio 4: Tolleranze numeriche diverse
**Mitigazione**: Configurare tolleranze libraries per match comportamento attuale

---

## Note Finali

**Funzioni NON sostituibili (core business):**
- noFitPolygon() e tutto il sistema orbital tracing
- polygonSlideDistance() e funzioni di distanza correlate
- searchStartPoint()
- Orbital helpers

Queste funzioni richiedono **massima priorità per test e validazione** in quanto sono il cuore dell'algoritmo DeepNest e non hanno equivalenti in librerie standard.

**Funzioni facilmente sostituibili:**
- ConvexHull → Boost.Geometry
- pointInPolygon → Boost.Geometry
- polygonArea → Boost.Geometry
- Funzioni già usando Clipper2 → OK

**Approccio incrementale:**
1. Iniziare con funzioni semplici (convex hull)
2. Procedere verso funzioni intermedie (polygon queries)
3. Validazione estensiva NFP (core business)
4. Sostituzione graduale dove ha senso
5. Mantenere implementazioni custom dove necessario
