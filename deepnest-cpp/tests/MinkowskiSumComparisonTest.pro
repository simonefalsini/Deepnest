QT -= gui
QT += core

CONFIG += c++17 console
CONFIG -= app_bundle

# Compiler flags
QMAKE_CXXFLAGS += -std=c++17

# Target
TARGET = MinkowskiSumComparisonTest
TEMPLATE = app

# Source files
SOURCES += MinkowskiSumComparisonTest.cpp

# Include paths
INCLUDEPATH += ../include
INCLUDEPATH += ../external/clipper2/CPP/Clipper2Lib/include

# Library paths and libraries
LIBS += -L../build -ldeepnest
LIBS += -L../external/clipper2/CPP/Build/Release -lClipper2

# Platform-specific settings
win32 {
    LIBS += -ladvapi32
}

unix {
    LIBS += -lpthread
}

# Output directory
DESTDIR = ../build/tests
OBJECTS_DIR = ../build/tests/obj
MOC_DIR = ../build/tests/moc
RCC_DIR = ../build/tests/rcc
UI_DIR = ../build/tests/ui
