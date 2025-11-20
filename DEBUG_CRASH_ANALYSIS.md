# DEBUG CRASH ANALYSIS: Access Violation 0xc0000005

**Data**: 2025-11-20
**Branch**: `claude/deepnest-js-to-cpp-01FL3yqoHLn2AXEAue7G1XYr`

---

## SITUAZIONE ATTUALE

### Il Mutex Non Ha Risolto il Problema

L'utente ha correttamente osservato che il mutex **non ha risolto** il crash:

```
Exception at 0x7ff74b159e51, code: 0xc0000005: read access violation at: 0x10
d:\buildopencv\boost\boost\polygon\detail\polygon_arbitrary_formation.hpp:1768
```

**Conclusione**: **NON è un problema di thread concorrenti**.

Se fosse stato concorrenza, il mutex avrebbe serializzato gli accessi e il crash sarebbe sparito. Ma il crash persiste, quindi il problema è **diverso**.

---

## IPOTESI RIVISTE

### Cosa NON È

❌ Geometrie degenerate semplici (già validato)
❌ Concorrenza tra thread (mutex non ha funzionato)
❌ Problema di scaling/truncation (Phase 1 corretto)

### Cosa POTREBBE ESSERE

1. **Bug in Boost.Polygon con certi input**
   - Alcune combinazioni di geometrie (anche valide) triggherano bug interno
   - Lo scanline algorithm ha edge cases non gestiti
   - Boost.Polygon non è perfetto, ha bug documentati

2. **Lifetime/Dangling References Interne a Boost**
   - Anche in single-thread, Boost potrebbe creare riferimenti temporanei invalidi
   - `.get()` chiama `.clean()` che riorganizza strutture interne
   - Questo potrebbe invalidare puntatori/iteratori usati internamente

3. **Memory Corruption Non Rilevata**
   - Corruzione heap avvenuta prima (in altra operazione)
   - Si manifesta solo quando Boost.Polygon accede a memoria corrotta
   - Difficile da tracciare

4. **Stack Overflow in Scanline Ricorsivo**
   - Con geometrie complesse, l'algoritmo scanline potrebbe andare troppo in profondità
   - Stack overflow su Windows può causare access violation invece di stack overflow exception

---

## DEBUG LOGGING AGGIUNTO

### Obiettivo

Identificare **ESATTAMENTE**:
1. Quale coppia di poligoni (A.id, B.id) causa il crash
2. In quale fase esatta del calcolo (insert, convolve, get)
3. Parametri delle geometrie problematiche (num points, area, etc.)

### Logging Points

**In `calculateNFP()`:**
```cpp
=== calculateNFP START ===
A: id=X, points=N, children=M
B: id=Y, points=P, children=Q
inner=true/false

DEBUG: Converting to Boost integer polygons...
DEBUG: Converted A to Boost polygon
DEBUG: Converted B to Boost polygon
DEBUG: Inserting A into polygon set...
DEBUG: Inserting B into polygon set...
DEBUG: Computing Minkowski convolution...
DEBUG: Minkowski convolution completed
DEBUG: Extracting result from polygon set (calling .get())...
[CRASH LIKELY HERE]
DEBUG: Successfully extracted N NFPs

=== calculateNFP END: SUCCESS, returning N NFPs ===
```

**In `fromBoostPolygonSet()`:**
```cpp
DEBUG: fromBoostPolygonSet called
DEBUG: About to call polySet.get() - THIS IS WHERE CRASH TYPICALLY OCCURS
DEBUG: polySet dirty() = true/false
[CRASH LIKELY HERE - access violation 0x10]
DEBUG: polySet.get() succeeded, extracted N polygons
DEBUG: fromBoostPolygonSet completed successfully, returning N polygons
```

---

## TESTING CON DEBUG LOGGING

### Compilazione

