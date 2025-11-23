TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle


# Include Qt core and gui for geometry types (QPainterPath, QPolygonF)
QT += core gui

TARGET = DeepnestLibTests

# Include paths
INCLUDEPATH += $$PWD/../include \
               $$PWD/../src


# Test source files
SOURCES += \
    $$PWD/DeepnestLibTests.cpp \
    $$PWD/FitnessTests.cpp \
    $$PWD/GeneticAlgorithmTests.cpp \
    $$PWD/JSComparisonTests.cpp \
    $$PWD/StepVerificationTests.cpp \
    $$PWD/SVGLoader.cpp \
    $$PWD/RandomShapeGenerator.cpp

# Headers
HEADERS += \
    $$PWD/TestRunners.h \
    $$PWD/SVGLoader.h \
    $$PWD/RandomShapeGenerator.h

# Link against DeepNest library
LIBS += -L$$PWD/../lib -ldeepnest

INCLUDEPATH += $$PWD/../../../boost
LIBS += -L$$PWD/../../../boost/stage/lib
#LIBS += libboost_thread-vc141-mt-gd-x64-1_89.lib

DEFINES += NOMINMAX
