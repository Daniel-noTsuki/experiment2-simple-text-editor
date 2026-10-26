QT += widgets
CONFIG += c++11
TEMPLATE = app
TARGET = SimpleTextEditor

SOURCES += main.cpp \
           mainwindow.cpp \
           codeeditor.cpp

HEADERS += mainwindow.h \
           codeeditor.h

FORMS += mainwindow.ui

