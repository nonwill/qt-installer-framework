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

#include "kdlockfile.h"
#include "kdlockfile_p.h"
#include <kdupdater.h>


#include <QCoreApplication>
#include <QFileInfo>

#ifdef Q_OS_WIN
//copied from qsystemerror.cpp in Qt
QString KDUpdater::windowsErrorString(int errorCode)
{
    QString ret;
    wchar_t *string = 0;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR) &string,
        0,
        NULL);
    ret = QString::fromWCharArray(string);
    LocalFree((HLOCAL) string);

    if (ret.isEmpty() && errorCode == ERROR_MOD_NOT_FOUND)
        ret = QCoreApplication::translate("QInstaller", "The specified module could not be found.");

    ret.append(QLatin1String(" (0x"));
    ret.append(QString::number(uint(errorCode), 16).rightJustified(8, QLatin1Char('0')));
    ret.append(QLatin1String(")"));

    return ret;
}

#endif

bool KDLockFile::Private::lock()
{
    if (locked)
        return locked;

    errorString.clear();
    handle = CreateFile(filename.toStdWString().data(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ, NULL, QFileInfo(filename).exists() ? OPEN_EXISTING : CREATE_NEW,
        FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);

    if (handle == INVALID_HANDLE_VALUE) {
        errorString = QCoreApplication::translate("KDLockFile", "Could not create lock file '%1': "
            "%2").arg(filename, KDUpdater::windowsErrorString(GetLastError()));
        return false;
    }

    DWORD bytesWritten;
    const QByteArray pid = QString::number(QCoreApplication::applicationPid()).toLatin1();
    if (!WriteFile(handle, pid.data(), pid.size(), &bytesWritten, NULL)) {
        errorString = QCoreApplication::translate("KDLockFile", "Could not write PID to lock file "
            "'%1': %2").arg(filename, KDUpdater::windowsErrorString(GetLastError()));
        return false;
    }
    FlushFileBuffers(handle);

    if (!LockFile(handle, 0, 0, QFileInfo(filename).size(), 0)) {
        errorString = QCoreApplication::translate("KDLockFile", "Could not obtain the lock for "
            "file '%1': %2").arg(filename, KDUpdater::windowsErrorString(GetLastError()));
    } else {
        locked = true;
    }
    return locked;
}

bool KDLockFile::Private::unlock()
{
    errorString.clear();
    if (!locked)
        return true;

    if (!UnlockFile(handle, 0, 0, QFileInfo(filename).size(), 0)) {
        errorString = QCoreApplication::translate("KDLockFile", "Could not release the lock for "
            "file '%1': %2").arg(filename, KDUpdater::windowsErrorString(GetLastError()));
    } else {
        locked = false;
        CloseHandle(handle);
    }
    return !locked;
}
