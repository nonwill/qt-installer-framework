
/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
**************************************************************************/
#include "downloadfiletask.h"

#include "downloadfiletask_p.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QNetworkProxyFactory>
#include <QSslError>
#include <QTemporaryFile>
#include <QTimer>

namespace QInstaller {

AuthenticationRequiredException::AuthenticationRequiredException(Type type, const QString &message)
    : TaskException(message)
    , m_type(type)
{
}

Downloader::Downloader()
    : m_finished(0)
{
    connect(&m_nam, SIGNAL(finished(QNetworkReply*)), SLOT(onFinished(QNetworkReply*)));
}

Downloader::~Downloader()
{
    m_nam.disconnect();
    for (const auto &pair : m_downloads) {
        pair.first->disconnect();
        pair.first->abort();
        pair.first->deleteLater();
    }
}

void Downloader::download(QFutureInterface<FileTaskResult> &fi, const QList<FileTaskItem> &items,
    QNetworkProxyFactory *networkProxyFactory)
{
    m_items = items;
    m_futureInterface = &fi;

    fi.reportStarted();
    fi.setExpectedResultCount(items.count());

    m_nam.setProxyFactory(networkProxyFactory);
    connect(&m_nam, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)), this,
        SLOT(onAuthenticationRequired(QNetworkReply*,QAuthenticator*)));
    connect(&m_nam, SIGNAL(proxyAuthenticationRequired(QNetworkProxy,QAuthenticator*)), this,
            SLOT(onProxyAuthenticationRequired(QNetworkProxy,QAuthenticator*)));
    QTimer::singleShot(0, this, SLOT(doDownload()));
}

void Downloader::doDownload()
{
    foreach (const FileTaskItem &item, m_items) {
        if (!startDownload(item))
            break;
    }

    if (m_items.isEmpty() || m_futureInterface->isCanceled()) {
        m_futureInterface->reportFinished();
        emit finished();    // emit finished, so the event loop can shutdown
    }
}


// -- private slots

void Downloader::onReadyRead()
{
    if (testCanceled()) {
        m_futureInterface->reportFinished();
        emit finished(); return;    // error
    }

    QNetworkReply *const reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply)
        return;

    Data &data = *m_downloads[reply];
    if (!data.file) {
        std::unique_ptr<QFile> file = nullptr;
        const QString target = data.taskItem.target();
        if (target.isEmpty()) {
            std::unique_ptr<QTemporaryFile> tmp(new QTemporaryFile);
            tmp->setAutoRemove(false);
            file = std::move(tmp);
        } else {
            std::unique_ptr<QFile> tmp(new QFile(target));
            file = std::move(tmp);
        }

        if (file->exists() && (!QFileInfo(file->fileName()).isFile())) {
            m_futureInterface->reportException(TaskException(tr("Target file '%1' already exists "
                "but is not a file.").arg(file->fileName())));
            return;
        }

        if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            //: %2 is a sentence describing the error
            m_futureInterface->reportException(TaskException(tr("Could not open target '%1' for "
                "write. Error: %2.").arg(file->fileName(), file->errorString())));
            return;
        }
        data.file = std::move(file);
    }

    if (!data.file->isOpen()) {
        //: %2 is a sentence describing the error.
        m_futureInterface->reportException(
                    TaskException(tr("Target '%1' not open for write. Error: %2.").arg(
                                          data.file->fileName(), data.file->errorString())));
        return;
    }

    QByteArray buffer(32768, Qt::Uninitialized);
    while (reply->bytesAvailable()) {
        if (testCanceled()) {
            m_futureInterface->reportFinished();
            emit finished(); return;    // error
        }

        const qint64 read = reply->read(buffer.data(), buffer.size());
        qint64 written = 0;
        while (written < read) {
            const qint64 toWrite = data.file->write(buffer.constData() + written, read - written);
            if (toWrite < 0) {
                //: %2 is a sentence describing the error.
                m_futureInterface->reportException(
                            TaskException(tr("Writing to target '%1' failed. Error: %2.").arg(
                                                  data.file->fileName(), data.file->errorString())));
                return;
            }
            written += toWrite;
        }

        data.observer->addSample(read);
        data.observer->addBytesTransfered(read);
        data.observer->addCheckSumData(buffer.data(), read);

        int progress = m_finished * 100;
        for (const auto &pair : m_downloads)
            progress += pair.second->observer->progressValue();
        if (!reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid()) {
            m_futureInterface->setProgressValueAndText(progress / m_items.count(),
                data.observer->progressText());
        }
    }
}

