##############################################
# DeepNest C++ Qt Test Application
##############################################

QT += core gui widgets xml

CONFIG += c++17
CONFIG += console

TARGET = TestApplication
TEMPLATE = app

# Windows-specific settings
win32 {
    DEFINES += _WIN32_WINNT=0x0601  # Windows 7 target
}

# Include paths
INCLUDEPATH += $$PWD/../include
INCLUDEPATH += $$PWD/../../Clipper2Lib/include

# Source files
SOURCES += \
    main.cpp \
    TestApplication.cpp \
    SVGLoader.cpp \
    RandomShapeGenerator.cpp

HEADERS += \
    TestApplication.h \
    SVGLoader.h \
    RandomShapeGenerator.h

# Link against deepnest library
LIBS += -L$$PWD/../lib -ldeepnest

# Boost libraries
unix {
    LIBS += -lboost_thread -lboost_system -lpthread
}

win32 {
    # Windows Boost paths - adjust as needed
    INCLUDEPATH += C:/boost/include
    LIBS += -LC:/boost/lib
    LIBS += -lboost_thread-mt -lboost_system-mt
}

# Clipper2
LIBS += -L$$PWD/../../Clipper2Lib/build -lClipper2

# Build directories
DESTDIR = $$PWD/../bin
OBJECTS_DIR = $$PWD/../build/test_app_obj
MOC_DIR = $$PWD/../build/test_app_moc
RCC_DIR = $$PWD/../build/test_app_rcc
UI_DIR = $$PWD/../build/test_app_ui
