# Piano di Implementazione DeepNest C++ Library

## Analisi Preliminare e Struttura del Progetto

### 1. Analisi Iniziale e Organizzazione del Progetto

**File JavaScript di riferimento**: Tutti i file nel repository
**File C++ da creare**: CMakeLists.txt, deepnest.pro

Si esegue un'analisi completa di tutti i file JavaScript e C++ presenti nel repository DeepNest. Si identificano le seguenti componenti principali:

- **Algoritmi geometrici di base** (geometryutil.js)
- **Sistema di configurazione** (config objects in deepnest.js/svgnest.js)
- **Algoritmo genetico** (GeneticAlgorithm in deepnest.js)
- **Calcolo NFP** (minkowski.cc esistente + logica JS)
- **Placement/Nesting** (placementworker.js, background.js)
- **Gestione cache NFP** (db object in background.js)


```
deepnest-cpp/
├── include/deepnest/
│   ├── core/
│   │   ├── Types.h
│   │   ├── Point.h
│   │   ├── Polygon.h
│   │   └── BoundingBox.h
│   ├── geometry/
│   │   ├── GeometryUtil.h
│   │   ├── PolygonOperations.h
│   │   ├── ConvexHull.h
│   │   ├── Transformation.h
│   │   └── MergeDetection.h
│   ├── nfp/
│   │   ├── NFPCalculator.h
│   │   ├── NFPCache.h
│   │   └── MinkowskiSum.h
│   ├── algorithm/
│   │   ├── GeneticAlgorithm.h
│   │   ├── Individual.h
│   │   └── Population.h
│   ├── placement/
│   │   ├── PlacementStrategy.h
│   │   ├── PlacementWorker.h
│   │   ├── NestingEngine.h
│   │   └── ParallelProcessor.h
│   ├── config/
│   │   └── DeepNestConfig.h
│   ├── converters/
│   │   └── QtBoostConverter.h
│   └── DeepNestSolver.h
├── src/
│   ├── core/
│   │   ├── Types.cpp
│   │   ├── Point.cpp
│   │   ├── Polygon.cpp
│   │   └── BoundingBox.cpp
│   ├── geometry/
│   │   ├── GeometryUtil.cpp
│   │   ├── PolygonOperations.cpp
│   │   ├── ConvexHull.cpp
│   │   ├── Transformation.cpp
│   │   └── MergeDetection.cpp
│   ├── nfp/
│   │   ├── NFPCalculator.cpp
│   │   ├── NFPCache.cpp
│   │   └── MinkowskiSum.cpp
│   ├── algorithm/
│   │   ├── GeneticAlgorithm.cpp
│   │   ├── Individual.cpp
│   │   └── Population.cpp
│   ├── placement/
│   │   ├── PlacementStrategy.cpp
│   │   ├── PlacementWorker.cpp
│   │   ├── NestingEngine.cpp
│   │   └── ParallelProcessor.cpp
│   ├── config/
│   │   └── DeepNestConfig.cpp
│   ├── converters/
│   │   └── QtBoostConverter.cpp
│   └── DeepNestSolver.cpp
├── tests/
│   ├── TestApplication.h
│   ├── TestApplication.cpp
│   ├── SVGLoader.h
│   ├── SVGLoader.cpp
│   ├── RandomShapeGenerator.h
│   ├── RandomShapeGenerator.cpp
│   └── main.cpp
├── third_party/
│   └── clipper2/
├── deepnest.pro
└── CMakeLists.txt
```

Questo piano fornisce una roadmap completa e dettagliata per la conversione di DeepNest da JavaScript a C++, mantenendo la fedeltà agli algoritmi originali e integrando le ottimizzazioni esistenti.
---

### 2. Tipi di Base e Strutture Dati Fondamentali

**File JavaScript di riferimento**: geometryutil.js (linee 1-50)
**File C++ da creare**: Types.h, Point.h

Si definiscono i tipi di base utilizzando Boost.Polygon per la rappresentazione interna. Si crea un sistema di conversione da/verso QPainterPath per l'interfaccia esterna.

