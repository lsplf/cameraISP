#-------------------------------------------------
#
# Project created by QtCreator 2019-04-30T09:29:10
#
#-------------------------------------------------

QT       += core gui multimedia multimediawidgets printsupport network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = cameraTest
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

linux-g++ | linux-g++-64 | linux-g++-32 {
# Tell qmake to use pkg-config to find QtGStreamer.
CONFIG += link_pkgconfig c++11

PKGCONFIG += Qt5GStreamer-1.0 Qt5GStreamerUi-1.0 Qt5GStreamerUtils-1.0 gstreamer-1.0 gstreamer-video-1.0
}

SOURCES += \
        main.cpp \
        mainwindow.cpp \
    selectroi.cpp \
    camerartp.cpp

HEADERS += \
        mainwindow.h \
    selectroi.h \
    camerartp.h

FORMS += \
        mainwindow.ui

INCLUDEPATH += /usr/local/include/opencv4
LIBS += -L/usr/local/lib -lopencv_shape -lopencv_videoio -lopencv_imgcodecs
LIBS += -lopencv_highgui -lopencv_core -lopencv_imgproc -lopencv_features2d -lopencv_calib3d

#LIBS += /usr/local/lib/libopencv_core.so
#LIBS += /usr/local/lib/libopencv_highgui.so
#LIBS += /usr/local/lib/libopencv_imgcodecs.so
#LIBS += /usr/local/lib/libopencv_imgproc.so
#LIBS += /usr/local/lib/libopencv_features2d.so
#LIBS += /usr/local/lib/libopencv_calib3d.so
