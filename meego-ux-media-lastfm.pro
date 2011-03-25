VERSION = 0.1.0
PROJECT_NAME = meego-ux-media-lastfm

TARGET = lastfm
TEMPLATE = lib

QT += network core
CONFIG += plugin link_pkgconfig
PKGCONFIG += meego-ux-media

# use pkg-config paths for include in both g++ and moc
INCLUDEPATH += $$system(pkg-config --cflags meego-ux-media \
    | tr \' \' \'\\n\' | grep ^-I | cut -d 'I' -f 2-)

SOURCES += \
    mediainfoplugin.cpp

HEADERS  += \
    mediainfoplugin.h

DESTDIR = metadata
OBJECTS_DIR = .obj
MOC_DIR = .moc

libdir.files += $$DESTDIR
libdir.path += $$[QT_INSTALL_PLUGINS]/MeeGo/Media
inidir.files += lastfm.ini
inidir.path += /usr/share/meego-ux-media/
INSTALLS += libdir inidir

TRANSLATIONS += mediainfoplugin.h mediainfoplugin.cpp
dist.commands += rm -fR $${PROJECT_NAME}-$${VERSION} &&
dist.commands += git clone . $${PROJECT_NAME}-$${VERSION} &&
dist.commands += rm -fR $${PROJECT_NAME}-$${VERSION}/.git &&
dist.commands += rm -f $${PROJECT_NAME}-$${VERSION}/.gitignore &&
dist.commands += mkdir -p $${PROJECT_NAME}-$${VERSION}/ts &&
dist.commands += lupdate $${TRANSLATIONS} -ts $${PROJECT_NAME}-$${VERSION}/ts/$${PROJECT_NAME}.ts &&
dist.commands += tar jcpvf $${PROJECT_NAME}-$${VERSION}.tar.bz2 $${PROJECT_NAME}-$${VERSION} &&
dist.commands += rm -fR $${PROJECT_NAME}-$${VERSION}
QMAKE_EXTRA_TARGETS += dist
