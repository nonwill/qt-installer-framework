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

#ifndef QPROCESSWRAPPER_H
#define QPROCESSWRAPPER_H

#include <installer_global.h>

#include<QtCore/QIODevice>
#include<QtCore/QObject>
#include<QtCore/QProcess>

class INSTALLER_EXPORT QProcessWrapper : public QObject
{
    Q_OBJECT
public:
    enum ProcessState {
        NotRunning,
        Starting,
        Running
    };

    enum ExitStatus {
        NormalExit,
        CrashExit
    };

    enum ProcessChannel {
        StandardOutput = 0,
        StandardError = 1
    };

    enum ProcessChannelMode {
        SeparateChannels = 0,
        MergedChannels = 1,
        ForwardedChannels = 2
    };

    explicit QProcessWrapper(QObject *parent = 0);
    ~QProcessWrapper();

    void closeWriteChannel();
    int exitCode() const;
    ExitStatus exitStatus() const;
    void kill();
    void terminate();
    QByteArray readAll();
    QByteArray readAllStandardOutput();
    QByteArray readAllStandardError();
    void setWorkingDirectory(const QString &dir);

    void start(const QString &program);
    void start(const QString &program, const QStringList &arguments,
        QIODevice::OpenMode mode = QIODevice::ReadWrite);

    static bool startDetached(const QString &program);
    static bool startDetached(const QString &program, const QStringList &arguments);
    static bool startDetached(const QString &program, const QStringList &arguments,
        const QString &workingDirectory, qint64 *pid = 0);

    ProcessState state() const;
    bool waitForStarted(int msecs = 30000);
    bool waitForFinished(int msecs = 30000);
    void setEnvironment(const QStringList &environment);
    QString workingDirectory() const;
    QString errorString() const;
    qint64 write(const QByteArray &byteArray);
    QProcessWrapper::ProcessChannel readChannel() const;
    void setReadChannel(QProcessWrapper::ProcessChannel channel);
    QProcessWrapper::ProcessChannelMode processChannelMode() const;
    void setProcessChannelMode(QProcessWrapper::ProcessChannelMode channel);
#ifdef Q_OS_WIN
    void setNativeArguments(const QString &arguments);
#endif

Q_SIGNALS:
    void bytesWritten(qint64);
    void aboutToClose();
    void readChannelFinished();
    void error(QProcess::ProcessError);
    void readyReadStandardOutput();
    void readyReadStandardError();
    void finished(int exitCode);
    void finished(int exitCode, QProcess::ExitStatus exitStatus);
    void readyRead();
    void started();
    void stateChanged(QProcess::ProcessState newState);

public Q_SLOTS:
    void cancel();

protected:
    void timerEvent(QTimerEvent *event);

private:
    class Private;
    Private *d;
};

#endif  // QPROCESSWRAPPER_H
