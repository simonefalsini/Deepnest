TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
QT += core

TARGET = JSComparisonTests

# Include paths
INCLUDEPATH += ..
INCLUDEPATH += ../../Clipper2Lib/include
INCLUDEPATH += ../../Clipper2Lib/include/clipper2

# Source files
SOURCES += JSComparisonTests.cpp \
    ../src/core/Point.cpp \
    ../src/core/Polygon.cpp \
    ../src/core/Types.cpp \
    ../src/config/DeepNestConfig.cpp \
    ../src/geometry/GeometryUtil.cpp \
    ../src/geometry/PolygonOperations.cpp \
    ../src/geometry/ConvexHull.cpp \
    ../src/geometry/Transformation.cpp \
    ../src/nfp/NFPCache.cpp \
    ../src/nfp/NFPCalculator.cpp \
    ../src/nfp/MinkowskiSum.cpp \
    ../src/placement/PlacementStrategy.cpp \
    ../src/placement/PlacementWorker.cpp \
    ../src/placement/MergeDetection.cpp \
    ../src/algorithm/Individual.cpp \
    ../src/algorithm/Population.cpp \
    ../src/algorithm/GeneticAlgorithm.cpp \
    ../../Clipper2Lib/src/clipper.engine.cpp \
    ../../Clipper2Lib/src/clipper.offset.cpp \
    ../../Clipper2Lib/src/clipper.rectclip.cpp

# Boost libraries
LIBS += -lboost_system -lboost_thread -lpthread

# Enable optimizations
QMAKE_CXXFLAGS += -O2
