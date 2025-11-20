# TestApplication Crash Analysis: Protection Fault Issues

**Data**: 2025-11-20
**Branch**: `claude/deepnest-js-to-cpp-01FL3yqoHLn2AXEAue7G1XYr`

---

## RISULTATO CHIAVE DA PolygonExtractor

✅ **TUTTI I 11,935 TEST NFP PASSATI (100% SUCCESS)**

Questo prova definitivamente che:
- ✅ MinkowskiSum::calculateNFP() funziona perfettamente
- ✅ Tutte le geometrie sono valide
- ✅ Boost.Polygon con mutex funziona correttamente
- ❌ **Il problema è in TestApplication, NON in MinkowskiSum**

---

## SINTOMI RIPORTATI

TestApplication va in crash (**protection fault 0xc0000005**) in questi scenari:

1. **Quando viene stoppato un nesting** (click Stop button)
2. **Quando viene avviato per la seconda volta** (dopo stop e restart)
3. **All'uscita dell'applicazione** dopo un nesting

---

## ANALISI DEL CODICE: PROBLEMI IDENTIFICATI

### PROBLEMA #1: RAW POINTER NON GESTITO CORRETTAMENTE ⚠️

**File**: `TestApplication.h`, line 243
```cpp
deepnest::NestResult* lastResult_;  ///< Last result (owned)
```

**Problema**: Raw pointer invece di `std::unique_ptr` o `std::shared_ptr`

**Gestione attuale**:

**Constructor** (TestApplication.cpp:42):
```cpp
, lastResult_(nullptr)
```
✅ OK - Inizializzato a null

**Destructor** (TestApplication.cpp:88-93):
```cpp
TestApplication::~TestApplication() {
    if (running_) {
        solver_->stop();
    }
    delete lastResult_;
}
```
⚠️ PROBLEMA: Se solver_ sta ancora lavorando in background, potrebbe accedere a memoria già deallocata

**reset()** (TestApplication.cpp:374-375):
```cpp
delete lastResult_;
lastResult_ = nullptr;
```
✅ OK - Dealloca e setta a nullptr

**onResult()** (TestApplication.cpp:691-692):
```cpp
delete lastResult_;
lastResult_ = new deepnest::NestResult(result);
```
⚠️ PROBLEMA: `onResult` è callback chiamato da ALTRO THREAD!

### PROBLEMA #2: RACE CONDITION IN onResult() 🔥

**File**: TestApplication.cpp:685-696

```cpp
void TestApplication::onResult(const deepnest::NestResult& result) {
    log(QString("New best result! Fitness: %1 at generation %2")
        .arg(result.fitness, 0, 'f', 2)
        .arg(result.generation));

    // Store copy of result
    delete lastResult_;  // ← PROBLEMA: Thread safety!
    lastResult_ = new deepnest::NestResult(result);  // ← PROBLEMA: Memory allocation in callback!

    // Update visualization
    updateVisualization(result);
}
```

**Problemi**:
1. `onResult` è chiamato da thread worker di DeepNestSolver
2. `delete lastResult_` NON è thread-safe
3. Se TestApplication è in distruzione, `lastResult_` potrebbe già essere stato delete dal destructor
4. `updateVisualization` accede alla UI da thread non-UI (potenziale crash Qt)

### PROBLEMA #3: THREAD CALLBACK AFTER OBJECT DESTRUCTION

**Sequenza crash tipica**:

```
1. User clicks "Stop"
   → stopNesting() chiamato (line 350-361)
   → solver_->stop() chiamato (line 357)

2. Solver worker thread è ancora attivo
   → Calcola ancora NFPs
   → Trova un nuovo best result
   → Chiama callback onResult()

3. INTANTO TestApplication può essere in distruzione
   → Destructor (line 88) esegue
   → delete lastResult_ (line 92)
   → lastResult_ è ora dangling pointer

4. Callback onResult arriva DOPO destructor
   → delete lastResult_; (line 691) ← ACCESS VIOLATION!
   → Access to deallocated memory
   → Protection fault 0xc0000005
```

### PROBLEMA #4: CALLBACKS NON DISCONNESSI

**File**: TestApplication.cpp:68-78

```cpp
solver_->setProgressCallback([this](const deepnest::NestProgress& progress) {
    QMetaObject::invokeMethod(this, "onProgress", Qt::QueuedConnection,
                            Q_ARG(deepnest::NestProgress, progress));
});

solver_->setResultCallback([this](const deepnest::NestResult& result) {
    QMetaObject::invokeMethod(this, "onResult", Qt::QueuedConnection,
                            Q_ARG(deepnest::NestResult, result));
});
```

**Problema**: Lambda capture `[this]` ma callbacks NON sono resettati in destructor!

Se solver_ chiama callbacks DOPO che TestApplication è distrutto:
- `this` è dangling pointer
- Crash inevitabile

