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

#include "registerfiletypeoperation.h"

#include "packagemanagercore.h"
#include "qsettingswrapper.h"

using namespace QInstaller;

#ifdef Q_OS_WIN
#include <shlobj.h>


struct StartsWithProgId
{
    bool operator()(const QString &s)
    {
        return s.startsWith(QLatin1String("ProgId="));
    }
};

static QString takeProgIdArgument(QStringList &args)
{
    // if the arguments contain an option in the form "ProgId=...", find it and consume it
    QStringList::iterator it = std::find_if (args.begin(), args.end(), StartsWithProgId());
    if (it == args.end())
        return QString();

    const QString progId = it->mid(QString::fromLatin1("ProgId=").size());
    args.erase(it);
    return progId;
}

static QVariantHash readHive(QSettingsWrapper *const settings, const QString &hive)
{
    QVariantHash keyValues;

    settings->beginGroup(hive);
    foreach (const QString &key, settings->allKeys())
        keyValues.insert(key, settings->value(key));
    settings->endGroup();

    return keyValues;
}
#endif


// -- RegisterFileTypeOperation

RegisterFileTypeOperation::RegisterFileTypeOperation()
{
    setName(QLatin1String("RegisterFileType"));
}

void RegisterFileTypeOperation::backup()
{
}

bool RegisterFileTypeOperation::performOperation()
{
#ifdef Q_OS_WIN
    QStringList args = arguments();
    QString progId = takeProgIdArgument(args);

    if (args.count() < 2 || args.count() > 5) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, %2 expected%3.")
            .arg(name()).arg(arguments().count()).arg(tr("2 to 5"), QLatin1String("")));
        return false;
    }

    bool allUsers = false;
    PackageManagerCore *const core = value(QLatin1String("installer")).value<PackageManagerCore*>();
    if (core && core->value(QLatin1String("AllUsers")) == QLatin1String("true"))
        allUsers = true;

    QSettingsWrapper settings(QLatin1String(allUsers ? "HKEY_LOCAL_MACHINE" : "HKEY_CURRENT_USER")
        , QSettingsWrapper::NativeFormat);

    const QString extension = args.at(0);
    if (progId.isEmpty())
        progId = QString::fromLatin1("%1_auto_file").arg(extension);
    const QString classesProgId = QString::fromLatin1("Software/Classes/") + progId;
    const QString classesFileType = QString::fromLatin1("Software/Classes/.%2").arg(extension);
    const QString classesApplications = QString::fromLatin1("Software/Classes/Applications/") + progId;

    // backup old value
    setValue(QLatin1String("oldType"), readHive(&settings, classesFileType));

    // register new values
    settings.setValue(QString::fromLatin1("%1/Default").arg(classesFileType), progId);
    settings.setValue(QString::fromLatin1("%1/OpenWithProgIds/%2").arg(classesFileType, progId), QString());
    settings.setValue(QString::fromLatin1("%1/shell/Open/Command/Default").arg(classesProgId), args.at(1));
    settings.setValue(QString::fromLatin1("%1/shell/Open/Command/Default").arg(classesApplications), args.at(1));

    // content type (optional)
    const QString contentType = args.value(3);
    if (!contentType.isEmpty())
        settings.setValue(QString::fromLatin1("%1/Content Type").arg(classesFileType), contentType);

    // description (optional)
    const QString description = args.value(2);
    if (!description.isEmpty())
        settings.setValue(QString::fromLatin1("%1/Default").arg(classesProgId), description);

     // icon (optional)
    const QString icon = args.value(4);
    if (!icon.isEmpty())
        settings.setValue(QString::fromLatin1("%1/DefaultIcon/Default").arg(classesProgId), icon);

    // backup new value
    setValue(QLatin1String("newType"), readHive(&settings, classesFileType));

    // force the shell to invalidate its cache
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

    return true;
#else
    setError(UserDefinedError);
    setErrorString(tr("Registering file types is only supported on Windows."));
    return false;
#endif
}

bool RegisterFileTypeOperation::undoOperation()
{
#ifdef Q_OS_WIN
    QStringList args = arguments();
    QString progId = takeProgIdArgument(args);

    if (args.count() < 2 || args.count() > 5) {
        setErrorString(tr("Register File Type: Invalid arguments"));
        return false;
    }

    bool allUsers = false;
    PackageManagerCore *const core = value(QLatin1String("installer")).value<PackageManagerCore*>();
    if (core && core->value(QLatin1String("AllUsers")) == QLatin1String("true"))
        allUsers = true;

    QSettingsWrapper settings(QLatin1String(allUsers ? "HKEY_LOCAL_MACHINE" : "HKEY_CURRENT_USER")
        , QSettingsWrapper::NativeFormat);

    const QString extension = args.at(0);
    if (progId.isEmpty())
        progId = QString::fromLatin1("%1_auto_file").arg(extension);
    const QString classesProgId = QString::fromLatin1("Software/Classes/") + progId;
    const QString classesFileType = QString::fromLatin1("Software/Classes/.%2").arg(extension);
    const QString classesApplications = QString::fromLatin1("Software/Classes/Applications/") + progId;

    // Quoting MSDN here: When uninstalling an application, the ProgIDs and most other registry information
    // associated with that application should be deleted as part of the uninstallation.However, applications
    // that have taken ownership of a file type (by setting the Default value of the file type's
    // HKEY...\.extension subkey to the ProgID of the application) should not attempt to remove that value
    // when uninstalling. Leaving the data in place for the Default value avoids the difficulty of
    // determining whether another application has taken ownership of the file type and overwritten the
    // Default value after the original application was installed. Windows respects the Default value only
    // if the ProgID found there is a registered ProgID. If the ProgID is unregistered, it is ignored.

    // if the hive didn't change since we touched it
    if (value(QLatin1String("newType")).toHash() == readHive(&settings, classesFileType)) {
        // reset to the values we found
        settings.remove(classesFileType);
        settings.beginGroup(classesFileType);
        const QVariantHash keyValues = value(QLatin1String("oldType")).toHash();
        foreach (const QString &key, keyValues.keys())
            settings.setValue(key, keyValues.value(key));
        settings.endGroup();
    } else {
        // some changes happened, remove the only save value we know about
        settings.remove(QString::fromLatin1("%1/OpenWithProgIds/%2").arg(classesFileType, progId));
    }

    // remove ProgId and Applications entry
    settings.remove(classesProgId);
    settings.remove(classesApplications);

    // force the shell to invalidate its cache
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

    return true;
#else
    setErrorString(tr("Registering file types is only supported on Windows."));
    return false;
#endif
}

bool RegisterFileTypeOperation::testOperation()
{
    return true;
}

Operation *RegisterFileTypeOperation::clone() const
{
    return new RegisterFileTypeOperation();
}
