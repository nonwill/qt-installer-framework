/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/
#include "init.h"

#include "createshortcutoperation.h"
#include "createdesktopentryoperation.h"
#include "createlocalrepositoryoperation.h"
#include "extractarchiveoperation.h"
#include "globalsettingsoperation.h"
#include "environmentvariablesoperation.h"
#include "registerfiletypeoperation.h"
#include "selfrestartoperation.h"
#include "installiconsoperation.h"
#include "elevatedexecuteoperation.h"
#include "fakestopprocessforupdateoperation.h"
#include "createlinkoperation.h"
#include "simplemovefileoperation.h"
#include "copydirectoryoperation.h"
#include "replaceoperation.h"
#include "linereplaceoperation.h"
#include "minimumprogressoperation.h"
#include "licenseoperation.h"
#include "settingsoperation.h"
#include "consumeoutputoperation.h"


#include "utils.h"

#include "kdupdaterupdateoperationfactory.h"
#include "kdupdaterfiledownloaderfactory.h"

#include "7zCrc.h"

#include <QtPlugin>
#include <QElapsedTimer>

#include <iostream>

namespace NArchive {
    namespace NXz {
        void registerArcxz();
    }
    namespace NSplit {
        void registerArcSplit();
    }
    namespace NLzma {
        namespace NLzmaAr {
            void registerArcLzma();
        }
        namespace NLzma86Ar {
            void registerArcLzma86();
        }
    }
}

void registerArc7z();

void registerCodecBCJ();
void registerCodecBCJ2();

void registerCodecLZMA();
void registerCodecLZMA2();

void registerCodecDelta();
void registerCodecBranch();
void registerCodecByteSwap();

using namespace KDUpdater;
using namespace QInstaller;

static void initArchives()
{
    CrcGenerateTable();

    registerArc7z();

    registerCodecBCJ();
    registerCodecBCJ2();
    registerCodecLZMA();
    registerCodecLZMA2();

    registerCodecDelta();
    registerCodecBranch();
    registerCodecByteSwap();

    NArchive::NXz::registerArcxz();
    NArchive::NSplit::registerArcSplit();
    NArchive::NLzma::NLzmaAr::registerArcLzma();
    NArchive::NLzma::NLzma86Ar::registerArcLzma86();
}

#if defined(QT_STATIC)
static void initResources()
{
    Q_INIT_RESOURCE(installer);
    // Qt5 or better qmake generates that automatically, so this is only needed on Qt4
# if QT_VERSION < 0x050000
    Q_IMPORT_PLUGIN(qico)
    Q_UNUSED(qt_plugin_instance_qico());
    Q_IMPORT_PLUGIN(qtaccessiblewidgets)
    Q_UNUSED(qt_plugin_instance_qtaccessiblewidgets());
# endif
}
#endif

static QByteArray trimAndPrepend(QtMsgType type, const QByteArray &msg)
{
    QByteArray ba(msg);
    // last character is a space from qDebug
    if (ba.endsWith(' '))
        ba.chop(1);

    // remove quotes if the whole message is surrounded with them
    if (ba.startsWith('"') && ba.endsWith('"'))
        ba = ba.mid(1, ba.length() - 2);

    // prepend the message type, skip QtDebugMsg
    switch (type) {
        case QtWarningMsg:
            ba.prepend("Warning: ");
        break;

        case QtCriticalMsg:
            ba.prepend("Critical: ");
        break;

        case QtFatalMsg:
            ba.prepend("Fatal: ");
        break;

        default:
            break;
    }
    return ba;
}

// start timer on construction (so we can use it as static member)
class Uptime : public QElapsedTimer {
public:
    Uptime() { start(); }
};

