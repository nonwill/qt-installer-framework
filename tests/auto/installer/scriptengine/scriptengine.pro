include(../../qttest.pri)

QT += script
greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
}

SOURCES += tst_scriptengine.cpp

RESOURCES += \
    scriptengine.qrc
