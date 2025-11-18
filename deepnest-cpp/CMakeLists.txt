cmake_minimum_required(VERSION 3.10)
project(DeepNest VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_TESTS "Build test applications" ON)

# Find required packages
find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)
find_package(Boost REQUIRED COMPONENTS thread system)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/deepnest
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../Clipper2Lib/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Clipper2Lib/include/clipper2
)

# Source files
set(DEEPNEST_SOURCES
    # Core
    src/core/Types.cpp
    src/core/Point.cpp
    src/core/Polygon.cpp

    # Geometry
    src/geometry/GeometryUtil.cpp
    src/geometry/PolygonOperations.cpp
    src/geometry/ConvexHull.cpp
    src/geometry/Transformation.cpp

    # NFP
    src/nfp/NFPCache.cpp
    src/nfp/MinkowskiSum.cpp
    src/nfp/NFPCalculator.cpp

    # Config
    src/config/DeepNestConfig.cpp

    # Algorithm
    src/algorithm/Individual.cpp
    src/algorithm/Population.cpp
    src/algorithm/GeneticAlgorithm.cpp

    # Placement
    src/placement/PlacementStrategy.cpp
    src/placement/MergeDetection.cpp
    src/placement/PlacementWorker.cpp

    # Parallel
    src/parallel/ParallelProcessor.cpp

    # Engine
    src/engine/NestingEngine.cpp

    # Clipper2
    ../Clipper2Lib/src/clipper.engine.cpp
    ../Clipper2Lib/src/clipper.offset.cpp
    ../Clipper2Lib/src/clipper.rectclip.cpp
)

# Header files
set(DEEPNEST_HEADERS
    # Core
    include/deepnest/core/Types.h
    include/deepnest/core/Point.h
    include/deepnest/core/BoundingBox.h
    include/deepnest/core/Polygon.h

    # Geometry
    include/deepnest/geometry/GeometryUtil.h
    include/deepnest/geometry/PolygonOperations.h
    include/deepnest/geometry/ConvexHull.h
    include/deepnest/geometry/Transformation.h

    # NFP
    include/deepnest/nfp/NFPCache.h
    include/deepnest/nfp/MinkowskiSum.h
    include/deepnest/nfp/NFPCalculator.h

    # Config
    include/deepnest/config/DeepNestConfig.h

    # Algorithm
    include/deepnest/algorithm/Individual.h
    include/deepnest/algorithm/Population.h
    include/deepnest/algorithm/GeneticAlgorithm.h

    # Placement
    include/deepnest/placement/PlacementStrategy.h
    include/deepnest/placement/MergeDetection.h
    include/deepnest/placement/PlacementWorker.h

    # Parallel
    include/deepnest/parallel/ParallelProcessor.h

    # Engine
    include/deepnest/engine/NestingEngine.h
)

# Create library
if(BUILD_SHARED_LIBS)
    add_library(deepnest SHARED ${DEEPNEST_SOURCES} ${DEEPNEST_HEADERS})
else()
    add_library(deepnest STATIC ${DEEPNEST_SOURCES} ${DEEPNEST_HEADERS})
endif()

# Link libraries
target_link_libraries(deepnest
    PUBLIC
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        ${Boost_LIBRARIES}
        pthread
)

# Set target properties
set_target_properties(deepnest PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${DEEPNEST_HEADERS}"
)

# Compiler options
target_compile_options(deepnest PRIVATE
    -Wall
    -Wextra
    -pedantic
)

# Install rules
install(TARGETS deepnest
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/deepnest
)

# Install headers with directory structure
install(DIRECTORY include/deepnest
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Export cmake config
install(EXPORT DeepNestTargets
    FILE DeepNestTargets.cmake
    NAMESPACE DeepNest::
    DESTINATION lib/cmake/DeepNest
)

# Create README for the library
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/README.md
"# DeepNest C++ Library

A C++ implementation of the DeepNest nesting algorithm.

## Building

### With CMake:
```bash
mkdir build
cd build
cmake ..
make
sudo make install
```

### With qmake:
```bash
qmake
make
sudo make install
```

## Usage

Link against the library:
```cmake
find_package(DeepNest REQUIRED)
target_link_libraries(your_app DeepNest::deepnest)
```

## Features

- Polygon operations with Clipper2
- No-Fit Polygon (NFP) calculation using Minkowski sum
- Genetic algorithm for optimization
- Thread-safe NFP caching
- Qt integration for UI applications
")

message(STATUS "DeepNest C++ Library configuration complete")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Qt5 version: ${Qt5_VERSION}")
message(STATUS "  Boost version: ${Boost_VERSION}")
