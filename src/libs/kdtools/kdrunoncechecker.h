/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#ifndef KDTOOLS_RUNONCECHECKER_H
#define KDTOOLS_RUNONCECHECKER_H

#include "kdlockfile.h"

#include <QString>

class KDTOOLS_EXPORT KDRunOnceChecker
{
    Q_DISABLE_COPY(KDRunOnceChecker)

public:
    enum ConditionFlag {
        Lockfile = 0x01,
        ProcessList = 0x02
    };
    Q_DECLARE_FLAGS(ConditionFlags, ConditionFlag)

    explicit KDRunOnceChecker(const QString &filename = QString());
    ~KDRunOnceChecker();

    bool isRunning(KDRunOnceChecker::ConditionFlags flags);

private:
    KDLockFile m_lockfile;
};

#endif // KDTOOLS_RUNONCECHECKER_H
