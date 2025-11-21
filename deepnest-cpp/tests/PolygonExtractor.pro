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
LIBS += -L$$PWD/../lib -ldeepnest

INCLUDEPATH += $$PWD/../../../boost
LIBS += -L$$PWD/../../../boost/stage/lib
#LIBS += libboost_thread-vc141-mt-gd-x64-1_89.lib

