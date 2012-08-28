TARGET = qscreenlinuxfbofs
include(qpluginbase.pri)

INCLUDEPATH +=/home1/mahesh/cmem/include
QTDIR_build:DESTDIR = $$QT_BUILD_TREE/plugins/gfxdrivers
LIBS += /home1/mahesh/cmem/lib/cmem.a470MV
target.path = $$[QT_INSTALL_PLUGINS]/gfxdrivers
INSTALLS += target

DEFINES += QT_QWS_LINUXFB

HEADERS	= qscreenlinuxfb_qws.h

SOURCES	= main.cpp \
          qscreenlinuxfb_qws.cpp
