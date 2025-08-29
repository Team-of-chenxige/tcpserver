QT += core network sql

CONFIG += c++11

SOURCES += main.cpp \
           tcpserver.cpp \
           clientthread.cpp

HEADERS += tcpserver.h \
           clientthread.h

RESOURCES +=