```cpp
// Types.h
namespace deepnest {
    using BoostPoint = boost::polygon::point_data<double>;
    using BoostPolygon = boost::polygon::polygon_with_holes_data<double>;
    using BoostPolygonSet = boost::polygon::polygon_set_data<double>;
    
    struct Point {
        double x, y;
        bool exact = false; // per merge detection
        BoostPoint toBoost() const;
        static Point fromQt(const QPointF& p);
    };
}
```

Si implementano le conversioni necessarie e gli operatori di base per i punti.

---

### 3. Classe di Configurazione

**File JavaScript di riferimento**: deepnest.js (linee 24-42), svgnest.js (linee 24-34)
**File C++ da creare**: DeepNestConfig.h, DeepNestConfig.cpp

Si crea una classe singleton per gestire tutti i parametri di configurazione estratti dal codice JavaScript.

```cpp
class DeepNestConfig {
private:
    static DeepNestConfig* instance;
    
public:
    double clipperScale = 10000000;
    double curveTolerance = 0.3;
    double spacing = 0;
    int rotations = 4;
    int populationSize = 10;
    int mutationRate = 10;
    int threads = 4;
    std::string placementType = "gravity";
    bool mergeLines = true;
    double timeRatio = 0.5;
    double scale = 72;
    bool simplify = false;
    
    static DeepNestConfig& getInstance();
    void loadFromJson(const QString& path);
};
```

---

### 4. Utility Geometriche di Base

**File JavaScript di riferimento**: geometryutil.js (completo)
**File C++ da creare**: GeometryUtil.h, GeometryUtil.cpp

Si convertono tutte le funzioni geometriche da JavaScript. Si implementano:
- `almostEqual()`, `withinDistance()`
- `lineIntersect()`, `onSegment()`
- `polygonArea()`, `getPolygonBounds()`
- `pointInPolygon()`
- Algoritmi per Bezier curves (Quadratic, Cubic)
- Algoritmi per Arc

```cpp
namespace GeometryUtil {
    bool almostEqual(double a, double b, double tolerance = 1e-9);
    bool pointInPolygon(const Point& point, const Polygon& polygon);
    double polygonArea(const Polygon& polygon);
    BoundingBox getPolygonBounds(const Polygon& polygon);
    // ... altre funzioni
}
```

---

### 5. Operazioni sui Poligoni con Clipper2

**File JavaScript di riferimento**: deepnest.js (polygonOffset, cleanPolygon)
**File C++ da creare**: PolygonOperations.h, PolygonOperations.cpp

Si implementano le operazioni sui poligoni utilizzando Clipper2:
- Offset dei poligoni
- Pulizia e semplificazione
- Operazioni booleane (unione, differenza, intersezione)

```cpp
class PolygonOperations {
public:
    static std::vector<Polygon> offset(const Polygon& poly, double delta);
    static Polygon cleanPolygon(const Polygon& poly);
    static Polygon simplifyPolygon(const Polygon& poly, bool inside);
    static std::vector<Polygon> mergePolygons(const std::vector<Polygon>& polys);
};
```

---

### 6. Convex Hull

**File JavaScript di riferimento**: hull.js, deepnest.js (getHull)
**File C++ da creare**: ConvexHull.h, ConvexHull.cpp

Si implementa l'algoritmo Graham Scan per il calcolo del convex hull, utilizzando Boost.Polygon quando possibile.

```cpp
class ConvexHull {
public:
    static Polygon computeHull(const std::vector<Point>& points);
    static Polygon computeHull(const Polygon& polygon);
};
```

---

### 7. Trasformazioni Geometriche

**File JavaScript di riferimento**: matrix.js, svgparser.js (transformParse)
**File C++ da creare**: Transformation.h, Transformation.cpp

Si implementa una classe per gestire le trasformazioni 2D (rotazione, traslazione, scala).

```cpp
class Transformation {
private:
    double matrix[6]; // matrice affine 2x3
    
public:
    void rotate(double angle, double cx = 0, double cy = 0);
    void translate(double tx, double ty);
    void scale(double sx, double sy);
    Point apply(const Point& p) const;
    Polygon apply(const Polygon& p) const;
};
```

---

### 8. Struttura Polygon con Supporto per Buchi

**File JavaScript di riferimento**: deepnest.js (polygontree structure)
**File C++ da creare**: Polygon.h, Polygon.cpp

Si crea una struttura poligono che supporta buchi (children) e metadati.

