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

#include "linereplaceoperation.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTextStream>

using namespace QInstaller;

LineReplaceOperation::LineReplaceOperation()
{
    setName(QLatin1String("LineReplace"));
}

void LineReplaceOperation::backup()
{
}

bool LineReplaceOperation::performOperation()
{
    const QStringList args = arguments();

    // Arguments:
    // 1. filename
    // 2. startsWith Search-String
    // 3. Replace-Line-String
    if (args.count() != 3) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, %2 expected%3.")
            .arg(name()).arg(arguments().count()).arg(tr("exactly 3"), QLatin1String("")));
        return false;
    }
    const QString fileName = args.at(0);
    const QString searchString = args.at(1);
    const QString replaceString = args.at(2);

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(UserDefinedError);
        setErrorString(tr("Failed to open '%1' for reading.").arg(fileName));
        return false;
    }

    QString replacement;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.trimmed().startsWith(searchString))
            replacement.append(replaceString + QLatin1String("\n"));
        else
            replacement.append(line + QLatin1String("\n"));
    }
    file.close();

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(UserDefinedError);
        setErrorString(tr("Failed to open '%1' for writing.").arg(fileName));
        return false;
    }

    stream.setDevice(&file);
    stream << replacement;
    file.close();

    return true;
}

bool LineReplaceOperation::undoOperation()
{
    return true;
}

bool LineReplaceOperation::testOperation()
{
    return true;
}

Operation *LineReplaceOperation::clone() const
{
    return new LineReplaceOperation();
}
