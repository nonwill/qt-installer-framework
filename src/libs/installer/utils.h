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

#ifndef QINSTALLER_UTILS_H
#define QINSTALLER_UTILS_H

#include "installer_global.h"

#include <QtCore/QBuffer>
#include <QtCore/QCryptographicHash>
#include <QtCore/QHash>
#include <QtCore/QUrl>
#include <QtCore/QTextStream>

#include <ostream>

QT_BEGIN_NAMESPACE
class QIODevice;
QT_END_NAMESPACE

namespace QInstaller {
    void INSTALLER_EXPORT uiDetachedWait(int ms);

    QByteArray INSTALLER_EXPORT calculateHash(QIODevice *device, QCryptographicHash::Algorithm algo);
    QByteArray INSTALLER_EXPORT calculateHash(const QString &path, QCryptographicHash::Algorithm algo);

    QString INSTALLER_EXPORT replaceVariables(const QHash<QString,QString> &vars, const QString &str);
    QString INSTALLER_EXPORT replaceWindowsEnvironmentVariables(const QString &str);
    QStringList INSTALLER_EXPORT parseCommandLineArgs(int argc, char **argv);
#ifdef Q_OS_WIN
    QString createCommandline(const QString &program, const QStringList &arguments);
#endif

    void INSTALLER_EXPORT setVerbose(bool v);
    bool INSTALLER_EXPORT isVerbose();

    INSTALLER_EXPORT std::ostream& stdverbose();
    INSTALLER_EXPORT std::ostream& operator<<(std::ostream &os, const QString &string);

    class VerboseWriter;
    INSTALLER_EXPORT VerboseWriter &verbose();

    class INSTALLER_EXPORT VerboseWriter : public QObject
    {
        Q_OBJECT
    public:
        explicit VerboseWriter(QObject *parent = 0);
        ~VerboseWriter();

        static VerboseWriter *instance();

        inline VerboseWriter &operator<<(const QString &t) { stdverbose() << t.toLocal8Bit().data(); stream << t; return *this; }
        inline VerboseWriter &operator<<(const QByteArray &t) { stdverbose() << t.data(); stream << t; return *this; }
        inline VerboseWriter &operator<<(const char *t) { stdverbose() << t; stream << t; return *this; }
        inline VerboseWriter &operator<<(std::ostream& (*f)(std::ostream &s)) { stdverbose() << *f; stream << "\n"; return *this; }
    public slots:
        void setOutputStream(const QString &fileName);

    private:
        QTextStream stream;
        QBuffer preFileBuffer;
        QString logFileName;
        QString currentDateTimeAsString;
    };

}

#endif // QINSTALLER_UTILS_H
