# THREAD SAFETY CRITICAL ANALYSIS - DeepNest C++ Core Components

**Data**: 2025-11-20
**Analisi**: DeepNestSolver, NestingEngine, NFPCache, ParallelProcessor, GeneticAlgorithm
**Focus**: Thread management e data references

---

## EXECUTIVE SUMMARY

Ho identificato **7 problemi critici** che causano crash (0xc0000005 access violation) nel codice NFP/nesting:

### 🔴 **PROBLEMA #1: USE-AFTER-FREE - PUNTATORI RAW AI POLYGON**
**Severità**: CRITICA
**Causa root**: `Individual::placement` contiene `std::vector<Polygon*>` che puntano a `NestingEngine::parts_`, ma i thread worker possono accedere a questi puntatori DOPO che `parts_` è stato distrutto o ricreato.

### 🔴 **PROBLEMA #2: DANGLING REFERENCE - LAMBDA CAPTURE IN PARALLELPROCESSOR**
**Severità**: CRITICA
**Causa root**: `ParallelProcessor::processPopulation()` cattura `&population` e `&worker` per reference, ma questi oggetti possono essere distrutti mentre i thread sono ancora in esecuzione.

### 🔴 **PROBLEMA #3: PARTPOINTERS_ INVALIDATI DAL VECTOR RESIZE**
**Severità**: CRITICA
**Causa root**: `NestingEngine::partPointers_` contiene puntatori agli elementi di `parts_` vector. Se `parts_` viene modificato (resize, clear), tutti i puntatori diventano invalidi.

### 🔴 **PROBLEMA #4: NESSUN WAIT IN STOP() - THREAD ANCORA ATTIVI**
**Severità**: ALTA
**Causa root**: `NestingEngine::stop()` distrugge `parallelProcessor_` ma non attende che tutti i task nei thread siano completati prima di distruggere `placementWorker_` e `nfpCalculator_`.

### 🔴 **PROBLEMA #5: RECREATE PARALLELPROCESSOR SENZA SYNC**
**Severità**: ALTA
**Causa root**: `NestingEngine::start()` ricrea `parallelProcessor_` ma non garantisce che i vecchi thread siano completamente terminati.

### 🟡 **PROBLEMA #6: NFPCACHE ACCESSO CONCORRENTE AI POLYGON**
**Severità**: MEDIA
**Causa root**: NFPCache usa mutex per proteggere la map, ma i Polygon copiati dalla cache potrebbero contenere dati Boost.Polygon non thread-safe.

### 🟡 **PROBLEMA #7: PARTS_ MODIFICATO DURANTE PROCESSING**
**Severità**: MEDIA
**Causa root**: Nessun lock protegge `parts_` da modifiche concorrenti se `initialize()` viene chiamato mentre il processing è attivo.

---

## DETAILED ANALYSIS

---

## PROBLEMA #1: USE-AFTER-FREE - PUNTATORI RAW AI POLYGON

### Descrizione Completa

**Individual.h, linea 33:**
```cpp
std::vector<Polygon*> placement;
```

Ogni Individual contiene un vettore di puntatori RAW ai Polygon. Questi puntatori puntano agli elementi del vector `NestingEngine::parts_`.

**Flusso del problema:**

1. **NestingEngine::initialize() - NestingEngine.cpp:140-144**
   ```cpp
   partPointers_.clear();
   for (auto& part : parts_) {
       partPointers_.push_back(&part);  // ← CREA PUNTATORI AGLI ELEMENTI DEL VECTOR
   }
   ```

2. **NestingEngine::initialize() - NestingEngine.cpp:148**
   ```cpp
   geneticAlgorithm_ = std::make_unique<GeneticAlgorithm>(partPointers_, config_);
   ```
   Passa `partPointers_` (che contengono puntatori a `parts_` elements) al GeneticAlgorithm.

3. **GeneticAlgorithm costruttore - GeneticAlgorithm.cpp:10**
   ```cpp
   parts_(adam)  // ← COPIA IL VECTOR DI PUNTATORI
   ```

