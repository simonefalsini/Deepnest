# DeepNest C++ Library

A C++ implementation of the DeepNest nesting algorithm, converted from the original JavaScript version.

## Project Status

### Completed Steps (1-20, 25)

✅ **Step 1**: Project structure and directory organization
✅ **Step 2**: Base types (Types.h, Point.h, BoundingBox.h)
✅ **Step 3**: Configuration system (DeepNestConfig)
✅ **Step 4**: Geometry utilities (GeometryUtil)
✅ **Step 5**: Polygon operations with Clipper2
✅ **Step 6**: Convex Hull (Graham Scan algorithm)
✅ **Step 7**: 2D affine transformations
✅ **Step 8**: Polygon class with hole support
✅ **Step 9**: Thread-safe NFP cache
✅ **Step 10**: Minkowski sum integration
✅ **Step 11**: Complete NFP calculator
✅ **Step 12**: Individual class for genetic algorithm
✅ **Step 13**: Population management and genetic operations
✅ **Step 14**: Main genetic algorithm orchestration
✅ **Step 15**: Placement strategies (gravity, bounding box, convex hull)
✅ **Step 16**: Merge lines detection for cut optimization
✅ **Step 17**: Placement worker for part positioning
✅ **Step 18**: Parallel processing with Boost.Thread thread pool
✅ **Step 19**: Main nesting engine coordination
✅ **Step 20**: DeepNestSolver user-facing interface
✅ **Step 21**: Qt-Boost converters (QtBoostConverter namespace)
✅ **Step 25**: Build system (qmake and CMake)

### Remaining Steps (22-24)

⏳ Step 22: Test application
⏳ Step 23: SVG loader
⏳ Step 24: Random shape generator

## Features

- **Boost.Polygon Integration**: High-performance geometric operations
- **Clipper2 Support**: Advanced polygon offsetting and boolean operations
- **Qt Compatibility**: Seamless integration with Qt applications (QPainterPath)
- **Thread-Safe Caching**: Boost.Thread shared_mutex for NFP cache
- **Minkowski Sum**: Efficient NFP calculation using convolution
- **Geometric Transformations**: Complete 2D affine transformation support

## Dependencies

- **Qt 5+**: Core, Gui, Widgets modules
- **Boost**: Polygon, Thread, System libraries
- **Clipper2**: Included in parent directory
- **C++17 compiler**: GCC 7+, Clang 5+, MSVC 2017+

## Building

### Using qmake

```bash
cd deepnest-cpp
qmake
make
sudo make install
```

### Using CMake

```bash
cd deepnest-cpp
mkdir build && cd build
cmake ..
make
sudo make install
```

## Project Structure

```
deepnest-cpp/
├── include/deepnest/
│   ├── core/           # Base types and polygon
│   ├── geometry/       # Geometric operations
│   ├── nfp/           # NFP calculation
│   ├── config/        # Configuration
│   ├── algorithm/     # Genetic algorithm
│   ├── placement/     # Placement strategies
│   └── converters/    # Qt-Boost type converters
├── src/               # Implementation files
├── tests/             # Test applications (TODO)
├── deepnest.pro       # qmake project file
└── CMakeLists.txt     # CMake configuration
```

## Core Components

### Types System
- `Point`: 2D point with transformation support
- `Polygon`: Polygon with holes and metadata
- `BoundingBox`: Axis-aligned bounding box
- Boost.Polygon integration for high-performance operations

### Geometry Utilities
- Line intersection and segment operations
- Point-in-polygon tests
- Polygon area and bounds calculation
- Bezier and arc linearization
- Convex hull computation

### Polygon Operations (Clipper2)
- Offset (expand/contract)
- Union, intersection, difference
- Simplification and cleaning
- Polygon boolean operations

### NFP Calculation
- Minkowski sum using Boost.Polygon convolution
- Thread-safe caching system
- Support for holes and complex polygons
- Batch processing capability

### Configuration
- Singleton configuration manager
- JSON import/export
- Runtime parameter validation

### Genetic Algorithm
- Individual representation with placement sequence and rotations
- Mutation operations for genetic optimization
- Fitness-based comparison
- Population management with elitism
- Single-point crossover for breeding
- Weighted random selection (fitness-based)
- Automatic generation evolution
- High-level GeneticAlgorithm orchestrator
- Generation tracking and statistics

