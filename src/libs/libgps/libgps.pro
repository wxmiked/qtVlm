######################################################################
# Automatically generated by qmake (2.01a) Sat Jan 5 17:44:47 2013
######################################################################

TEMPLATE=lib
TARGET=gpsmm
CONFIG+=staticlib
INCLUDEPATH += .

QT -= gui

DESTDIR=../build

#DEFINES += USE_QT

# Input
HEADERS += gps_json.h \
           gpsd.h \
           gpsd_config.h \
           gpsdclient.h \
           json.h \
           libgps.h \
           sockaddr.h
           
SOURCES += ais_json.c \
           daemon.c \
           gps_maskdump.c \
           gpsdclient.c \
           gpsutils.c \
           hex.c \
           json.c \           
           libgps_dbus.c \
           libgps_json.c \
           libgps_shm.c \
           libgps_sock.c \
           netlib.c \
           rtcm2_json.c \
           shared_json.c \
           strl.c \
           libgps_core.c