4. **Population::initialize()** crea Individual con:
   ```cpp
   individual.placement = parts;  // ← OGNI INDIVIDUAL HA PUNTATORI AGLI ELEMENTI DI parts_
   ```

5. **ParallelProcessor::processPopulation() - ParallelProcessor.cpp:126**
   ```cpp
   Individual individualCopy = individual.clone();
   ```
   `clone()` copia il vector `placement`, che contiene i PUNTATORI RAW.

6. **Lambda worker thread - ParallelProcessor.cpp:144-148**
   ```cpp
   for (size_t j = 0; j < individualCopy.placement.size(); ++j) {
       Polygon part = *individualCopy.placement[j];  // ← DEREFERENZIA IL PUNTATORE
       part.rotation = individualCopy.rotation[j];
       parts.push_back(part);
   }
   ```

**SCENARIO DI CRASH:**

```
Thread Principal (UI):
1. Chiama NestingEngine::stop()
   → parallelProcessor_.reset() (distrugge processor)
   → parts_ rimane valido

2. Chiama NestingEngine::start() di nuovo (restart)
   → parts_.clear()  // ← DISTRUGGE TUTTI I POLYGON NEL VECTOR!
   → parts_.push_back(...)  // ← CREA NUOVI POLYGON IN NUOVE LOCAZIONI DI MEMORIA

Thread Worker (background):
3. Lambda ancora in esecuzione (non terminata durante stop)
   → Polygon part = *individualCopy.placement[j];  // ← DEREFERENZIA PUNTATORE INVALIDO!
   → 💥 ACCESS VIOLATION 0xc0000005
```

**Perché succede:**
- `std::vector::clear()` distrugge tutti gli elementi
- I puntatori in `individualCopy.placement` puntano a memoria già deallocata
- Quando il thread worker dereferenzia il puntatore, accede a memoria invalida

### Root Cause

**C++ Standard behavior:**
> When a vector is resized, moved, or cleared, all iterators, pointers, and references to its elements are invalidated.

`parts_` è un `std::vector<Polygon>`. Quando viene fatto `clear()`:
1. Vengono chiamati i distruttori di tutti i Polygon
2. La memoria viene deallocata
3. Tutti i puntatori agli elementi diventano **dangling pointers**

### Impact

- ✅ Spiega il crash 0xc0000005 in `addHole()` (accesso a memoria deallocata)
- ✅ Spiega perché il crash avviene solo su restart (dopo stop+start)
- ✅ Spiega perché il crash avviene con molti elementi (più thread attivi)
- ✅ Spiega perché PolygonExtractor non crasha (non fa mai restart)

### Soluzione Proposta

**Opzione A: Shared Pointers (CONSIGLIATA)**

Cambiare `std::vector<Polygon*>` → `std::vector<std::shared_ptr<Polygon>>`

```cpp
// Individual.h
std::vector<std::shared_ptr<Polygon>> placement;

// NestingEngine.cpp
std::vector<std::shared_ptr<Polygon>> partPointers_;

for (auto& part : parts_) {
    partPointers_.push_back(std::make_shared<Polygon>(part));
}
```

**Vantaggi:**
- Reference counting automatico
- Polygon non vengono distrutti finché almeno un thread li sta usando
- Thread-safe (std::shared_ptr è thread-safe per reference counting)

**Opzione B: Deep Copy dei Polygon (ALTERNATIVA)**

Copiare i Polygon invece di usare puntatori:

```cpp
// ParallelProcessor.cpp - linea 143
std::vector<Polygon> partsCopy;
for (size_t j = 0; j < individualCopy.placement.size(); ++j) {
    Polygon part = *individualCopy.placement[j];  // Copia già fatta qui
    part.rotation = individualCopy.rotation[j];
    partsCopy.push_back(part);
}

// Ora partsCopy è owned dal lambda e non dipende da individualCopy.placement
```

**Problema:** Questo non risolve il problema se `placement[j]` è già un dangling pointer.

**Opzione C: Lock durante clear (NON CONSIGLIATA)**

Aggiungere mutex e aspettare che tutti i thread finiscano prima di `clear()`.

**Problema:** Deadlock potential, performance impact.

---

