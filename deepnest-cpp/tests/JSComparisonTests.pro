TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

TARGET = JSComparisonTests

# Include paths
INCLUDEPATH += ..

# Source files
SOURCES += JSComparisonTests.cpp \
    ../src/core/Point.cpp \
    ../src/core/BoundingBox.cpp \
    ../src/core/Polygon.cpp \
    ../src/config/DeepNestConfig.cpp \
    ../src/geometry/GeometryUtil.cpp \
    ../src/geometry/PolygonOperations.cpp \
    ../src/geometry/ConvexHull.cpp \
    ../src/geometry/Transformation.cpp \
    ../src/geometry/MergeDetection.cpp \
    ../src/nfp/NFPCache.cpp \
    ../src/nfp/NFPCalculator.cpp \
    ../src/nfp/MinkowskiSum.cpp \
    ../src/placement/PlacementStrategy.cpp \
    ../src/placement/PlacementWorker.cpp \
    ../src/algorithm/Individual.cpp \
    ../src/algorithm/Population.cpp \
    ../src/algorithm/GeneticAlgorithm.cpp

# Clipper2 library
LIBS += -L../lib/Clipper2/CPP/Clipper2Lib -lClipper2

# Boost libraries
LIBS += -lboost_system -lboost_thread -lpthread

# Enable optimizations
QMAKE_CXXFLAGS += -O2