```batch
cd deepnest-cpp\build
cmake .. -G "Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Esecuzione TestApplication con Redirect Output

**IMPORTANTE**: Redirigere stderr a file per catturare tutti i log prima del crash:

```batch
cd tests\Release
TestApplication.exe --test ..\..\testdata\test__SVGnest-output_clean.svg --generations 1 2>&1 > debug_log.txt
```

**Nota**: `2>&1` redirige stderr a stdout, poi `>` salva tutto in file.

### Analisi del Log

**1. Se il programma crasha:**

Aprire `debug_log.txt` e cercare l'**ULTIMA coppia loggata**:

```
=== calculateNFP START ===
A: id=42, points=87, children=0
B: id=15, points=154, children=0
inner=true
DEBUG: Converting to Boost integer polygons...
DEBUG: Converted A to Boost polygon
DEBUG: Converted B to Boost polygon
DEBUG: Inserting A into polygon set...
DEBUG: Inserting B into polygon set...
DEBUG: Computing Minkowski convolution...
DEBUG: Minkowski convolution completed
DEBUG: Extracting result from polygon set (calling .get())...
DEBUG: fromBoostPolygonSet called
DEBUG: About to call polySet.get() - THIS IS WHERE CRASH TYPICALLY OCCURS
DEBUG: polySet dirty() = true

[PROGRAM CRASHES HERE - NO MORE OUTPUT]
```

**Conclusione**: Il crash è causato dalla coppia **(A.id=42, B.id=15)** durante la chiamata `polySet.get()`.

**2. Informazioni da Estrarre:**

- **ID dei poligoni problematici**: A.id e B.id
- **Numero di punti**: Se molto alto (>500) potrebbe indicare complessità eccessiva
- **Inner vs Outer**: Se `inner=true`, è un Inner NFP (sheet vs part)
- **Fase esatta**: Quale DEBUG message è l'ultimo prima del crash

---

## PROSSIME AZIONI (Basate sui Risultati)

### Scenario 1: Crash Sempre sulla Stessa Coppia

**Se il log mostra che il crash è sempre su A.id=X, B.id=Y:**

1. **Estrarre geometrie problematiche** dal SVG per isolare il caso
2. **Creare test minimale** con solo quella coppia
3. **Analizzare geometrie** (self-intersections, topologia non manifold, etc.)
4. **Provare workaround Boost**:
   - Simplificare poligoni prima del calcolo NFP
   - Usare algoritmo diverso (Orbital tracing)
   - Skippare quella coppia specifica

### Scenario 2: Crash su Coppie Diverse

**Se il crash avviene su coppie diverse in run successive:**

Indica **memory corruption** o **heap corruption**:

1. **Valgrind/Address Sanitizer** (su Linux/WSL):
   ```bash
   cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address"
   ```

2. **Windows Application Verifier** (su Windows):
   - Abilita heap checks
   - Rileva corruzioni prima che causino crash

3. **Analizzare operazioni prima del crash**:
   - Buffer overflows in conversioni
   - Out-of-bounds access in loops

### Scenario 3: Crash Durante `convolve_two_polygon_sets()`

**Se l'ultimo message è "Computing Minkowski convolution..." (non arriva a .get()):**

Il problema è nell'algoritmo di convoluzione stesso:

1. **Limitare complessità input**:
   ```cpp
   if (A.points.size() > 300 || B.points.size() > 300) {
       std::cerr << "WARNING: Polygon too complex, skipping NFP" << std::endl;
       return std::vector<Polygon>();
   }
   ```

2. **Simplificare poligoni prima**:
   - Applicare Douglas-Peucker con tolerance alta (5-10 units)
   - Ridurre numero di punti mantenendo forma generale

### Scenario 4: Crash Durante `insert()`

**Se crash tra "Inserting A/B into polygon set..." e "Computing...":**

Problema nella costruzione del polygon set:

1. **Verificare validità Boost polygon**:
   ```cpp
   if (!boostA.is_valid()) {
       std::cerr << "ERROR: Invalid Boost polygon A" << std::endl;
       return empty;
   }
   ```

2. **Controllare winding order**:
   - Boost richiede CCW (counter-clockwise)
   - Verificare dopo negazione e reverse

---

## WORKAROUNDS POSSIBILI

### Workaround 1: Fallback a Orbital Tracing

Se Minkowski continua a crashare, usare Orbital tracing:

```cpp
std::vector<Polygon> nfps = calculateNFP(A, B, inner);
if (nfps.empty()) {
    // Fallback: try orbital tracing instead
    std::cerr << "INFO: Minkowski failed for pair (" << A.id << ", " << B.id
              << "), trying Orbital tracing" << std::endl;
    nfps = OrbitalNFP::calculateNFP(A, B, inner);
}
```

### Workaround 2: Pre-Simplification

Semplificare poligoni **prima** di calcolare NFP:

```cpp
Polygon A_simplified = simplifyPolygon(A, tolerance=5.0);
Polygon B_simplified = simplifyPolygon(B, tolerance=5.0);
nfps = calculateNFP(A_simplified, B_simplified, inner);
```

### Workaround 3: Skip Problematic Pairs

Se certe coppie crashano sempre, skipparle:

```cpp
// Blacklist di coppie problematiche (da debug log)
std::set<std::pair<int,int>> blacklist = {{42, 15}, {87, 23}};

