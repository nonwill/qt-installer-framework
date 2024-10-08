win32 {
    7ZIP_BASE=$$PWD/win
    INCLUDEPATH += $$7ZIP_BASE/C $$7ZIP_BASE/CPP
    DEFINES += WIN_LONG_PATH _UNICODE _NO_CRYPTO
    msvc*:QMAKE_CXXFLAGS_RELEASE -= -Zc:strictStrings
    msvc*:QMAKE_CXXFLAGS_RELEASE_WITH_DEBUGINFO -= -Zc:strictStrings
}

unix {
    7ZIP_BASE=$$PWD/unix
    INCLUDEPATH += \
        $$7ZIP_BASE/C \
        $$7ZIP_BASE/CPP \
        $$7ZIP_BASE/CPP/myWindows \
        $$7ZIP_BASE/CPP/include_windows

    macx:DEFINES += ENV_MACOSX
    DEFINES += _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE NDEBUG _REENTRANT ENV_UNIX UNICODE _UNICODE _NO_CRYPTO
}