```cpp
class Polygon {
public:
    std::vector<Point> points;
    std::vector<Polygon> children; // holes
    int id = -1;
    int source = -1;
    double rotation = 0;
    
    BoostPolygon toBoostPolygon() const;
    static Polygon fromQPainterPath(const QPainterPath& path);
    QPainterPath toQPainterPath() const;
    
    Polygon rotate(double angle) const;
    Polygon translate(double x, double y) const;
};
```

---

### 9. Cache NFP

**File JavaScript di riferimento**: background.js (window.db object)
**File C++ da creare**: NFPCache.h, NFPCache.cpp

Si implementa un sistema di cache thread-safe per i calcoli NFP utilizzando Boost.Thread.

```cpp
class NFPCache {
private:
    mutable boost::shared_mutex mutex;
    std::unordered_map<std::string, Polygon> cache;
    
public:
    bool has(int idA, int idB, double rotA, double rotB) const;
    Polygon find(int idA, int idB, double rotA, double rotB) const;
    void insert(int idA, int idB, double rotA, double rotB, const Polygon& nfp);
    std::string generateKey(int idA, int idB, double rotA, double rotB) const;
};
```

---

### 10. Integrazione Minkowski C++ Esistente

**File JavaScript di riferimento**: background.js (getOuterNfp)
**File C++ esistenti**: minkowski.cc, minkowski.h
**File C++ da creare**: MinkowskiSum.h, MinkowskiSum.cpp

Si adatta e si integra il codice C++ esistente per il calcolo Minkowski, rimuovendo le dipendenze da V8/Node.js.

```cpp
class MinkowskiSum {
private:
    static double calculateScale(const Polygon& A, const Polygon& B);
    
public:
    static Polygon calculateNFP(const Polygon& A, const Polygon& B);
    static std::vector<Polygon> calculateNFPBatch(
        const std::vector<Polygon>& Alist, 
        const Polygon& B
    );
};
```

---

### 11. Calcolo NFP Completo

**File JavaScript di riferimento**: background.js (getOuterNfp, getInnerNfp), geometryutil.js (noFitPolygon)
**File C++ da creare**: NFPCalculator.h, NFPCalculator.cpp

Si implementa il calcolo completo NFP integrando Minkowski e Clipper2.

```cpp
class NFPCalculator {
private:
    NFPCache& cache;
    
public:
    Polygon getOuterNFP(const Polygon& A, const Polygon& B, bool inside = false);
    std::vector<Polygon> getInnerNFP(const Polygon& A, const Polygon& B);
    Polygon getFrame(const Polygon& A);
};
```

---

### 12. Individuo per Algoritmo Genetico

**File JavaScript di riferimento**: deepnest.js (GeneticAlgorithm population structure)
**File C++ da creare**: Individual.h, Individual.cpp

Si crea la struttura per rappresentare un individuo nell'algoritmo genetico.

```cpp
class Individual {
public:
    std::vector<Polygon*> placement; // ordine di inserimento
    std::vector<double> rotation;    // rotazioni per ogni parte
    double fitness = std::numeric_limits<double>::max();
    bool processing = false;
    
    Individual clone() const;
    void mutate(double mutationRate, int rotations);
};
```

---

### 13. Popolazione e Operazioni Genetiche

**File JavaScript di riferimento**: deepnest.js (GeneticAlgorithm methods)
**File C++ da creare**: Population.h, Population.cpp

Si implementano le operazioni genetiche sulla popolazione.

```cpp
class Population {
private:
    std::vector<Individual> individuals;
    
public:
    void initialize(const std::vector<Polygon>& parts, const DeepNestConfig& config);
    std::pair<Individual, Individual> crossover(
        const Individual& parent1, 
        const Individual& parent2
    );
    Individual selectWeightedRandom(const Individual* exclude = nullptr);
    void nextGeneration();
};
```

---

### 14. Algoritmo Genetico Principale

**File JavaScript di riferimento**: deepnest.js (GeneticAlgorithm class)
**File C++ da creare**: GeneticAlgorithm.h, GeneticAlgorithm.cpp

Si implementa la classe principale dell'algoritmo genetico.

