/**************************************************************************
**
** Copyright (c) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of Installer Framework
**
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
**************************************************************************/

#ifndef PRODUCTKEYCHECK_H
#define PRODUCTKEYCHECK_H

#include "installer_global.h"

#include <QString>

namespace QInstaller{
    class PackageManagerCore;
    class Repository;
}

class ProductKeyCheckPrivate;

class INSTALLER_EXPORT ProductKeyCheck
{
public:
    ~ProductKeyCheck();
    static ProductKeyCheck *instance();
    void init(QInstaller::PackageManagerCore *core);

    // was validLicense
    bool hasValidKey();
    QString lastErrorString();
    QString maintainanceToolDetailErrorNotice();

    //is used in the generic ApplyProductKeyOperation, for example to patch things
    bool applyKey(const QStringList &arguments);

    // to filter none valid licenses
    bool isValidLicenseTextFile(const QString &fileName);

    // to filter repositories not matching the license
    bool isValidRepository(const QInstaller::Repository &repository) const;

    QList<int> registeredPages() const;

private:
    ProductKeyCheck();
    ProductKeyCheckPrivate *const d;
    Q_DISABLE_COPY(ProductKeyCheck)
};

#endif // PRODUCTKEYCHECK_H
