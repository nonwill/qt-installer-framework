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

#ifndef METADATAJOB_H
#define METADATAJOB_H

#include "downloadfiletask.h"
#include "fileutils.h"
#include "kdjob.h"
#include "repository.h"

#include <QFutureWatcher>

namespace QInstaller {

class PackageManagerCore;

struct Metadata
{
    QString directory;
    Repository repository;
};

class INSTALLER_EXPORT MetadataJob : public KDJob
{
    Q_OBJECT
    Q_DISABLE_COPY(MetadataJob)

    enum Status {
        XmlDownloadRetry,
        XmlDownloadFailure,
        XmlDownloadSuccess
    };

public:
    explicit MetadataJob(QObject *parent = 0);
    ~MetadataJob();

    QList<Metadata> metadata() const { return m_metadata.values(); }
    Repository repositoryForDirectory(const QString &directory) const;
    void setPackageManagerCore(PackageManagerCore *core) { m_core = core; }

private slots:
    void doStart();
    void doCancel();

    void xmlTaskFinished();
    void unzipTaskFinished();
    void metadataTaskFinished();
    void progressChanged(int progress);

private:
    void reset();
    Status parseUpdatesXml(const QList<FileTaskResult> &results);

private:
    PackageManagerCore *m_core;

    QList<FileTaskItem> m_packages;
    TempDirDeleter m_tempDirDeleter;
    QHash<QString, Metadata> m_metadata;
    QFutureWatcher<FileTaskResult> m_xmlTask;
    QFutureWatcher<FileTaskResult> m_metadataTask;
    QHash<QFutureWatcher<void> *, QObject*> m_unzipTasks;
};

}   // namespace QInstaller

#endif  // METADATAJOB_H
