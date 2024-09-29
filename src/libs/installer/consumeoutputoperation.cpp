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

#include "consumeoutputoperation.h"
#include "packagemanagercore.h"
#include "utils.h"

#include <QFile>
#include <QDir>
#include <QProcess>
#include <QDebug>

using namespace QInstaller;

ConsumeOutputOperation::ConsumeOutputOperation()
{
    setName(QLatin1String("ConsumeOutput"));
}

void ConsumeOutputOperation::backup()
{
}

bool ConsumeOutputOperation::performOperation()
{
    // Arguments:
    // 1. key where the output will be saved
    // 2. executable path
    // 3. argument for the executable
    // 4. more arguments possible ...
    if (arguments().count() < 3) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, %2 expected%3.").arg(name()).arg(
            arguments().count()).arg(tr("at least 2"), QLatin1String("(<to be saved installer key name>, "
            "<executable>, [argument1], [argument2], ...)")));
        return false;
    }

    PackageManagerCore *const core = value(QLatin1String("installer")).value<PackageManagerCore*>();
    if (!core) {
        setError(UserDefinedError);
        setErrorString(tr("Needed installer object in %1 operation is empty.").arg(name()));
        return false;
    }

    const QString installerKeyName = arguments().at(0);
    if (installerKeyName.isEmpty()) {
        setError(UserDefinedError);
        setErrorString(tr("Can not save the output of %1 to an empty installer key value.").arg(
            arguments().at(1)));
        return false;
    }

    QString executablePath = arguments().at(1);
    QFileInfo executable(executablePath);
#ifdef Q_OS_WIN
    if (!executable.exists() && executable.suffix().isEmpty())
        executable = QFileInfo(executablePath + QLatin1String(".exe"));
#endif

    if (!executable.exists() || !executable.isExecutable()) {
        setError(UserDefinedError);
        setErrorString(tr("File '%1' does not exist or is not an executable binary.").arg(
            QDir::toNativeSeparators(executable.absoluteFilePath())));
        return false;
    }

    QByteArray executableOutput;


    const QStringList processArguments = arguments().mid(2);
    // in some cases it is not runable, because another process is blocking it(filewatcher ...)
    int waitCount = 0;
    while (executableOutput.isEmpty() && waitCount < 3) {
        QProcess process;
        process.start(executable.absoluteFilePath(), processArguments, QIODevice::ReadOnly);
        if (process.waitForFinished(10000)) {
            if (process.exitStatus() == QProcess::CrashExit) {
                qWarning() << executable.absoluteFilePath() << processArguments
                           << "crashed with exit code" << process.exitCode()
                           << "standard output: " << process.readAllStandardOutput()
                           << "error output: " << process.readAllStandardError();
                setError(UserDefinedError);
                setErrorString(tr("Running '%1' resulted in a crash.").arg(
                    QDir::toNativeSeparators(executable.absoluteFilePath())));
                return false;
            }
            executableOutput.append(process.readAllStandardOutput());
        }
        if (executableOutput.isEmpty()) {
            ++waitCount;
            static const int waitTimeInMilliSeconds = 500;
            uiDetachedWait(waitTimeInMilliSeconds);
        }
        if (process.state() > QProcess::NotRunning ) {
            qWarning() << executable.absoluteFilePath() << "process is still running, need to kill it.";
            process.kill();
        }

    }
    if (executableOutput.isEmpty()) {
        qWarning() << QString::fromLatin1("Cannot get any query output from executable: '%1'").arg(
            executable.absoluteFilePath());
    }
    core->setValue(installerKeyName, QString::fromLocal8Bit(executableOutput));
    return true;
}

bool ConsumeOutputOperation::undoOperation()
{
    return true;
}

bool ConsumeOutputOperation::testOperation()
{
    return true;
}

Operation *ConsumeOutputOperation::clone() const
{
    return new ConsumeOutputOperation();
}