void Downloader::onFinished(QNetworkReply *reply)
{
    Data &data = *m_downloads[reply];
    const QString filename = data.file ? data.file->fileName() : QString();
    if (!m_futureInterface->isCanceled()) {
        if (reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid()) {
            const QUrl url = reply->url()
                .resolved(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
            const QList<QUrl> redirects = m_redirects.values(reply);
            if (!redirects.contains(url)) {
                if (data.file)
                    data.file->remove();

                FileTaskItem taskItem = data.taskItem;
                taskItem.insert(TaskRole::SourceFile, url.toString());
                QNetworkReply *const redirectReply = startDownload(taskItem);

                foreach (const QUrl &redirect, redirects)
                    m_redirects.insertMulti(redirectReply, redirect);
                m_redirects.insertMulti(redirectReply, url);

                m_downloads.erase(reply);
                m_redirects.remove(reply);
                reply->deleteLater();
                return;
            } else {
                m_futureInterface->reportException(TaskException(tr("Redirect loop detected '%1'.")
                    .arg(url.toString())));
                return;
            }
        }
    }

    const QByteArray ba = reply->readAll();
    if (!ba.isEmpty()) {
        data.observer->addSample(ba.size());
        data.observer->addBytesTransfered(ba.size());
        data.observer->addCheckSumData(ba.data(), ba.size());
    }

    const QByteArray expectedCheckSum = data.taskItem.value(TaskRole::Checksum).toByteArray();
    if (!expectedCheckSum.isEmpty()) {
        if (expectedCheckSum != data.observer->checkSum().toHex()) {
            m_futureInterface->reportException(TaskException(tr("Checksum mismatch detected '%1'.")
                .arg(reply->url().toString())));
        }
    }
    m_futureInterface->reportResult(FileTaskResult(filename, data.observer->checkSum(), data.taskItem));

    m_downloads.erase(reply);
    m_redirects.remove(reply);
    reply->deleteLater();

    m_finished++;
    if (m_downloads.empty() || m_futureInterface->isCanceled()) {
        m_futureInterface->reportFinished();
        emit finished();    // emit finished, so the event loop can shutdown
    }
}

void Downloader::onError(QNetworkReply::NetworkError error)
{
    QNetworkReply *const reply = qobject_cast<QNetworkReply *>(sender());

    if (error == QNetworkReply::ProxyAuthenticationRequiredError)
        return; // already handled by onProxyAuthenticationRequired
    if (error == QNetworkReply::AuthenticationRequiredError)
        return; // already handled by onAuthenticationRequired

    if (reply) {
        const Data &data = *m_downloads[reply];
        //Do not throw error if Updates.xml not found. The repository might be removed
        //with RepositoryUpdate in Updates.xml later.
        if (data.taskItem.source().contains(QLatin1String("Updates.xml"), Qt::CaseInsensitive)) {
            qDebug() << QString::fromLatin1("Network error while downloading '%1': %2.").arg(
                   data.taskItem.source(), reply->errorString());
        } else {
            m_futureInterface->reportException(
                TaskException(tr("Network error while downloading '%1': %2.").arg(
                                      data.taskItem.source(), reply->errorString())));
        }
        //: %2 is a sentence describing the error

    } else {
        //: %1 is a sentence describing the error
        m_futureInterface->reportException(
                    TaskException(tr("Unknown network error while downloading: %1.").arg(error)));
    }
}

void Downloader::onSslErrors(const QList<QSslError> &sslErrors)
{
#ifdef QT_NO_SSL
    Q_UNUSED(sslErrors);
#else
    foreach (const QSslError &error, sslErrors)
        qDebug() << QString::fromLatin1("SSL error: %s").arg(error.errorString());
#endif
}

void Downloader::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    Q_UNUSED(bytesReceived)
    QNetworkReply *const reply = qobject_cast<QNetworkReply *>(sender());
    if (reply) {
        const Data &data = *m_downloads[reply];
        data.observer->setBytesToTransfer(bytesTotal);
    }
}

