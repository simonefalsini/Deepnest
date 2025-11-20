##############################################
# DeepNest C++ Diagnostic Tool
##############################################

QT += core gui widgets

CONFIG += c++17
CONFIG += console

TARGET = DiagnosticTool
TEMPLATE = app

# Windows-specific settings
win32 {
    DEFINES += _WIN32_WINNT=0x0601  # Windows 7 target
}

# Include paths
INCLUDEPATH += $$PWD/../include
INCLUDEPATH += $$PWD/../../Clipper2Lib/include

# Source files
SOURCES += DiagnosticTool.cpp \
            SVGLoader.cpp

# Link against deepnest library
LIBS += -L$$PWD/../lib -ldeepnest

# Boost libraries
unix {
    LIBS += -lboost_thread -lboost_system -lboost_chrono -lpthread
}

win32 {
    # Windows Boost paths - adjust as needed
    INCLUDEPATH += $$PWD/../../../boost
    LIBS += -L$$PWD/../../../boost/lib64-msvc-14.1
    LIBS += boost_thread-vc141-mt-x64-1_89.lib
}

# Build directories
DESTDIR = $$PWD/../bin
OBJECTS_DIR = $$PWD/../build/diagnostic_obj
MOC_DIR = $$PWD/../build/diagnostic_moc
RCC_DIR = $$PWD/../build/diagnostic_rcc
UI_DIR = $$PWD/../build/diagnostic_ui