```cpp
class GeneticAlgorithm {
private:
    Population population;
    DeepNestConfig config;
    std::vector<Polygon> parts;
    
public:
    GeneticAlgorithm(const std::vector<Polygon>& parts, const DeepNestConfig& config);
    void generation();
    Individual getBestIndividual() const;
    bool isGenerationComplete() const;
};
```

---

### 15. Strategia di Posizionamento

**File JavaScript di riferimento**: background.js (placeParts logic)
**File C++ da creare**: PlacementStrategy.h, PlacementStrategy.cpp

Si implementano le strategie di posizionamento (gravity, bounding box, convex hull).

```cpp
class PlacementStrategy {
public:
    enum Type { GRAVITY, BOUNDING_BOX, CONVEX_HULL };
    
    virtual Point findBestPosition(
        const Polygon& part,
        const std::vector<Polygon>& placed,
        const std::vector<Point>& positions,
        const std::vector<Polygon>& nfps
    ) = 0;
};

class GravityPlacement : public PlacementStrategy {
    // implementazione specifica
};
```

---

### 16. Merge Lines Detection

**File JavaScript di riferimento**: background.js (mergedLength function)
**File C++ da creare**: MergeDetection.h, MergeDetection.cpp

Si implementa il sistema per rilevare e ottimizzare le linee che possono essere unite.

```cpp
class MergeDetection {
public:
    struct MergeResult {
        double totalLength;
        std::vector<std::pair<Point, Point>> segments;
    };
    
    static MergeResult calculateMergedLength(
        const std::vector<Polygon>& placed,
        const Polygon& newPart,
        double minLength,
        double tolerance
    );
};
```

---

### 17. Worker di Posizionamento

**File JavaScript di riferimento**: placementworker.js, background.js (placeParts)
**File C++ da creare**: PlacementWorker.h, PlacementWorker.cpp

Si implementa il worker che esegue il posizionamento delle parti.

```cpp
class PlacementWorker {
private:
    NFPCalculator nfpCalculator;
    PlacementStrategy* strategy;
    
public:
    struct PlacementResult {
        std::vector<std::vector<Placement>> placements; // per sheet
        double fitness;
        double area;
        double mergedLength;
    };
    
    PlacementResult placeParts(
        const std::vector<Polygon>& sheets,
        std::vector<Polygon> parts,
        const DeepNestConfig& config
    );
};
```

---

### 18. Parallelizzazione con Boost.Thread

**File JavaScript di riferimento**: background.js (parallel processing logic)
**File C++ da creare**: ParallelProcessor.h, ParallelProcessor.cpp

Si implementa il sistema di parallelizzazione utilizzando Boost.Thread con un thread pool.

```cpp
class ParallelProcessor {
private:
    boost::asio::io_service ioService;
    boost::thread_group threads;
    std::unique_ptr<boost::asio::io_service::work> work;
    
public:
    ParallelProcessor(int numThreads);
    
    template<typename Func>
    std::future<typename std::result_of<Func()>::type> 
    enqueue(Func&& f);
    
    void processPopulation(
        Population& pop,
        const std::vector<Polygon>& sheets,
        PlacementWorker& worker
    );
};
```

---

### 19. Engine di Nesting Principale

**File JavaScript di riferimento**: deepnest.js (main nesting logic)
**File C++ da creare**: NestingEngine.h, NestingEngine.cpp

Si implementa il motore principale che orchestra tutto il processo di nesting.

```cpp
class NestingEngine {
private:
    GeneticAlgorithm ga;
    ParallelProcessor processor;
    NFPCache nfpCache;
    std::vector<PlacementResult> bestResults;
    
    std::function<void(double)> progressCallback;
    std::function<void(const PlacementResult&)> displayCallback;
    
public:
    void start(
        const std::vector<Polygon>& sheets,
        const std::vector<Polygon>& parts,
        const DeepNestConfig& config
    );
    
    void stop();
    std::vector<PlacementResult> getBestResults() const;
};
```

---

### 20. Interfaccia Principale DeepNestSolver

**File JavaScript di riferimento**: deepnest.js (public interface)
**File C++ da creare**: DeepNestSolver.h, DeepNestSolver.cpp

Si implementa l'interfaccia pubblica principale della libreria.