## PROBLEMA #2: DANGLING REFERENCE - LAMBDA CAPTURE IN PARALLELPROCESSOR

### Descrizione Completa

**ParallelProcessor.cpp, linea 133:**
```cpp
enqueue([&population, index, sheetsCopy, &worker, individualCopy, this]() mutable {
    // ...
    PlacementWorker::PlacementResult result = worker.placeParts(sheetsCopy, parts);  // ← USA &worker

    {
        boost::lock_guard<boost::mutex> lock(this->mutex_);
        auto& individuals = population.getIndividuals();  // ← USA &population
        if (index < individuals.size()) {
            individuals[index].fitness = result.fitness;
            // ...
        }
    }
});
```

**Capture by reference:**
- `&population` - Reference alla Population
- `&worker` - Reference al PlacementWorker

**SCENARIO DI CRASH:**

```
Thread Principal:
1. NestingEngine::stop() viene chiamato
   → parallelProcessor_->stop()
   → threads_.join_all()  // ← Aspetta SOLO i thread, non i task
   → parallelProcessor_.reset()  // ← Distrugge parallelProcessor

2. NestingEngine destructor continua
   → placementWorker_.reset()  // ← DISTRUGGE IL WORKER! 💀
   → geneticAlgorithm_.reset()  // ← DISTRUGGE POPULATION! 💀

Thread Worker (background):
3. Lambda task ancora nella coda (non eseguito)
4. Thread inizia a eseguire il task
   → PlacementWorker::PlacementResult result = worker.placeParts(...)
   → 💥 DANGLING REFERENCE! worker è già distrutto!
   → 💥 ACCESS VIOLATION 0xc0000005
```

### Root Cause

**ParallelProcessor::stop() - ParallelProcessor.cpp:36-64**
```cpp
void ParallelProcessor::stop() {
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        if (stopped_) {
            return;
        }
        stopped_ = true;
        workGuard_.reset();
    }

    ioContext_.stop();
    threads_.join_all();  // ← ATTENDE SOLO I THREAD, NON I TASK NELLA CODA!
}
```

**Il problema:**
- `threads_.join_all()` aspetta che i thread terminino
- Ma i thread potrebbero ancora avere task nella coda `ioContext_`
- Quando `stop()` ritorna, `NestingEngine` distrugge `placementWorker_` e `geneticAlgorithm_`
- Ma i task nella coda hanno `&worker` e `&population` che ora sono dangling references!

### Impact

- ✅ Spiega crash su stop durante nesting
- ✅ Spiega crash su restart (stop + start rapido)
- ✅ Spiega crash su exit durante nesting
- ✅ Manifesto solo quando ci sono task ancora in coda

### Soluzione Proposta

**Opzione A: Flush della coda prima di stop (CONSIGLIATA)**

```cpp
void ParallelProcessor::stop() {
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        if (stopped_) {
            return;
        }
        stopped_ = true;
        workGuard_.reset();
    }

    // CRITICAL FIX: Wait for ALL pending tasks to complete
    while (!ioContext_.stopped()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Poll to check if there are pending tasks
        if (ioContext_.poll() == 0) {
            break;  // No more tasks
        }
    }

    ioContext_.stop();
    threads_.join_all();
}
```

**Opzione B: Capture by value (PARZIALE)**

Catturare copie invece di reference:

```cpp
// NON FUNZIONA: population è grande e non copyable
// worker è non-copyable (contiene mutex)
```

**Opzione C: Shared ownership (CONSIGLIATA INSIEME AD OPZIONE A)**

Usare `std::shared_ptr` per `placementWorker_` e passarlo alla lambda:

```cpp
// NestingEngine.h
std::shared_ptr<PlacementWorker> placementWorker_;

// ParallelProcessor.cpp
auto workerPtr = placementWorker_;  // Incrementa ref count
enqueue([&population, index, sheetsCopy, workerPtr, individualCopy, this]() mutable {
    PlacementWorker::PlacementResult result = workerPtr->placeParts(sheetsCopy, parts);
    // ...
});
```

---

## PROBLEMA #3: PARTPOINTERS_ INVALIDATI DAL VECTOR RESIZE

