# DeepNest C++ Library

A C++ implementation of the DeepNest nesting algorithm, converted from the original JavaScript version.

## Project Status

### Completed Steps (1-10, 25)

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
✅ **Step 25**: Build system (qmake and CMake)

### Remaining Steps (11-24)

⏳ Step 11: Complete NFP calculator
⏳ Step 12: Individual class for genetic algorithm
⏳ Step 13: Population and genetic operations
⏳ Step 14: Main genetic algorithm
⏳ Step 15: Placement strategies
⏳ Step 16: Merge lines detection
⏳ Step 17: Placement worker
⏳ Step 18: Parallelization with Boost.Thread
⏳ Step 19: Main nesting engine
⏳ Step 20: DeepNestSolver interface
⏳ Step 21: Qt-Boost converters
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
│   ├── algorithm/     # Genetic algorithm (TODO)
│   ├── placement/     # Placement strategies (TODO)
│   └── converters/    # Type converters (TODO)
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

## Usage Example

```cpp
#include <deepnest/core/Polygon.h>
#include <deepnest/nfp/MinkowskiSum.h>
#include <deepnest/nfp/NFPCache.h>

using namespace deepnest;

// Create polygons
Polygon partA({{0, 0}, {100, 0}, {100, 100}, {0, 100}});
Polygon partB({{0, 0}, {50, 0}, {50, 50}, {0, 50}});

// Calculate NFP
NFPCache cache;
auto nfps = MinkowskiSum::calculateNFP(partA, partB);

// Cache the result
cache.insert(partA.id, partB.id, 0.0, 0.0, nfps);
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