```cpp
class DeepNestSolver {
private:
    class Impl; // PIMPL idiom
    std::unique_ptr<Impl> pImpl;
    
public:
    DeepNestSolver();
    ~DeepNestSolver();
    
    // Configurazione
    void setConfig(const DeepNestConfig& config);
    
    // Interfaccia principale
    QList<QHash<QString, QPair<QPointF, double>>> solve(
        const QPainterPath& container,
        const QHash<QString, QPainterPath>& shapes
    );
    
    // Callbacks
    void setProgressCallback(std::function<void(double)> callback);
    void setDisplayCallback(std::function<void(const PlacementResult&)> callback);
    
    // Controllo
    void stop();
    void reset();
};
```

---

### 21. Conversioni Qt-Boost

**File JavaScript di riferimento**: N/A (nuovo per C++)
**File C++ da creare**: QtBoostConverter.h, QtBoostConverter.cpp

Si implementano le conversioni tra tipi Qt e Boost.Polygon.

```cpp
namespace QtBoostConverter {
    Polygon fromQPainterPath(const QPainterPath& path);
    QPainterPath toQPainterPath(const Polygon& polygon);
    
    BoostPolygon toBoostPolygon(const QPainterPath& path);
    QPainterPath fromBoostPolygon(const BoostPolygon& poly);
    
    Point fromQPointF(const QPointF& point);
    QPointF toQPointF(const Point& point);
}
```

---

### 22. Sistema di Test con Qt

**File JavaScript di riferimento**: N/A
**File C++ da creare**: TestApplication.h, TestApplication.cpp, main.cpp

Si crea un'applicazione di test Qt.

```cpp
class TestApplication : public QMainWindow {
    Q_OBJECT
    
private:
    DeepNestSolver solver;
    QGraphicsScene* scene;
    
public:
    TestApplication(QWidget* parent = nullptr);
    
private slots:
    void loadSVG();
    void startNesting();
    void testRandomRectangles();
    void onProgress(double progress);
    void onPlacementReady(const PlacementResult& result);
};
```

---

### 23. SVG Loader per Test

**File JavaScript di riferimento**: svgparser.js (solo per riferimento)
**File C++ da creare**: SVGLoader.h, SVGLoader.cpp

Si implementa un loader SVG minimale per i test.

```cpp
class SVGLoader {
public:
    struct Shape {
        QString id;
        QPainterPath path;
        QTransform transform;
    };
    
    static std::vector<Shape> loadShapes(const QString& svgPath);
    static QPainterPath loadContainer(const QString& svgPath);
};
```

---

### 24. Generatore Random per Test

**File JavaScript di riferimento**: N/A
**File C++ da creare**: RandomShapeGenerator.h, RandomShapeGenerator.cpp

Si crea un generatore di forme casuali per test.

```cpp
class RandomShapeGenerator {
public:
    static QPainterPath generateRandomRectangle(
        double minWidth, double maxWidth,
        double minHeight, double maxHeight
    );
    
    static std::vector<QPainterPath> generateTestSet(
        int numShapes,
        double containerWidth,
        double containerHeight
    );
};
```

---

### 25. File di Progetto qmake

**File da creare**: deepnest.pro

Si crea il file di progetto qmake.

```makefile
TEMPLATE = app
TARGET = DeepNestTest
QT += core gui widgets

CONFIG += c++17
CONFIG += thread

# Boost
INCLUDEPATH += /usr/local/include
LIBS += -L/usr/local/lib
LIBS += -lboost_polygon
LIBS += -lboost_thread
LIBS += -lboost_system

# Clipper2
INCLUDEPATH += $$PWD/third_party/clipper2/include
SOURCES += $$PWD/third_party/clipper2/src/*.cpp

# Headers
HEADERS += \
    include/deepnest/*.h \
    include/deepnest/core/*.h \
    include/deepnest/geometry/*.h \
    include/deepnest/nfp/*.h \
    include/deepnest/algorithm/*.h \
    include/deepnest/placement/*.h \
    include/deepnest/config/*.h

# Sources
SOURCES += \
    src/*.cpp \
    src/core/*.cpp \
    src/geometry/*.cpp \
    src/nfp/*.cpp \
    src/algorithm/*.cpp \
    src/placement/*.cpp \
    src/config/*.cpp \
    tests/*.cpp
```

---