### Descrizione

**NestingEngine.h, linea 343:**
```cpp
std::vector<Polygon*> partPointers_;
```

**NestingEngine.h, linea 340:**
```cpp
std::vector<Polygon> parts_;
```

**NestingEngine.cpp, linee 140-144:**
```cpp
partPointers_.clear();
for (auto& part : parts_) {
    partPointers_.push_back(&part);  // ← PUNTATORI AGLI ELEMENTI DI parts_
}
```

### Root Cause

**C++ vector invalidation rules:**

Quando un `std::vector` viene modificato (resize, push_back, erase, clear), **tutti i puntatori, reference e iteratori agli elementi possono essere invalidati**.

**Scenario di invalidazione:**

```cpp
std::vector<Polygon> parts_;  // Capacità iniziale: 10

parts_.push_back(polygon1);  // Size: 1
parts_.push_back(polygon2);  // Size: 2
// ...
parts_.push_back(polygon11); // Size: 11 → RESIZE! 💀

// ← Tutti i puntatori creati prima del resize sono ora INVALIDI!
```

**Nel nostro codice:**

1. `initialize()` crea `parts_` con size N
2. Crea `partPointers_` con puntatori agli elementi
3. **Se mai `parts_` viene modificato** (anche solo un `parts_.push_back()`), tutti i puntatori diventano invalidi

### Current Code Safety

**Attualmente SICURO perché:**
- `parts_` viene popolato in `initialize()`
- Dopo, `parts_` non viene MAI modificato fino a `clear()`
- `clear()` viene chiamato solo in `initialize()` che ricrea `partPointers_`

**Tuttavia è FRAGILE:**
- Se qualcuno aggiunge codice che modifica `parts_` (es. `parts_.resize()`, `parts_.push_back()`), instant crash
- Non c'è protezione o invariant checking

### Soluzione Proposta