void Downloader::onAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator)
{
    if (!authenticator || !reply || m_downloads.find(reply) == m_downloads.cend())
        return;

    FileTaskItem *item = &m_downloads[reply]->taskItem;
    const QAuthenticator auth = item->value(TaskRole::Authenticator).value<QAuthenticator>();
    if (auth.user().isEmpty()) {
        AuthenticationRequiredException e(AuthenticationRequiredException::Type::Server,
            QCoreApplication::translate("AuthenticationRequiredException", "%1 at %2")
            .arg(authenticator->realm(), reply->url().host()));
        item->insert(TaskRole::Authenticator, QVariant::fromValue(QAuthenticator(*authenticator)));
        e.setFileTaskItem(*item);
        m_futureInterface->reportException(e);
    } else {
        authenticator->setUser(auth.user());
        authenticator->setPassword(auth.password());
        item->insert(TaskRole::Authenticator, QVariant()); // clear so we fail on next call
    }
}

void Downloader::onProxyAuthenticationRequired(const QNetworkProxy &proxy, QAuthenticator *)
{
    // Report to GUI thread.
    // (MetadataJob will ask for username/password, and restart the download ...)
    AuthenticationRequiredException e(AuthenticationRequiredException::Type::Proxy,
        QCoreApplication::translate("AuthenticationRequiredException",
        "Proxy requires authentication."));
    e.setProxy(proxy);
    m_futureInterface->reportException(e);
}


// -- private

bool Downloader::testCanceled()
{
    // TODO: figure out how to implement pause and resume
    if (m_futureInterface->isPaused()) {
        m_futureInterface->togglePaused();  // Note: this will trigger cancel
        m_futureInterface->reportException(
                    TaskException(tr("Pause and resume not supported by network transfers.")));
    }
    return m_futureInterface->isCanceled();
}

QNetworkReply *Downloader::startDownload(const FileTaskItem &item)
{
    QUrl const source = item.source();
    if (!source.isValid()) {
        //: %2 is a sentence describing the error
        m_futureInterface->reportException(TaskException(tr("Invalid source '%1'. Error: %2.")
            .arg(source.toString(), source.errorString())));
        return 0;
    }

    QNetworkReply *reply = m_nam.get(QNetworkRequest(source));
    std::unique_ptr<Data> data(new Data(item));
    m_downloads[reply] = std::move(data);

    connect(reply, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this,
        SLOT(onError(QNetworkReply::NetworkError)));
#ifndef QT_NO_SSL
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)), SLOT(onSslErrors(QList<QSslError>)));
#endif
    connect(reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(onDownloadProgress(qint64,
        qint64)));
    return reply;
}


// -- DownloadFileTask

DownloadFileTask::DownloadFileTask(const QList<FileTaskItem> &items)
    : AbstractFileTask()
{
    setTaskItems(items);
}

void DownloadFileTask::setTaskItem(const FileTaskItem &item)
{
    AbstractFileTask::setTaskItem(item);
}

void DownloadFileTask::addTaskItem(const FileTaskItem &item)
{
    AbstractFileTask::addTaskItem(item);
}

void DownloadFileTask::setTaskItems(const QList<FileTaskItem> &items)
{
    AbstractFileTask::setTaskItems(items);
}

void DownloadFileTask::addTaskItems(const QList<FileTaskItem> &items)
{
    AbstractFileTask::addTaskItems(items);
}

void DownloadFileTask::setAuthenticator(const QAuthenticator &authenticator)
{
    m_authenticator = authenticator;
}

void DownloadFileTask::setProxyFactory(KDUpdater::FileDownloaderProxyFactory *factory)
{
    m_proxyFactory.reset(factory);
}

void DownloadFileTask::doTask(QFutureInterface<FileTaskResult> &fi)
{
    QEventLoop el;
    Downloader downloader;
    connect(&downloader, SIGNAL(finished()), &el, SLOT(quit()));

    QList<FileTaskItem> items = taskItems();
    if (!m_authenticator.isNull()) {
        for (int i = 0; i < items.count(); ++i) {
            if (items.at(i).value(TaskRole::Authenticator).isNull())
                items[i].insert(TaskRole::Authenticator, QVariant::fromValue(m_authenticator));
        }
    }
    downloader.download(fi, items, (m_proxyFactory.isNull() ? 0 : m_proxyFactory->clone()));
    el.exec();  // That's tricky here, we need to run our own event loop to keep QNAM working.
}

}   // namespace QInstaller
