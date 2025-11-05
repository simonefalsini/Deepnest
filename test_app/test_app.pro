TEMPLATE = app
CONFIG += c++17 console
QT += core gui svg

INCLUDEPATH += ../include
LIBS += -L$$OUT_PWD/../deepnestlib -ldeepnest

SOURCES = main.cpp

DESTDIR = $$OUT_PWD/bin

