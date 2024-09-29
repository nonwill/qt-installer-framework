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

#ifndef LIBINSTALLER_ENVIRONMENT_H
#define LIBINSTALLER_ENVIRONMENT_H

#include "kdtoolsglobal.h"

#include <QString>
#include <QHash>

QT_BEGIN_NAMESPACE
class QProcess;
class QProcessEnvironment;
QT_END_NAMESPACE

namespace KDUpdater {

class KDTOOLS_EXPORT Environment
{
public:
    static Environment &instance();

    ~Environment() {}

    QString value(const QString &key, const QString &defaultValue = QString()) const;
    void setTemporaryValue(const QString &key, const QString &value);

    QProcessEnvironment applyTo(const QProcessEnvironment &qpe) const;
    void applyTo(QProcess *process);

private:
    Environment() {}

private:
    Q_DISABLE_COPY(Environment)
    QHash<QString, QString> m_tempValues;
};

} // namespace KDUpdater

#endif
