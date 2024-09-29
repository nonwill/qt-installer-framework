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

#ifndef KD_UPDATER_FILE_DOWNLOADER_FACTORY_H
#define KD_UPDATER_FILE_DOWNLOADER_FACTORY_H

#include "kdupdater.h"
#include <kdgenericfactory.h>

#include <QtCore/QStringList>
#include <QtCore/QUrl>

#include <QtNetwork/QNetworkProxyFactory>

QT_BEGIN_NAMESPACE
class QObject;
QT_END_NAMESPACE

namespace KDUpdater {

class FileDownloader;

class KDTOOLS_EXPORT FileDownloaderProxyFactory : public QNetworkProxyFactory
{
    public:
        virtual ~FileDownloaderProxyFactory() {}
        virtual FileDownloaderProxyFactory *clone() const = 0;
};

class KDTOOLS_EXPORT FileDownloaderFactory : public KDGenericFactory<FileDownloader>
{
    Q_DISABLE_COPY(FileDownloaderFactory)
    struct FileDownloaderFactoryData {
        FileDownloaderFactoryData() : m_factory(0) {}
        ~FileDownloaderFactoryData() { delete m_factory; }

        bool m_followRedirects;
        bool m_ignoreSslErrors;
        QStringList m_supportedSchemes;
        FileDownloaderProxyFactory *m_factory;
    };

public:
    static FileDownloaderFactory &instance();
    ~FileDownloaderFactory();

    template<typename T>
    void registerFileDownloader(const QString &scheme)
    {
        registerProduct<T>(scheme);
        d->m_supportedSchemes.append(scheme);
    }
    FileDownloader *create(const QString &scheme, QObject *parent = 0) const;

    static bool followRedirects();
    static void setFollowRedirects(bool val);

    static void setProxyFactory(FileDownloaderProxyFactory *factory);

    static bool ignoreSslErrors();
    static void setIgnoreSslErrors(bool ignore);

    static QStringList supportedSchemes();
    static bool isSupportedScheme(const QString &scheme);

private:
    FileDownloaderFactory();

private:
    FileDownloaderFactoryData *d;
};

} // namespace KDUpdater

#endif // KD_UPDATER_FILE_DOWNLOADER_FACTORY_H