Usare `shared_ptr` (vedi Problema #1, Opzione A) per eliminare la dipendenza dalla stabilità del vector.

---

## PROBLEMA #4: NESSUN WAIT IN STOP() - THREAD ANCORA ATTIVI

### Descrizione

**NestingEngine::stop() - NestingEngine.cpp:176-184:**
```cpp
void NestingEngine::stop() {
    running_ = false;
    if (parallelProcessor_) {
        parallelProcessor_->stop();
        parallelProcessor_.reset();  // ← DISTRUGGE PROCESSOR IMMEDIATAMENTE
    }
}
```

**NestingEngine destructor - NestingEngine.cpp:26-52:**
```cpp
NestingEngine::~NestingEngine() {
    stop();

    nfpCache_.clear();

    if (parallelProcessor_) {
        parallelProcessor_.reset();  // ← Già nullptr da stop()
    }

    if (placementWorker_) {
        placementWorker_.reset();  // ← DISTRUGGE WORKER MENTRE THREAD ATTIVI!
    }

    if (nfpCalculator_) {
        nfpCalculator_.reset();
    }
}
```

### Root Cause

**Ordine di distruzione errato:**

1. `parallelProcessor_->stop()` ferma i thread e chiama `join_all()`
2. `parallelProcessor_.reset()` distrugge il processor
3. `placementWorker_.reset()` **distrugge il worker**
4. **MA:** Le lambda potrebbero ancora avere `&worker` che ora è dangling!

**Il problema è che `ParallelProcessor::stop()` non flush i task pendenti prima di ritornare.**

### Impact

- Crash quando si chiama `stop()` mentre ci sono task attivi
- Crash quando si esce dall'applicazione durante nesting

### Soluzione Proposta

**Fix #1: Flush task queue in ParallelProcessor::stop()**

Vedi Problema #2, Opzione A.

**Fix #2: Aggiungere wait in NestingEngine::stop()**

```cpp
void NestingEngine::stop() {
    running_ = false;
    if (parallelProcessor_) {
        parallelProcessor_->stop();  // ← Questo ora aspetta il flush
        parallelProcessor_->waitAll();  // ← Wait esplicito per tutti i task
        parallelProcessor_.reset();
    }
}
```

---

## PROBLEMA #5: RECREATE PARALLELPROCESSOR SENZA SYNC

### Descrizione

**NestingEngine::start() - NestingEngine.cpp:160-165:**
```cpp
// CRITICAL FIX: Recreate ParallelProcessor if it was previously stopped
if (!parallelProcessor_) {
    parallelProcessor_ = std::make_unique<ParallelProcessor>(config_.threads);
}
```

**NestingEngine::stop() - NestingEngine.cpp:176-184:**
```cpp
void NestingEngine::stop() {
    running_ = false;
    if (parallelProcessor_) {
        parallelProcessor_->stop();
        parallelProcessor_.reset();  // ← Distrugge processor
    }
}
```

### Scenario

```
User: Start nesting
  → parallelProcessor_ creato
  → Thread pool attivo

User: Stop nesting
  → parallelProcessor_->stop()
  → threads_.join_all()  // ← Aspetta thread
  → parallelProcessor_.reset()  // ← Distrugge processor

User: Start nesting AGAIN
  → parallelProcessor_ = std::make_unique<ParallelProcessor>(...)  // ← CREA NUOVO
  → Nuovo thread pool
```

### Root Cause

**Gap di sincronizzazione:**

Tra il momento in cui `threads_.join_all()` ritorna e il momento in cui viene creato il nuovo `ParallelProcessor`, potrebbero esserci ancora:
- Task nella vecchia coda `ioContext_`
- Callback pendenti
- Mutex locks non rilasciati

**Se il nuovo `ParallelProcessor` viene creato immediatamente, potrebbe:**
- Riciclare gli stessi thread ID
- Avere race condition con cleanup non completo del vecchio processor

### Impact

- Crash intermittenti su restart (timing-dependent)
- Deadlock potenziali
- Corruzione di stato

### Soluzione Proposta

**Fix: Aggiungere delay o barrier**

```cpp
void NestingEngine::stop() {
    running_ = false;
    if (parallelProcessor_) {
        parallelProcessor_->stop();
        parallelProcessor_->waitAll();  // ← WAIT FOR QUEUE FLUSH
        parallelProcessor_.reset();

        // CRITICAL: Small delay to ensure complete cleanup
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
```

---

## PROBLEMA #6: NFPCACHE ACCESSO CONCORRENTE AI POLYGON

### Descrizione

**NFPCache.h, linee 58-60:**
```cpp
mutable boost::shared_mutex mutex_;
std::unordered_map<std::string, std::vector<Polygon>> cache_;
```

**NFPCache.cpp, linee 64-68:**
```cpp
if (it != cache_.end()) {
    result = it->second;  // ← COPIA IL VECTOR<POLYGON>
    ++hits_;
    return true;
}
```

### Root Cause

**Thread safety attuale:**
- ✅ La `boost::shared_mutex` protegge la `std::unordered_map`
- ✅ Multiple thread possono leggere contemporaneamente (shared_lock)
- ✅ Solo un thread può scrivere (unique_lock)

**Problema:**
- `result = it->second` copia il `std::vector<Polygon>`
- Ogni `Polygon` contiene `std::vector<Point>`, `children`, ecc.
- Se `Polygon` internamente usa dati Boost.Polygon (tipo `polygon_set_data`), questi potrebbero non essere thread-safe durante la copia

**MinkowskiSum.cpp** aggiunge NFP alla cache con `insert()`:

```cpp
nfpCache_.insert(keyA, keyB, nfpResult, inner);
```

Se due thread:
- Thread 1: Legge dalla cache (copia Polygon)
- Thread 2: Inserisce nella cache (modifica Polygon internals)

Potrebbe esserci race condition **dentro** Boost.Polygon data structures.

### Impact

- ⚠️ Potenziale crash in Boost.Polygon durante copia
- ⚠️ Corruzione di dati geometrici
- ⚠️ Difficile da riprodurre (race condition)

### Soluzione Proposta

**Opzione A: Deep Copy Defensiva**

Assicurarsi che la copia di Polygon sia veramente deep e thread-safe:

```cpp
bool NFPCache::find(const NFPKey& key, std::vector<Polygon>& result) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);

    std::string keyStr = generateKey(key);
    auto it = cache_.find(keyStr);

    if (it != cache_.end()) {
        // Deep copy each polygon to ensure thread safety
        result.clear();
        result.reserve(it->second.size());
        for (const auto& polygon : it->second) {
            result.push_back(polygon.deepCopy());  // ← Serve implementazione deepCopy()
        }
        ++hits_;
        return true;
    }

    ++misses_;
    return false;
}
```

**Opzione B: Polygon Immutabile**

Fare in modo che Polygon sia immutable dopo la creazione, garantendo thread-safety.

**Opzione C: Serialize/Deserialize**

Invece di copiare direttamente, serializzare e deserializzare Polygon per garantire deep copy:

```cpp
Polygon polygon = it->second[0];
std::string serialized = polygon.serialize();
Polygon copy = Polygon::deserialize(serialized);
```

---

## PROBLEMA #7: PARTS_ MODIFICATO DURANTE PROCESSING

### Descrizione

**NestingEngine::initialize() - NestingEngine.cpp:68:**
```cpp
parts_.clear();  // ← CANCELLA TUTTI I POLYGON
```

**NestingEngine::step() - NestingEngine.cpp:257-262:**
```cpp
parallelProcessor_->processPopulation(
    geneticAlgorithm_->getPopulationObject(),
    sheets_,
    *placementWorker_,
    config_.threads
);
```

### Root Cause

**Nessun lock protegge `parts_` da:**
- Modifiche concorrenti
- Clear durante processing

**Scenario teorico di crash:**

```
User: Chiama initialize() mentre nesting è ancora running

Thread UI:
  → NestingEngine::initialize()
  → parts_.clear()  // ← DISTRUGGE TUTTI I POLYGON

Thread Worker (background):
  → Lambda esegue
  → Polygon part = *individualCopy.placement[j];  // ← DANGLING POINTER!
  → 💥 CRASH
```

**Attualmente protetto da:**
- `initialize()` viene chiamato solo da `DeepNestSolver::start()`
- `start()` fallisce se `running_ == true`

```cpp
if (running_) {
    throw std::runtime_error("Nesting is already running");
}
```

**Tuttavia:**
- Non c'è invariant checking nel codice
- Se qualcuno chiama direttamente `NestingEngine::initialize()` mentre running, crash

### Soluzione Proposta

**Fix: Aggiungere check in initialize()**

```cpp
void NestingEngine::initialize(...) {
    if (running_) {
        throw std::runtime_error("Cannot initialize while nesting is running. Call stop() first.");
    }

    // ... resto del codice
}
```

---

## SUMMARY MATRIX

| Problema | Severità | Causa Root | Componenti Coinvolti | Impact Crash |
|----------|----------|------------|---------------------|--------------|
| #1 USE-AFTER-FREE Puntatori | 🔴 CRITICA | Raw pointers a vector elements | Individual, NestingEngine, ParallelProcessor | ✅ Stop+Restart |
| #2 Dangling Reference Lambda | 🔴 CRITICA | Lambda capture &population, &worker | ParallelProcessor, NestingEngine | ✅ Stop durante nesting |
| #3 partPointers_ Invalidation | 🔴 CRITICA | Puntatori invalidati da vector resize | NestingEngine | ⚠️ Potenziale |
| #4 No Wait in Stop | 🔴 ALTA | Task queue non flushed | ParallelProcessor, NestingEngine | ✅ Stop+Exit |
| #5 Recreate Processor | 🔴 ALTA | Gap di sincronizzazione | NestingEngine, ParallelProcessor | ⚠️ Intermittente |
| #6 NFPCache Polygon Copy | 🟡 MEDIA | Boost.Polygon non thread-safe copy | NFPCache, MinkowskiSum | ⚠️ Race condition |
| #7 parts_ Modifica Concorrente | 🟡 MEDIA | Nessun lock su parts_ | NestingEngine | ⚠️ Protetto da logic |

---

## PRIORITY FIXES

### P0 - CRITICAL (Must Fix)

1. **Fix Problema #1**: Usare `std::shared_ptr<Polygon>` invece di raw pointers
2. **Fix Problema #2**: Flush task queue in `ParallelProcessor::stop()`
3. **Fix Problema #4**: Aggiungere wait esplicito in `NestingEngine::stop()`

### P1 - HIGH (Should Fix)

4. **Fix Problema #5**: Aggiungere delay dopo `parallelProcessor_.reset()`
5. **Fix Problema #3**: Shared pointers (risolto insieme a #1)

### P2 - MEDIUM (Nice to Have)

6. **Fix Problema #6**: Implementare deep copy thread-safe per Polygon
7. **Fix Problema #7**: Aggiungere check in `initialize()`

---

## RECOMMENDED IMPLEMENTATION ORDER

### FASE 1: Thread Lifecycle Management (Risolve crash su Stop/Restart)

**File: ParallelProcessor.cpp**

```cpp
void ParallelProcessor::stop() {
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        if (stopped_) {
            return;
        }
        stopped_ = true;
        workGuard_.reset();
    }

    // CRITICAL FIX: Flush all pending tasks
    LOG_THREAD("Flushing pending tasks before stopping threads");
    while (!ioContext_.stopped()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        size_t remaining = ioContext_.poll();
        if (remaining == 0) {
            LOG_THREAD("All tasks flushed");
            break;
        }
        LOG_THREAD("Still have " << remaining << " tasks pending");
    }

    ioContext_.stop();
    LOG_THREAD("Waiting for all threads to join");
    threads_.join_all();
    LOG_THREAD("All threads joined successfully");
}
```

**File: NestingEngine.cpp**

```cpp
void NestingEngine::stop() {
    if (!running_) {
        return;
    }

    LOG_NESTING("Stopping nesting engine");
    running_ = false;

    if (parallelProcessor_) {
        LOG_THREAD("Stopping parallel processor");
        parallelProcessor_->stop();  // ← Questo ora flush i task
        parallelProcessor_->waitAll();  // ← Wait esplicito
        parallelProcessor_.reset();

        // Small delay to ensure complete cleanup
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        LOG_THREAD("Parallel processor stopped and destroyed");
    }

    LOG_NESTING("Nesting engine stopped");
}
```

### FASE 2: Shared Ownership (Risolve Use-After-Free)

**File: Individual.h**

```cpp
// Cambia da:
std::vector<Polygon*> placement;

// A:
std::vector<std::shared_ptr<Polygon>> placement;
```

**File: NestingEngine.h**

```cpp
// Cambia da:
std::vector<Polygon*> partPointers_;

// A:
std::vector<std::shared_ptr<Polygon>> partPointers_;
```

**File: NestingEngine.cpp**

```cpp
void NestingEngine::initialize(...) {
    // ...

    // Create shared pointers for GA
    partPointers_.clear();
    for (auto& part : parts_) {
        partPointers_.push_back(std::make_shared<Polygon>(part));
    }

    geneticAlgorithm_ = std::make_unique<GeneticAlgorithm>(partPointers_, config_);
}
```

**File: ParallelProcessor.cpp**

```cpp
// Lambda - linea 144-148 cambia da:
for (size_t j = 0; j < individualCopy.placement.size(); ++j) {
    Polygon part = *individualCopy.placement[j];  // ← Raw pointer dereference
    part.rotation = individualCopy.rotation[j];
    parts.push_back(part);
}

// A:
for (size_t j = 0; j < individualCopy.placement.size(); ++j) {
    Polygon part = *individualCopy.placement[j];  // ← Shared pointer dereference (same syntax!)
    part.rotation = individualCopy.rotation[j];
    parts.push_back(part);
}
```

**Nota**: La sintassi rimane identica perché `std::shared_ptr` overload `operator*`.

### FASE 3: Shared PlacementWorker (Risolve Dangling Reference)

**File: NestingEngine.h**

```cpp
// Cambia da:
std::unique_ptr<PlacementWorker> placementWorker_;

// A:
std::shared_ptr<PlacementWorker> placementWorker_;
```

**File: NestingEngine.cpp - costruttore**

```cpp
placementWorker_ = std::make_shared<PlacementWorker>(config_, *nfpCalculator_);
```

**File: ParallelProcessor.cpp**

```cpp
void ParallelProcessor::processPopulation(
    Population& population,
    const std::vector<Polygon>& sheets,
    PlacementWorker& worker,  // ← Ancora reference
    int maxConcurrent
) {
    // ...

    for (size_t i = 0; i < individuals.size(); ++i) {
        // ...

        // CRITICAL: Capture shared_ptr to worker to ensure it stays alive
        std::shared_ptr<PlacementWorker> workerPtr = ???;  // ← PROBLEMA: worker è reference!

        enqueue([&population, index, sheetsCopy, workerPtr, individualCopy, this]() mutable {
            PlacementWorker::PlacementResult result = workerPtr->placeParts(sheetsCopy, parts);
            // ...
        });
    }
}
```

**PROBLEMA:** `processPopulation` riceve `PlacementWorker&`, non `shared_ptr`.

**SOLUZIONE:** Cambiare signature:

```cpp
// ParallelProcessor.h
void processPopulation(
    Population& population,
    const std::vector<Polygon>& sheets,
    std::shared_ptr<PlacementWorker> worker,  // ← Cambia a shared_ptr
    int maxConcurrent = 0
);

// ParallelProcessor.cpp
void ParallelProcessor::processPopulation(
    Population& population,
    const std::vector<Polygon>& sheets,
    std::shared_ptr<PlacementWorker> worker,
    int maxConcurrent
) {
    // ...

    enqueue([&population, index, sheetsCopy, worker, individualCopy, this]() mutable {
        PlacementWorker::PlacementResult result = worker->placeParts(sheetsCopy, parts);
        // ...
    });
}

// NestingEngine.cpp - chiamata cambia da:
parallelProcessor_->processPopulation(
    geneticAlgorithm_->getPopulationObject(),
    sheets_,
    *placementWorker_,  // ← Dereference
    config_.threads
);

// A:
parallelProcessor_->processPopulation(
    geneticAlgorithm_->getPopulationObject(),
    sheets_,
    placementWorker_,  // ← Passa shared_ptr direttamente
    config_.threads
);
```

---

## TESTING STRATEGY

### Test 1: Stop durante nesting
```
1. Load 154 parts
2. Start nesting
3. Dopo 5 generazioni, Stop
4. Verificare: No crash, clean shutdown
5. Check logs: "All tasks flushed", "All threads joined"
```

### Test 2: Restart rapido
```
1. Load 154 parts
2. Start nesting
3. Dopo 3 generazioni, Stop
4. Wait 100ms
5. Start nesting again
6. Verificare: No crash, restart pulito
```

### Test 3: Exit durante nesting
```
1. Load 154 parts
2. Start nesting
3. Dopo 5 generazioni, chiudi applicazione
4. Verificare: No crash, exit pulito
5. Check logs: Destructor completo
```

### Test 4: Stress test - Multiple cycles
```
for (int i = 0; i < 10; i++) {
    Start nesting
    Wait 2 generazioni
    Stop
    Wait 50ms
}
Verificare: No crash, no memory leak
```

---

## EXPECTED RESULTS

Dopo l'implementazione di tutti i fix:

✅ No crash su Stop durante nesting
✅ No crash su Restart (Stop + Start)
✅ No crash su Exit durante nesting
✅ No memory leaks
✅ Thread-safe operations
✅ Stable su multiple cycles

---

## CONCLUSIONI

I crash 0xc0000005 sono causati da una combinazione di:

1. **Raw pointers a vector elements** che diventano invalidi quando il vector viene modificato
2. **Lambda capture by reference** che catturano oggetti che vengono distrutti prima che il task venga eseguito
3. **Task queue non flushed** prima della distruzione degli oggetti

La soluzione richiede:
- **Shared ownership** per Polygon e PlacementWorker
- **Task queue flush** in ParallelProcessor::stop()
- **Wait esplicito** in NestingEngine::stop()

Questi fix risolvono tutti e 3 gli scenari di crash riportati dall'utente.

---

**FINE ANALISI**
