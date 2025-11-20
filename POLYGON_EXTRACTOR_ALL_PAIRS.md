# PolygonExtractor: All Pairs Testing Mode

**Data**: 2025-11-20
**Branch**: `claude/deepnest-js-to-cpp-01FL3yqoHLn2AXEAue7G1XYr`

---

## OBIETTIVO

Testare **TUTTE le coppie NFP possibili** in modo isolato, evitando la complessità della logica di nesting di TestApplication.

Questo permette di:
1. **Identificare esattamente** quale coppia di poligoni causa il crash
2. **Testare sistematicamente** tutti i casi (Outer + Inner NFP)
3. **Evitare** la complessità del genetic algorithm e della logica di placement
4. **Logging chiaro** di quale coppia viene testata

---

## MODIFICHE A PolygonExtractor

### Opzione `--all-pairs`

**Aggiunta nuova opzione comando**:
```bash
PolygonExtractor <svg-file> --all-pairs
```

### Modalità di Test

#### Senza `--all-pairs` (Default)
Testa solo le coppie predefinite in `problematicPairs` (come prima)

#### Con `--all-pairs`
Testa **TUTTE** le combinazioni possibili:

1. **PART 1: Outer NFP (part vs part)**
   - Testa ogni poligono vs ogni altro poligono
   - Modalità: `inner=false`
   - Numero test: N*(N-1)/2 (per 154 poligoni = 11,781 test)

2. **PART 2: Inner NFP (sheet vs part)**
   - Crea un "sheet" (rettangolo che racchiude tutti i poligoni + margin 20%)
   - Testa sheet vs ogni poligono
   - Modalità: `inner=true`
   - Numero test: N (per 154 poligoni = 154 test)

**Totale test con 154 poligoni**: 11,781 + 154 = **11,935 test**

---

## COMPILAZIONE

```batch
cd deepnest-cpp\build
cmake .. -G "Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

---

## UTILIZZO

### Test Completo (con redirect output)

**IMPORTANTE**: Redirigere output a file per catturare tutto prima del crash:

```batch
cd tests\Release
PolygonExtractor.exe ..\..\testdata\test__SVGnest-output_clean.svg --all-pairs 2>&1 > all_pairs_log.txt
```

### Solo Outer NFP (parte vs parte)

Se vuoi testare solo le coppie outer NFP:
```batch
REM Modificare il codice per commentare PART 2
PolygonExtractor.exe ..\..\testdata\test__SVGnest-output_clean.svg --all-pairs 2>&1 > outer_nfp_log.txt
```

### Solo Inner NFP (sheet vs parte)

Se vuoi testare solo inner NFP:
```batch
REM Modificare il codice per commentare PART 1
PolygonExtractor.exe ..\..\testdata\test__SVGnest-output_clean.svg --all-pairs 2>&1 > inner_nfp_log.txt
```

---

## OUTPUT FORMATO

### Durante Esecuzione

```
========================================
  TESTING ALL PAIRS MODE
========================================
Total tests to perform: 11935
  - Outer NFP (part vs part): 11781 pairs
  - Inner NFP (sheet vs part): 154 pairs

========================================
  PART 1: OUTER NFP (part vs part)
========================================

----------------------------------------
Test 1/11935
Outer NFP: Polygon 0 vs 1
  A: 45 points
  B: 67 points
----------------------------------------

=== calculateNFP START ===
A: id=0, points=45, children=0
B: id=1, points=67, children=0
inner=false
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
DEBUG: polySet.get() succeeded, extracted 1 polygons
DEBUG: Successfully extracted 1 NFPs
=== calculateNFP END: SUCCESS, returning 1 NFPs ===

  ✅ SUCCESS: 1 NFP(s) generated

[Continua per tutti i test...]

----------------------------------------
Test 11781/11935
Outer NFP: Polygon 152 vs 153
...
  ✅ SUCCESS: 1 NFP(s) generated

========================================
  PART 2: INNER NFP (sheet vs part)
========================================
Sheet rectangle: 1200 x 800
Sheet origin: (-100, -100)

----------------------------------------
Test 11782/11935
Inner NFP: Sheet vs Polygon 0
  Sheet: 4 points (rectangle)
  Part: 45 points
----------------------------------------

