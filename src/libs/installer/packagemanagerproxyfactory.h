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

#ifndef PACKAGEMANAGERPROXYFACTORY_H
#define PACKAGEMANAGERPROXYFACTORY_H

#include "kdupdaterfiledownloaderfactory.h"

namespace QInstaller {

class PackageManagerCore;

struct ProxyCredential {
    QString host;
    QString user;
    QString password;
    int port;
};

class PackageManagerProxyFactory : public KDUpdater::FileDownloaderProxyFactory
{
public:
    explicit PackageManagerProxyFactory(const PackageManagerCore *const core);

    PackageManagerProxyFactory *clone() const;
    QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query = QNetworkProxyQuery());

    void setProxyCredentials(const QNetworkProxy &proxy, const QString &user, const QString &password);

private:

    QList<ProxyCredential> m_proxyCredentials;
    const PackageManagerCore *const m_core;
};

}   // QInstaller

#endif // PACKAGEMANAGERPROXYFACTORY_H
