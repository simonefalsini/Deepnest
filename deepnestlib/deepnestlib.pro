TEMPLATE = lib
CONFIG += c++17 warn_on
QT += core gui

TARGET = deepnest

INCLUDEPATH += ../include \
               ../Clipper2Lib/include \
               D:\buildOpencv\boost


HEADERS = \
    ../include/deepnest/ClipperBridge.h \
    ../include/deepnest/DeepNestConfig.h \
    ../include/deepnest/DeepNestSolver.h \
    ../include/deepnest/GeneticAlgorithm.h \
    ../include/deepnest/GeometryTypes.h \
    ../include/deepnest/GeometryUtils.h \
    ../include/deepnest/MinkowskiConvolution.h \
    ../include/deepnest/NestEngine.h \
    ../include/deepnest/NestPartRepository.h \
    ../include/deepnest/NestPolygon.h \
    ../include/deepnest/NfpCache.h \
    ../include/deepnest/NfpGenerator.h \
    ../include/deepnest/PlacementCostEvaluator.h \
    ../include/deepnest/PlacementWorker.h \
    ../include/deepnest/QtGeometryConverters.h \
    ../include/deepnest/ThreadPool.h

SOURCES = \
    ../src/DeepNestSolver.cpp \
    ../src/config/DeepNestConfig.cpp \
    ../src/core/GeneticAlgorithm.cpp \
    ../src/core/NestEngine.cpp \
    ../src/core/PlacementCostEvaluator.cpp \
    ../src/core/PlacementWorker.cpp \
    ../src/geometry/ClipperBridge.cpp \
    ../src/geometry/GeometryUtils.cpp \
    ../src/geometry/MinkowskiConvolution.cpp \
    ../src/geometry/NestPartRepository.cpp \
    ../src/geometry/NestPolygon.cpp \
    ../src/geometry/NfpCache.cpp \
    ../src/geometry/NfpGenerator.cpp \
    ../src/geometry/QtGeometryConverters.cpp \
    ../src/utils/ThreadPool.cpp
