/**************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#ifndef DOWNLOADFILETASK_H
#define DOWNLOADFILETASK_H

#include "abstractfiletask.h"
#include "kdupdaterfiledownloaderfactory.h"

#include <QAuthenticator>
Q_DECLARE_METATYPE(QAuthenticator)

namespace QInstaller {

namespace TaskRole {
enum
{
    Authenticator = TaskRole::TargetFile + 10
};
}

class INSTALLER_EXPORT DownloadFileTask : public AbstractFileTask
{
    Q_OBJECT
    Q_DISABLE_COPY(DownloadFileTask)

public:
    DownloadFileTask() {}
    explicit DownloadFileTask(const FileTaskItem &item)
        : AbstractFileTask(item) {}
    explicit DownloadFileTask(const QList<FileTaskItem> &items);

    explicit DownloadFileTask(const QString &source)
        : AbstractFileTask(source) {}
    DownloadFileTask(const QString &source, const QString &target)
        : AbstractFileTask(source, target) {}

    void addTaskItem(const FileTaskItem &items);
    void addTaskItems(const QList<FileTaskItem> &items);

    void setTaskItem(const FileTaskItem &items);
    void setTaskItems(const QList<FileTaskItem> &items);

    void setAuthenticator(const QAuthenticator &authenticator);
    void setProxyFactory(KDUpdater::FileDownloaderProxyFactory *factory);

    void doTask(QFutureInterface<FileTaskResult> &fi);

private:
    friend class Downloader;
    QAuthenticator m_authenticator;
    QScopedPointer<KDUpdater::FileDownloaderProxyFactory> m_proxyFactory;
};

}   // namespace QInstaller

#endif // DOWNLOADFILETASK_H