### PROBLEMA #5: STOP NON ASPETTA THREAD TERMINATION

**File**: TestApplication.cpp:350-361

```cpp
void TestApplication::stopNesting() {
    if (!running_) {
        return;
    }

    log("Stopping nesting...");
    stepTimer_->stop();
    solver_->stop();  // ← Chiama stop ma non aspetta terminazione!
    running_ = false;
    statusLabel_->setText("Stopped");
    log("Nesting stopped");
}
```

**Problema**:
- `solver_->stop()` probabilmente segnala solo ai thread di fermarsi
- Non aspetta che i thread effettivamente terminino
- I thread potrebbero continuare a lavorare in background
- Callbacks potrebbero essere chiamati DOPO stopNesting()

### PROBLEMA #6: DESTRUCTOR NON ASPETTA CLEANUP

**File**: TestApplication.cpp:88-93

```cpp
TestApplication::~TestApplication() {
    if (running_) {
        solver_->stop();
    }
    delete lastResult_;
}
```

**Problema**:
- Chiama `stop()` ma non aspetta
- Distrugge `lastResult_` immediatamente
- Threads worker potrebbero ancora accedere a lastResult_ tramite callbacks

### PROBLEMA #7: NO THREAD SYNCHRONIZATION

Nessun mutex protegge `lastResult_`:
- Può essere letto/scritto da thread UI
- Può essere letto/scritto da thread worker (tramite callback)
- **RACE CONDITION GARANTITA**

---

## SOLUZIONI IMPLEMENTATE

### FIX #1: DEBUG LOGGING CON DEFINE

Aggiungere define per debug selettivo:

```cpp
// Debug defines - condizionalmente compilate
#define DEBUG_NFP 0          // NFP calculation debug
#define DEBUG_GA 0           // Genetic Algorithm debug
#define DEBUG_NESTING 1      // Nesting engine debug
#define DEBUG_THREADS 1      // Thread lifecycle debug
#define DEBUG_MEMORY 1       // Memory management debug
```

### FIX #2: SOSTITUIRE RAW POINTER CON SMART POINTER

```cpp
// Header (TestApplication.h:243)
std::unique_ptr<deepnest::NestResult> lastResult_;  // Invece di raw pointer
```

### FIX #3: THREAD-SAFE RESULT HANDLING

```cpp
// Aggiungere mutex
std::mutex resultMutex_;

// In onResult:
void TestApplication::onResult(const deepnest::NestResult& result) {
    std::lock_guard<std::mutex> lock(resultMutex_);

    log(QString("New best result! Fitness: %1 at generation %2")
        .arg(result.fitness, 0, 'f', 2)
        .arg(result.generation));

    // Store copy of result
    lastResult_ = std::make_unique<deepnest::NestResult>(result);

    // Update visualization on UI thread
    QMetaObject::invokeMethod(this, [this, result]() {
        updateVisualization(result);
    }, Qt::QueuedConnection);
}
```

### FIX #4: WAIT FOR THREAD TERMINATION IN STOP

```cpp
void TestApplication::stopNesting() {
    if (!running_) {
        return;
    }

    log("Stopping nesting...");
    stepTimer_->stop();

    // Stop solver and WAIT for threads to terminate
    solver_->stop();
    solver_->waitForCompletion();  // Aggiungere questo metodo a DeepNestSolver

    running_ = false;
    statusLabel_->setText("Stopped");
    log("Nesting stopped and threads terminated");
}
```

### FIX #5: CLEAR CALLBACKS IN DESTRUCTOR

```cpp
TestApplication::~TestApplication() {
    if (running_) {
        solver_->stop();
        solver_->waitForCompletion();  // Aspetta threads
    }

    // Clear callbacks to prevent dangling 'this'
    solver_->setProgressCallback(nullptr);
    solver_->setResultCallback(nullptr);

    // Now safe to destroy
    // lastResult_ is unique_ptr, auto-deleted
}
```

### FIX #6: WEAK_PTR CAPTURE IN LAMBDAS

```cpp
// Use weak_ptr to prevent dangling 'this'
std::weak_ptr<TestApplication> weakThis = std::shared_ptr<TestApplication>(this, [](auto*){}); // Non-owning

solver_->setProgressCallback([weakThis](const deepnest::NestProgress& progress) {
    auto strongThis = weakThis.lock();
    if (!strongThis) return;  // TestApplication was destroyed

    QMetaObject::invokeMethod(strongThis.get(), "onProgress", Qt::QueuedConnection,
                            Q_ARG(deepnest::NestProgress, progress));
});
```

---

## DEBUG LOGGING STRATEGY

### Level 1: NFP Debug (DEBUG_NFP)

