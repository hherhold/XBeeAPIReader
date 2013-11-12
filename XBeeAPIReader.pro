#-------------------------------------------------
#
# Project created by QtCreator 2013-06-23T07:03:52
#
#-------------------------------------------------

QT       += core gui
QT += serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = XBeeAPIReader
TEMPLATE = app


SOURCES += main.cpp\
        XBeeAPIReaderDialog.cpp

HEADERS  += XBeeAPIReaderDialog.h

FORMS    += XBeeAPIReaderDialog.ui