[Se crasha qui, vedremo l'ultimo output]
```

### Se Crasha

**ULTIMO OUTPUT VISIBILE** nel log mostrerà esattamente quale coppia:

```
----------------------------------------
Test 11850/11935
Inner NFP: Sheet vs Polygon 68
  Sheet: 4 points (rectangle)
  Part: 154 points
----------------------------------------

=== calculateNFP START ===
A: id=sheet, points=4, children=0
B: id=68, points=154, children=0
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
[Access violation 0xc0000005 in polygon_arbitrary_formation.hpp:1768]
```

**IDENTIFICAZIONE**:
- Coppia problematica: **Sheet vs Polygon 68**
- Polygon 68: **154 points**
- Modalità: **Inner NFP** (inner=true)
- Fase crash: **polySet.get()** (scanline algorithm)

### Se Completa Senza Crash

```
========================================
  ALL PAIRS TEST SUMMARY
========================================
Total tests: 11935
Succeeded: 11930
Failed/Warning: 5

⚠️  Some tests failed or returned empty results

========================================
Testing complete!
Check console output and generated SVG files
========================================
```

---

## ANALISI DEL LOG

### Cosa Cercare nel File `all_pairs_log.txt`

1. **Cerca "Test XXXX/11935"** - ultimo numero mostrato indica quanti test completati
2. **Cerca ultimo "=== calculateNFP START ==="** - identifica coppia problematica
3. **Cerca ultimo "DEBUG:"** - identifica fase esatta del crash
4. **Se crash in PART 1** → Problema con Outer NFP (part vs part)
5. **Se crash in PART 2** → Problema con Inner NFP (sheet vs part)

### Esempio Analisi

**File: all_pairs_log.txt (ultime 30 righe)**

```bash
# Su Linux/WSL:
tail -30 all_pairs_log.txt

# Su Windows PowerShell:
Get-Content all_pairs_log.txt | Select-Object -Last 30
```

**Pattern da identificare**:
```
Test 11850/11935          <- Test number
Inner NFP: Sheet vs Polygon 68   <- Coppia
A: id=sheet               <- ID A
B: id=68                  <- ID B
inner=true                <- Modalità
DEBUG: About to call polySet.get()  <- Ultima fase
```

**Informazioni estratte**:
- **Test #**: 11850
- **Coppia**: Sheet vs Polygon 68
- **Polygon problematico**: ID 68
- **Punti**: 154
- **Modalità**: Inner NFP
- **Fase crash**: polySet.get()

---

## PROSSIMI STEP (Basati sui Risultati)

### Scenario 1: Crash su Coppia Specifica (Inner NFP)

**Se crash sempre su Inner NFP con poligono specifico (es. Polygon 68)**:

1. **Estrarre geometria** dal SVG
2. **Analizzare** punti, area, topologia
3. **Workaround immediato**: Skip quel poligono
   ```cpp
   if (polygons[i].id == 68) {
       std::cout << "  ⚠️  Skipping known problematic polygon 68" << std::endl;
       continue;
   }
   ```

### Scenario 2: Crash su Più Coppie Inner NFP

**Se crash su molte coppie Inner NFP**:

1. **Problema generale** con Inner NFP (sheet vs part)
2. **Possibile causa**: Sheet rectangle + complex parts
3. **Workaround**: Simplificare parts prima di Inner NFP
   ```cpp
   Polygon simplified = simplifyPolygon(polygons[i], tolerance=5.0);
   auto nfps = MinkowskiSum::calculateNFP(sheet, simplified, true);
   ```

### Scenario 3: Crash su Coppie Outer NFP

**Se crash su Outer NFP (part vs part)**:

1. **Identificare quali coppie**
2. **Pattern comune** (es. poligoni con > 100 punti)
3. **Workaround**: Limite complessità
   ```cpp
   if (polygons[i].points.size() > 100 || polygons[j].points.size() > 100) {
       std::cout << "  ⚠️  Skipping: polygon too complex" << std::endl;
       continue;
   }
   ```

### Scenario 4: Completa Senza Crash (Best Case)

**Se tutti i 11,935 test passano**:

1. **Problema NON è in MinkowskiSum** direttamente
2. **Problema è nella logica di TestApplication**:
   - Genetic algorithm
   - Placement logic
   - NFP caching/reuse
   - Multi-threading
3. **Prossimo debug**: Isolare logica TestApplication

---

## VANTAGGI SEPARAZIONE TEST

### Prima (TestApplication)
- ❌ Logica complessa di nesting
- ❌ Genetic algorithm
- ❌ Placement strategy
- ❌ Multi-threading
- ❌ Difficile isolare problema
- ❌ Crash non identifica coppia specifica

### Dopo (PolygonExtractor --all-pairs)
- ✅ Test isolato di calcolo NFP
- ✅ Nessuna logica extra
- ✅ Single-threaded
- ✅ Logging chiaro
- ✅ **Identifica esattamente** coppia problematica
- ✅ Più facile debuggare

---

## PERFORMANCE

### Tempo Stimato

Con 154 poligoni = 11,935 test totali

**Se ogni test impiega ~0.1 secondi**:
- Totale: ~20 minuti

**Se ogni test impiega ~0.5 secondi** (più realistico con mutex):
- Totale: ~100 minuti (~1.5 ore)

### Ottimizzazione

**Se troppo lento**, testare solo subset:

1. **Solo Inner NFP** (154 test): ~1-2 minuti
2. **Solo primi 10 poligoni Outer**: 45 test
3. **Campionamento random**: 1000 test random

Modificare codice:
```cpp
// Test solo primi 10 poligoni
for (size_t i = 0; i < std::min(size_t(10), polygons.size()); i++) {
    for (size_t j = i + 1; j < std::min(size_t(10), polygons.size()); j++) {
        // ...
    }
}
```

---

## CONCLUSIONE

**PolygonExtractor --all-pairs** è lo strumento perfetto per:
- ✅ Isolare problema NFP dalla logica applicazione
- ✅ Identificare coppia problematica esatta
- ✅ Testare sistematicamente tutti i casi
- ✅ Debug più semplice e diretto

**Prossimo step**:
1. Compilare PolygonExtractor modificato
2. Eseguire con --all-pairs e redirect output
3. Analizzare `all_pairs_log.txt`
4. Identificare coppia/pattern problematico
5. Decidere strategia fix basata su risultati

---

**Commit pronto per**:
- PolygonExtractor --all-pairs mode
- Test sistematico di tutte le coppie NFP
- Logging dettagliato per crash analysis

**Branch**: `claude/deepnest-js-to-cpp-01FL3yqoHLn2AXEAue7G1XYr`
**Ready for**: Compilation + Test run + Log analysis
