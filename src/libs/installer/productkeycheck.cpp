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

#include "productkeycheck.h"
#include "packagemanagercore.h"

class ProductKeyCheckPrivate
{
};

ProductKeyCheck::ProductKeyCheck()
    : d(new ProductKeyCheckPrivate())
{
}

ProductKeyCheck::~ProductKeyCheck()
{
    delete d;
}

ProductKeyCheck *ProductKeyCheck::instance()
{
    static ProductKeyCheck instance;
    return &instance;
}

void ProductKeyCheck::init(QInstaller::PackageManagerCore *core)
{
    Q_UNUSED(core)
}

bool ProductKeyCheck::hasValidKey()
{
    return true;
}

bool ProductKeyCheck::applyKey(const QStringList &/*arguments*/)
{
    return true;
}

QString ProductKeyCheck::lastErrorString()
{
    return QString();
}

QString ProductKeyCheck::maintainanceToolDetailErrorNotice()
{
    return QString();
}

// to filter none valid licenses
bool ProductKeyCheck::isValidLicenseTextFile(const QString &/*fileName*/)
{
    return true;
}

bool ProductKeyCheck::isValidRepository(const QInstaller::Repository &repository) const
{
    Q_UNUSED(repository)
    return true;
}

QList<int> ProductKeyCheck::registeredPages() const
{
    return QList<int>();
}