#if QT_VERSION < 0x050000
static void messageHandler(QtMsgType type, const char *msg)
{
    const QByteArray ba = trimAndPrepend(type, QByteArray::fromRawData(msg, strlen(msg)));
#else
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // suppress warning from QPA minimal plugin
    if (msg.contains(QLatin1String("This plugin does not support propagateSizeHints")))
        return;
    QByteArray ba = trimAndPrepend(type, msg.toLocal8Bit());
    if (type != QtDebugMsg) {
        ba += QString(QStringLiteral(" (%1:%2, %3)")).arg(
                    QString::fromLatin1(context.file)).arg(context.line).arg(
                    QString::fromLatin1(context.function)).toLocal8Bit();
    }
#endif

    static Uptime uptime;
    QString t = QLatin1Char('[') + QString::number(uptime.elapsed()) + QLatin1String("] ");

    verbose() << t.toUtf8() << ba << std::endl;
    if (!isVerbose() && type != QtDebugMsg)
        std::cout << ba.constData() << std::endl << std::endl;

    if (type == QtFatalMsg) {
#if QT_VERSION < 0x050000
        QtMsgHandler oldMsgHandler = qInstallMsgHandler(0);
        qt_message_output(type, msg);
        qInstallMsgHandler(oldMsgHandler);
#else
        QtMessageHandler oldMsgHandler = qInstallMessageHandler(0);
        qt_message_output(type, context, msg);
        qInstallMessageHandler(oldMsgHandler);
#endif
    }
}

void QInstaller::init()
{
    ::initArchives();
#if defined(QT_STATIC)
    ::initResources();
#endif

    UpdateOperationFactory &factory = UpdateOperationFactory::instance();
    factory.registerUpdateOperation<CreateShortcutOperation>(QLatin1String("CreateShortcut"));
    factory.registerUpdateOperation<CreateDesktopEntryOperation>(QLatin1String("CreateDesktopEntry"));
    factory.registerUpdateOperation<CreateLocalRepositoryOperation>(QLatin1String("CreateLocalRepository"));
    factory.registerUpdateOperation<ExtractArchiveOperation>(QLatin1String("Extract"));
    factory.registerUpdateOperation<GlobalSettingsOperation>(QLatin1String("GlobalConfig"));
    factory.registerUpdateOperation<EnvironmentVariableOperation>(QLatin1String("EnvironmentVariable"));
    factory.registerUpdateOperation<RegisterFileTypeOperation>(QLatin1String("RegisterFileType"));
    factory.registerUpdateOperation<SelfRestartOperation>(QLatin1String("SelfRestart"));
    factory.registerUpdateOperation<InstallIconsOperation>(QLatin1String("InstallIcons"));
    factory.registerUpdateOperation<ElevatedExecuteOperation>(QLatin1String("Execute"));
    factory.registerUpdateOperation<FakeStopProcessForUpdateOperation>(QLatin1String("FakeStopProcessForUpdate"));
    factory.registerUpdateOperation<CreateLinkOperation>(QLatin1String("CreateLink"));
    factory.registerUpdateOperation<SimpleMoveFileOperation>(QLatin1String("SimpleMoveFile"));
    factory.registerUpdateOperation<CopyDirectoryOperation>(QLatin1String("CopyDirectory"));
    factory.registerUpdateOperation<ReplaceOperation>(QLatin1String("Replace"));
    factory.registerUpdateOperation<LineReplaceOperation>(QLatin1String("LineReplace"));
    factory.registerUpdateOperation<MinimumProgressOperation>(QLatin1String("MinimumProgress"));
    factory.registerUpdateOperation<LicenseOperation>(QLatin1String("License"));
    factory.registerUpdateOperation<ConsumeOutputOperation>(QLatin1String("ConsumeOutput"));
    factory.registerUpdateOperation<SettingsOperation>(QLatin1String("Settings"));

    FileDownloaderFactory::setFollowRedirects(true);

   // qDebug -> verbose()
#if QT_VERSION < 0x050000
   qInstallMsgHandler(messageHandler);
#else
   qInstallMessageHandler(messageHandler);
#endif
}
