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
#include "copyfiletask.h"
#include "observer.h"

#include <QFileInfo>
#include <QTemporaryFile>

namespace QInstaller {

CopyFileTask::CopyFileTask(const FileTaskItem &item)
    : AbstractFileTask(item)
{
}

CopyFileTask::CopyFileTask(const QString &source)
    : AbstractFileTask(source)
{
}

CopyFileTask::CopyFileTask(const QString &source, const QString &target)
    : AbstractFileTask(source, target)
{
}

void CopyFileTask::doTask(QFutureInterface<FileTaskResult> &fi)
{
    fi.reportStarted();
    fi.setExpectedResultCount(1);

    if (taskItems().isEmpty()) {
        fi.reportException(FileTaskException(QLatin1String("Invalid task item count.")));
        fi.reportFinished(); return;    // error
    }

    const FileTaskItem item = taskItems().first();
    FileTaskObserver observer(QCryptographicHash::Sha1);

    QFile source(item.source());
    if (!source.open(QIODevice::ReadOnly)) {
        fi.reportException(FileTaskException(QString::fromLatin1("Could not open source '%1' "
            "for read. Error: %2.").arg(source.fileName(), source.errorString())));
        fi.reportFinished(); return;    // error
    }
    observer.setBytesToTransfer(source.size());

    QScopedPointer<QFile> file;
    const QString target = item.target();
    if (target.isEmpty()) {
        QTemporaryFile *tmp = new QTemporaryFile;
        tmp->setAutoRemove(false);
        file.reset(tmp);
    } else {
        file.reset(new QFile(target));
    }
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fi.reportException(FileTaskException(QString::fromLatin1("Could not open target '%1' "
            "for write. Error: %2.").arg(file->fileName(), file->errorString())));
        fi.reportFinished(); return;    // error
    }

    QByteArray buffer(32768, Qt::Uninitialized);
    while (!source.atEnd() && source.error() == QFile::NoError) {
        if (fi.isCanceled())
            break;
        if (fi.isPaused())
            fi.waitForResume();

        const qint64 read = source.read(buffer.data(), buffer.size());
        qint64 written = 0;
        while (written < read) {
            const qint64 toWrite = file->write(buffer.constData() + written, read - written);
            if (toWrite < 0) {
                fi.reportException(FileTaskException(QString::fromLatin1("Writing to target "
                    "'%1' failed. Error: %2.").arg(file->fileName(), file->errorString())));
            }
            written += toWrite;
        }

        observer.addSample(read);
        observer.timerEvent(NULL);
        observer.addBytesTransfered(read);
        observer.addCheckSumData(buffer.data(), read);

        fi.setProgressValueAndText(observer.progressValue(), observer.progressText());
    }

    fi.reportResult(FileTaskResult(file->fileName(), observer.checkSum(), item), 0);
    fi.reportFinished();
}

}   // namespace QInstaller
