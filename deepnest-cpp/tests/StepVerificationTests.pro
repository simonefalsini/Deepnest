##############################################
# DeepNest C++ Step Verification Tests
##############################################

QT += core gui widgets

CONFIG += c++17
CONFIG += console

TARGET = StepVerificationTests
TEMPLATE = app

# Include paths
INCLUDEPATH += $$PWD/../include
INCLUDEPATH += $$PWD/../../Clipper2Lib/include

# Source files
SOURCES += StepVerificationTests.cpp

# Link against deepnest library
LIBS += -L$$PWD/../lib -ldeepnest

# Boost libraries
unix {
    LIBS += -lboost_thread -lboost_system
}

win32 {
    # Windows Boost paths - adjust as needed
    INCLUDEPATH += C:/boost/include
    LIBS += -LC:/boost/lib
    LIBS += -lboost_thread-mt -lboost_system-mt
}

# Clipper2
LIBS += -L$$PWD/../../Clipper2Lib/build -lClipper2
