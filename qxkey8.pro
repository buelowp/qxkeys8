QT -= gui

TARGET = xkey8
TEMPLATE = lib
CONFIG += debug

DEFINES += LIBQXKEY24_LIBRARY

SOURCES += XKey8.cpp

HEADERS += XKey8.h

unix {
    LIBS += -Bstatic -lpiehid -Bdynamic

    target.path = /usr/lib64
    INSTALLS += target

    header_files.files = $$HEADERS
    header_files.path = /usr/include
    INSTALLS += header_files
}
