/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#ifndef KD_UPDATER_FILE_DOWNLOADER_P_H
#define KD_UPDATER_FILE_DOWNLOADER_P_H

#include "kdupdaterfiledownloader.h"

#include <QtNetwork/QNetworkReply>

// these classes are not a part of the public API

namespace KDUpdater {

class LocalFileDownloader : public FileDownloader
{
    Q_OBJECT

public:
    explicit LocalFileDownloader(QObject *parent = 0);
    ~LocalFileDownloader();

    bool canDownload() const;
    bool isDownloaded() const;
    QString downloadedFileName() const;
    void setDownloadedFileName(const QString &name);
    LocalFileDownloader *clone(QObject *parent = 0) const;

public Q_SLOTS:
    void cancelDownload();

protected:
    void timerEvent(QTimerEvent *te);
    void onError();
    void onSuccess();

private Q_SLOTS:
    void doDownload();

private:
    struct Private;
    Private *d;
};

class ResourceFileDownloader : public FileDownloader
{
    Q_OBJECT

public:
    explicit ResourceFileDownloader(QObject *parent = 0);
    ~ResourceFileDownloader();

    bool canDownload() const;
    bool isDownloaded() const;
    QString downloadedFileName() const;
    void setDownloadedFileName(const QString &name);
    ResourceFileDownloader *clone(QObject *parent = 0) const;

public Q_SLOTS:
    void cancelDownload();

protected:
    void timerEvent(QTimerEvent *te);
    void onError();
    void onSuccess();

private Q_SLOTS:
    void doDownload();

private:
    struct Private;
    Private *d;
};

class HttpDownloader : public FileDownloader
{
    Q_OBJECT

public:
    explicit HttpDownloader(QObject *parent = 0);
    ~HttpDownloader();

    bool canDownload() const;
    bool isDownloaded() const;
    QString downloadedFileName() const;
    void setDownloadedFileName(const QString &name);
    HttpDownloader *clone(QObject *parent = 0) const;

public Q_SLOTS:
    void cancelDownload();

protected:
    void onError();
    void onSuccess();
    void timerEvent(QTimerEvent *event);

private Q_SLOTS:
    void doDownload();

    void httpReadyRead();
    void httpReadProgress(qint64 done, qint64 total);
    void httpError(QNetworkReply::NetworkError);
    void httpDone(bool error);
    void httpReqFinished();
    void onAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator);
#ifndef QT_NO_OPENSSL
    // TODO: once we switch to Qt5, use QT_NO_SSL instead of QT_NO_OPENSSL
    void onSslErrors(QNetworkReply* reply, const QList<QSslError> &errors);
#endif
private:
    void startDownload(const QUrl &url);

private:
    struct Private;
    Private *d;
};

} // namespace KDUpdater

#endif // KD_UPDATER_FILE_DOWNLOADER_P_H