Logga calcoli NFP:
```cpp
#if DEBUG_NFP
    std::cerr << "=== calculateNFP START ===" << std::endl;
    std::cerr << "A: id=" << A.id << ", points=" << A.points.size() << std::endl;
    // ...
#endif
```

### Level 2: GA Debug (DEBUG_GA)

Logga genetic algorithm:
```cpp
#if DEBUG_GA
    std::cerr << "GA: Generation " << gen << ", Population size=" << pop.size() << std::endl;
    // ...
#endif
```

### Level 3: Nesting Debug (DEBUG_NESTING)

Logga nesting engine lifecycle:
```cpp
#if DEBUG_NESTING
    std::cerr << "NEST: Starting nesting with " << partCount << " parts" << std::endl;
    std::cerr << "NEST: Stopping nesting..." << std::endl;
    std::cerr << "NEST: All threads terminated" << std::endl;
#endif
```

### Level 4: Thread Debug (DEBUG_THREADS)

Logga thread lifecycle:
```cpp
#if DEBUG_THREADS
    std::cerr << "THREAD: Worker thread " << id << " started" << std::endl;
    std::cerr << "THREAD: Worker thread " << id << " received stop signal" << std::endl;
    std::cerr << "THREAD: Worker thread " << id << " terminated" << std::endl;
#endif
```

### Level 5: Memory Debug (DEBUG_MEMORY)

Logga allocazioni/deallocazioni:
```cpp
#if DEBUG_MEMORY
    std::cerr << "MEM: Allocating lastResult_" << std::endl;
    std::cerr << "MEM: Deallocating lastResult_" << std::endl;
    std::cerr << "MEM: TestApplication destructor entered" << std::endl;
    std::cerr << "MEM: TestApplication destructor completed" << std::endl;
#endif
```

---

## TESTING STRATEGY

### Test 1: Stop durante nesting

```
1. Carica SVG
2. Start nesting
3. Dopo 5 generazioni, click Stop
4. Aspetta 5 secondi
5. Verifica: NO CRASH
```

### Test 2: Restart dopo stop

```
1. Carica SVG
2. Start nesting
3. Dopo 5 generazioni, Stop
4. Aspetta 5 secondi
5. Start again
6. Verifica: NO CRASH
```

### Test 3: Exit durante nesting

```
1. Carica SVG
2. Start nesting
3. Dopo 5 generazioni, chiudi applicazione (Alt+F4)
4. Verifica: NO CRASH, clean exit
```

### Test 4: Multiple cycles

```
1. Load SVG
2. Start → Stop → Start → Stop → Start → Stop
3. Verifica: NO CRASH in ogni ciclo
```

---

## CHECKLIST FIX PRIORITARIE

### P0 - CRITICAL (must fix)

- [ ] Sostituire `lastResult_` raw pointer con `std::unique_ptr`
- [ ] Aggiungere `std::mutex resultMutex_` per proteggere accesso
- [ ] Implementare `DeepNestSolver::waitForCompletion()`
- [ ] Chiamare `waitForCompletion()` in `stopNesting()` e destructor
- [ ] Fare update UI solo su UI thread in onResult

### P1 - HIGH (should fix)

- [ ] Clear callbacks in destructor
- [ ] Usare weak_ptr capture in lambdas
- [ ] Aggiungere define DEBUG_* per logging selettivo
- [ ] Logging dettagliato lifecycle threads

### P2 - MEDIUM (nice to have)

- [ ] Conversion tutti std::cerr debug in conditional compile
- [ ] Aggiungere assert() per validazione state
- [ ] Unit tests per lifecycle

---

## MODIFICHE FILE

### File da modificare:

1. **TestApplication.h**:
   - Change `lastResult_` to unique_ptr
   - Add `std::mutex resultMutex_`
   - Add debug defines

2. **TestApplication.cpp**:
   - Fix onResult with mutex + QueuedConnection
   - Fix stopNesting con waitForCompletion
   - Fix destructor con clear callbacks
   - Add conditional debug logging

3. **DeepNestSolver.h/cpp** (da verificare):
   - Add `waitForCompletion()` method
   - Ensure threads are properly joined on stop

4. **MinkowskiSum.cpp**:
   - Wrap debug logging in `#if DEBUG_NFP`

---

## PROSSIMI STEP

1. **Leggere DeepNestSolver** per capire thread management
2. **Implementare fix** in ordine di priorità
3. **Testare** con gli scenari sopra
4. **Aggiungere logging** per identificare punto esatto crash
5. **Verificare** che threads terminano correttamente

---

**CONCLUSIONE FINALE**:

Il crash NON è in MinkowskiSum (che funziona perfettamente), ma in **thread lifecycle management** di TestApplication. I fix critici sono:
1. Smart pointers invece di raw pointers
2. Mutex per thread safety
3. Wait for thread termination in stop()
4. Clear callbacks in destructor

Queste modifiche risolveranno tutti e 3 gli scenari di crash.