### Placement Strategies
- Gravity placement: Minimize width*2 + height (compress downward)
- Bounding box placement: Minimize width * height
- Convex hull placement: Minimize convex hull area
- Pluggable strategy pattern for custom objectives
- Automatic best position selection from NFP candidates

### Nesting Engine
- Main coordination layer for entire nesting process
- Integrates GA, parallel processor, placement worker, NFP calculator
- Automatic part sorting by area (largest first - "adam")
- Spacing offset application to parts and sheets
- Progress tracking with callback support
- Result tracking (keeps top N best nests)
- Generation-based optimization loop
- Asynchronous processing with configurable max generations

### DeepNestSolver Interface
- High-level user-facing API for nesting
- Simple configuration methods (spacing, rotations, population size, etc.)
- Easy part and sheet management with quantities
- Start/stop/step control for nesting process
- Progress and result callbacks for UI integration
- Synchronous and asynchronous execution modes
- Complete example code in header documentation

### Qt-Boost Converters
- Centralized conversion namespace (QtBoostConverter)
- Point conversions: Point ↔ QPointF ↔ BoostPoint
- Polygon conversions: Polygon ↔ QPainterPath ↔ BoostPolygon
- Direct conversions for performance-critical operations
- Batch conversion utilities for lists/vectors
- Support for polygons with holes (BoostPolygonWithHoles)
- BoostPolygonSet to QPainterPath conversion

## Usage Example

```cpp
#include <deepnest/DeepNestSolver.h>
#include <iostream>

using namespace deepnest;

int main() {
    // Create solver
    DeepNestSolver solver;

    // Configure nesting parameters
    solver.setSpacing(5.0);           // 5 units spacing between parts
    solver.setRotations(4);           // Try 4 rotations (90° increments)
    solver.setPopulationSize(10);     // GA population size
    solver.setPlacementType("gravity"); // Use gravity placement strategy

    // Create parts (rectangles for this example)
    Polygon part1({{0, 0}, {100, 0}, {100, 50}, {0, 50}});
    Polygon part2({{0, 0}, {80, 0}, {80, 60}, {0, 60}});

    // Add parts with quantities
    solver.addPart(part1, 5, "Rectangle 100x50");
    solver.addPart(part2, 3, "Rectangle 80x60");

    // Create sheet
    Polygon sheet({{0, 0}, {500, 0}, {500, 300}, {0, 300}});
    solver.addSheet(sheet, 1, "Sheet 500x300");

    // Set callbacks
    solver.setProgressCallback([](const NestProgress& progress) {
        std::cout << "Generation " << progress.generation
                  << ", Best fitness: " << progress.bestFitness
                  << ", Progress: " << progress.percentComplete << "%"
                  << std::endl;
    });

    solver.setResultCallback([](const NestResult& result) {
        std::cout << "New best result! Fitness: " << result.fitness
                  << " at generation " << result.generation << std::endl;
    });

    // Start nesting (100 generations max)
    solver.start(100);

    // Run nesting loop (step-based, can be called from timer/main loop)
    while (solver.isRunning()) {
        solver.step();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Or use synchronous mode (blocks until complete)
    // solver.runUntilComplete(100);

    // Get best result
    const NestResult* best = solver.getBestResult();
    if (best) {
        std::cout << "Final best fitness: " << best->fitness << std::endl;
        std::cout << "Number of placements: " << best->placements.size() << std::endl;
    }

    return 0;
}
```

## Next Steps

The remaining implementation includes:

1. **NFP Calculator**: High-level NFP calculation with caching
2. **Genetic Algorithm**: Population-based optimization
3. **Placement Engine**: Part placement strategies
4. **Nesting Engine**: Main orchestration logic
5. **Test Applications**: Qt-based test and visualization tools

## Contributing

This is a conversion project following the detailed plan in `conversion_step.md`.
Each step preserves the algorithms from the original JavaScript implementation.

## License

Same as original DeepNest project (GPLv3)

## References

- Original DeepNest: [GitHub](https://github.com/Jack000/Deepnest)
- Conversion Plan: See `../conversion_step.md`
