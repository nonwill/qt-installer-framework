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
#ifndef TESTREPOSITORY_H
#define TESTREPOSITORY_H

#include "qinstallerglobal.h"

#include <repository.h>
#include <settings.h>

#include <kdjob.h>

QT_BEGIN_NAMESPACE
class QAuthenticator;
class QLocale;
class QVariant;
QT_END_NAMESPACE

namespace KDUpdater {
    class FileDownloader;
}

namespace QInstaller {
    class PackageManagerCore;
}

namespace QInstaller {

class INSTALLER_EXPORT TestRepository : public KDJob
{
    Q_OBJECT

public:

    explicit TestRepository(QObject *parent = 0);
    ~TestRepository();

    QInstaller::Repository repository() const;
    void setRepository(const QInstaller::Repository &repository);

private:
    void doStart();
    void doCancel();

private Q_SLOTS:
    void downloadCompleted();
    void downloadAborted(const QString &reason);
    void onAuthenticatorChanged(const QAuthenticator &authenticator);

private:
    QInstaller::Repository m_repository;
    KDUpdater::FileDownloader *m_downloader;
};

} //namespace QInstaller

#endif  // TESTREPOSITORY_H
