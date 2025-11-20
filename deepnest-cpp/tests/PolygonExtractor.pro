TEMPLATE = app
TARGET = PolygonExtractor
CONFIG += console c++17
CONFIG -= app_bundle
QT += core xml

# Output directory
DESTDIR = ../bin

# Build directories
OBJECTS_DIR = ../build/extractor_obj
MOC_DIR = ../build/extractor_moc

# Include paths
INCLUDEPATH += . \
               ../include \
               ../../Clipper2Lib/include

# Source files
SOURCES += PolygonExtractor.cpp \
           SVGLoader.cpp

HEADERS += SVGLoader.h

# Link against DeepNest library
LIBS += -L../lib -ldeepnest
LIBS += -lboost_thread -lboost_system -lboost_chrono
