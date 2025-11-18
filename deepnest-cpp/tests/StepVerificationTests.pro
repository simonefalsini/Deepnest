##############################################
# DeepNest C++ Step Verification Tests
##############################################

QT += core gui widgets

CONFIG += c++17
CONFIG += console

TARGET = StepVerificationTests
TEMPLATE = app

# Windows-specific settings
win32 {
    DEFINES += _WIN32_WINNT=0x0601  # Windows 7 target
}

# Include paths
INCLUDEPATH += $$PWD/../include
INCLUDEPATH += $$PWD/../../Clipper2Lib/include

# Source files
SOURCES += StepVerificationTests.cpp

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
OBJECTS_DIR = $$PWD/../build/verification_obj
MOC_DIR = $$PWD/../build/verification_moc
RCC_DIR = $$PWD/../build/verification_rcc
UI_DIR = $$PWD/../build/verification_ui