if (blacklist.count({A.id, B.id})) {
    std::cerr << "WARNING: Skipping known problematic pair ("
              << A.id << ", " << B.id << ")" << std::endl;
    return std::vector<Polygon>();
}
```

### Workaround 4: Usare Libreria Alternativa

Se Boost.Polygon è troppo instabile:

1. **Clipper2** (C++, header-only):
   - Più stabile di Boost
   - Supporta Minkowski sum
   - Migliore gestione edge cases

2. **CGAL** (C++, complesso):
   - Libreria geometrica industriale
   - Molto robusta ma pesante
   - Curva di apprendimento ripida

---

## INFORMAZIONI DA FORNIRE

Quando esegui il test con debug logging, fornisci:

1. **File `debug_log.txt` completo** (soprattutto le ultime 50 righe)
2. **Ultima coppia loggata prima del crash**:
   - A.id = ?
   - B.id = ?
   - inner = ?
   - points A = ?
   - points B = ?
3. **Fase esatta del crash** (ultimo DEBUG message)
4. **Se crash è ripetibile** sulla stessa coppia o varia

### Esempio Output Atteso

**Caso Ideale (nessun crash)**:
```
=== calculateNFP START ===
A: id=0, points=45, children=0
B: id=1, points=67, children=0
inner=false
...
=== calculateNFP END: SUCCESS, returning 1 NFPs ===

=== calculateNFP START ===
A: id=0, points=45, children=0
B: id=2, points=34, children=0
...
[Continua per tutte le coppie senza crash]
```

**Caso Crash (fornire questo)**:
```
=== calculateNFP START ===
A: id=sheet, points=4, children=0
B: id=part_42, points=154, children=0
inner=true
DEBUG: Converting to Boost integer polygons...
DEBUG: Converted A to Boost polygon
DEBUG: Converted B to Boost polygon
DEBUG: Inserting A into polygon set...
DEBUG: Inserting B into polygon set...
DEBUG: Computing Minkowski convolution...
DEBUG: Minkowski convolution completed
DEBUG: Extracting result from polygon set (calling .get())...
DEBUG: fromBoostPolygonSet called
DEBUG: About to call polySet.get() - THIS IS WHERE CRASH TYPICALLY OCCURS
DEBUG: polySet dirty() = true

[CRASH - NO MORE OUTPUT]
[Exception 0xc0000005 in polygon_arbitrary_formation.hpp:1768]
```

---

## CONCLUSIONE

Il mutex non ha risolto il problema perché **non era un problema di threading**.

Il crash `access violation at 0x10` in `polygon_arbitrary_formation.hpp:1768` indica che **Boost.Polygon stesso ha un bug** o **incontra un edge case non gestito** con certi input.

**Prossimo step**: Eseguire test con debug logging per identificare **esattamente quale coppia** di poligoni causa il crash, poi decidere strategia di fix:
- Workaround (skip, fallback, simplify)
- Libreria alternativa (Clipper2, CGAL)
- Fix upstream Boost (se troviamo il bug specifico)

---

**Commit ready for**: Debug logging extensive per crash analysis
**Branch**: `claude/deepnest-js-to-cpp-01FL3yqoHLn2AXEAue7G1XYr`
**Ready for**: Test run con output redirect e analisi log crash
