TEMPLATE = lib
TARGET = installer
INCLUDEPATH += . ..

include(../7zip/7zip.pri)
include(../kdtools/kdtools.pri)
include(../../../installerfw.pri)

# productkeycheck API
# call qmake "PRODUCTKEYCHECK_PRI_FILE=<your_path_to_pri_file>"
# content of that pri file needs to be:
#   SOURCES += $$PWD/productkeycheck.cpp
#   ...
#   your files if needed
HEADERS += productkeycheck.h
!isEmpty(PRODUCTKEYCHECK_PRI_FILE) {
    # use undocumented no_batch config which disable the implicit rules on msvc compilers
    # this fixes the problem that same cpp files in different directories are overwritting
    # each other
    CONFIG += no_batch
    include($$PRODUCTKEYCHECK_PRI_FILE)
} else {
    SOURCES += productkeycheck.cpp
}

DESTDIR = $$IFW_LIB_PATH
DLLDESTDIR = $$IFW_APP_PATH

DEFINES += BUILD_LIB_INSTALLER

QT += \
    script \
    network \
    xml

isEqual(QT_MAJOR_VERSION, 5) {
  QT += concurrent widgets core-private
}

HEADERS += packagemanagercore.h \
    packagemanagercore_p.h \
    packagemanagergui.h \
    binaryformat.h \
    binaryformatengine.h \
    binaryformatenginehandler.h \
    repository.h \
    utils.h \
    errors.h \
    component.h \
    scriptengine.h \
    componentmodel.h \
    qinstallerglobal.h \
    qtpatch.h \
    qtpatchoperation.h \
    consumeoutputoperation.h \
    replaceoperation.h \
    linereplaceoperation.h \
    setqtcreatorvalueoperation.h \
    addqtcreatorarrayvalueoperation.h \
    copydirectoryoperation.h \
    simplemovefileoperation.h \
    extractarchiveoperation.h \
    extractarchiveoperation_p.h \
    globalsettingsoperation.h \
    createshortcutoperation.h \
    createdesktopentryoperation.h \
    registerfiletypeoperation.h \
    environmentvariablesoperation.h \
    installiconsoperation.h \
    selfrestartoperation.h \
    settings.h \
    downloadarchivesjob.h \
    init.h \
    updater.h \
    operationrunner.h \
    updatesettings.h \
    adminauthorization.h \
    fsengineclient.h \
    fsengineserver.h \
    elevatedexecuteoperation.h \
    fakestopprocessforupdateoperation.h \
    lazyplaintextedit.h \
    progresscoordinator.h \
    minimumprogressoperation.h \
    performinstallationform.h \
    messageboxhandler.h \
    licenseoperation.h \
    component_p.h \
    qtcreator_constants.h \
    qprocesswrapper.h \
    qsettingswrapper.h \
    constants.h \
    packagemanagerproxyfactory.h \
    createlocalrepositoryoperation.h \
    lib7z_facade.h \
    link.h \
    createlinkoperation.h \
    packagemanagercoredata.h \
    applyproductkeyoperation.h \
    globals.h \
    graph.h \
    settingsoperation.h \
    testrepository.h \
    packagemanagerpagefactory.h \
    abstracttask.h\
    abstractfiletask.h \
    copyfiletask.h \
    downloadfiletask.h \
    downloadfiletask_p.h \
    unziptask.h \
    observer.h \
    runextensions.h \
    metadatajob.h \
    metadatajob_p.h

    SOURCES += packagemanagercore.cpp \
    packagemanagercore_p.cpp \
    packagemanagergui.cpp \
    binaryformat.cpp \
    binaryformatengine.cpp \
    binaryformatenginehandler.cpp \
    repository.cpp \
    fileutils.cpp \
    utils.cpp \
    component.cpp \
    scriptengine.cpp \
    componentmodel.cpp \
    qtpatch.cpp \
    qtpatchoperation.cpp  \
    consumeoutputoperation.cpp \
    replaceoperation.cpp \
    linereplaceoperation.cpp \
    setqtcreatorvalueoperation.cpp \
    addqtcreatorarrayvalueoperation.cpp \
    copydirectoryoperation.cpp \
    simplemovefileoperation.cpp \
    extractarchiveoperation.cpp \
    globalsettingsoperation.cpp \
    createshortcutoperation.cpp \
    createdesktopentryoperation.cpp \
    registerfiletypeoperation.cpp \
    environmentvariablesoperation.cpp \
    installiconsoperation.cpp \
    selfrestartoperation.cpp \
    downloadarchivesjob.cpp \
    init.cpp \
    updater.cpp \
    operationrunner.cpp \
    updatesettings.cpp \
    adminauthorization.cpp \
    fsengineclient.cpp \
    fsengineserver.cpp \
    elevatedexecuteoperation.cpp \
    fakestopprocessforupdateoperation.cpp \
    lazyplaintextedit.cpp \
    progresscoordinator.cpp \
    minimumprogressoperation.cpp \
    performinstallationform.cpp \
    messageboxhandler.cpp \
    licenseoperation.cpp \
    component_p.cpp \
    qprocesswrapper.cpp \
    templates.cpp \
    qsettingswrapper.cpp \
    settings.cpp \
    packagemanagerproxyfactory.cpp \
    createlocalrepositoryoperation.cpp \
    lib7z_facade.cpp \
    link.cpp \
    createlinkoperation.cpp \
    packagemanagercoredata.cpp \
    applyproductkeyoperation.cpp \
    globals.cpp \
    settingsoperation.cpp \
    testrepository.cpp \
    packagemanagerpagefactory.cpp \
    abstractfiletask.cpp \
    copyfiletask.cpp \
    downloadfiletask.cpp \
    unziptask.cpp \
    observer.cpp \
    metadatajob.cpp

RESOURCES += resources/patch_file_lists.qrc \
             resources/installer.qrc

macx {
    HEADERS += \
               macreplaceinstallnamesoperation.h
    SOURCES += adminauthorization_mac.cpp \
               macreplaceinstallnamesoperation.cpp
}

unix:!macx:SOURCES += adminauthorization_x11.cpp

LIBS += -l7z
win32 {
    SOURCES += adminauthorization_win.cpp sysinfo_win.cpp

    LIBS += -loleaut32 -luser32     # 7zip
    LIBS += -ladvapi32 -lpsapi      # kdtools
    LIBS += -lole32 -lshell32       # createshortcutoperation

    win32-g++*:LIBS += -lmpr -luuid
    win32-g++*:QMAKE_CXXFLAGS += -Wno-missing-field-initializers
}
