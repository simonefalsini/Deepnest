TEMPLATE = lib
TARGET = deepnest
CONFIG += c++17 staticlib
QT += core gui widgets

# Remove default compiler flags
CONFIG -= debug_and_release debug_and_release_target

# Compiler settings
#QMAKE_CXXFLAGS += -std=c++17
#QMAKE_CXXFLAGS_WARN_ON += -Wall

# Build directories
DESTDIR = $$PWD/lib
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR = $$PWD/build/moc
RCC_DIR = $$PWD/build/rcc
UI_DIR = $$PWD/build/ui

# Include paths
INCLUDEPATH += $$PWD/include
INCLUDEPATH += $$PWD/include/deepnest

# Boost
INCLUDEPATH += ../../boost
LIBS += -L../../boost/lib64-msvc-14.1
LIBS += -lboost_thread
LIBS += -lboost_system
LIBS += -lpthread

# Clipper2
INCLUDEPATH += $$PWD/../Clipper2Lib/include
INCLUDEPATH += $$PWD/../Clipper2Lib/include/clipper2

# Add Clipper2 sources directly
SOURCES += $$PWD/../Clipper2Lib/src/clipper.engine.cpp \
           $$PWD/../Clipper2Lib/src/clipper.offset.cpp \
           $$PWD/../Clipper2Lib/src/clipper.rectclip.cpp

# Headers
HEADERS += \
    include/deepnest/core/Types.h \
    include/deepnest/core/Point.h \
    include/deepnest/core/BoundingBox.h \
    include/deepnest/core/Polygon.h \
    include/deepnest/geometry/GeometryUtil.h \
    include/deepnest/geometry/PolygonOperations.h \
    include/deepnest/geometry/ConvexHull.h \
    include/deepnest/geometry/Transformation.h \
    include/deepnest/nfp/NFPCache.h \
    include/deepnest/nfp/MinkowskiSum.h \
    include/deepnest/nfp/NFPCalculator.h \
    include/deepnest/config/DeepNestConfig.h \
    include/deepnest/algorithm/Individual.h

# Sources
SOURCES += \
    src/core/Types.cpp \
    src/core/Point.cpp \
    src/core/Polygon.cpp \
    src/geometry/GeometryUtil.cpp \
    src/geometry/PolygonOperations.cpp \
    src/geometry/ConvexHull.cpp \
    src/geometry/Transformation.cpp \
    src/nfp/NFPCache.cpp \
    src/nfp/MinkowskiSum.cpp \
    src/nfp/NFPCalculator.cpp \
    src/config/DeepNestConfig.cpp \
    src/algorithm/Individual.cpp

# Installation
headers.files = $$HEADERS
headers.path = /usr/local/include/deepnest
target.path = /usr/local/lib

INSTALLS += target headers
