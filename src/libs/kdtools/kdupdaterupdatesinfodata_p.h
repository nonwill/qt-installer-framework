/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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

#ifndef KD_UPDATER_UPDATE_INFO_DATA_H
#define KD_UPDATER_UPDATE_INFO_DATA_H

#include <QCoreApplication>
#include <QDomElement>
#include <QSharedData>

namespace KDUpdater {

struct UpdateInfo;

struct UpdatesInfoData : public QSharedData
{
    Q_DECLARE_TR_FUNCTIONS(KDUpdater::UpdatesInfoData)

public:
    UpdatesInfoData();
    ~UpdatesInfoData();

    int error;
    QString errorMessage;
    QString updateXmlFile;
    QString applicationName;
    QString applicationVersion;
    QList<UpdateInfo> updateInfoList;

    void parseFile(const QString &updateXmlFile);
    bool parsePackageUpdateElement(const QDomElement &updateE);

    void setInvalidContentError(const QString &detail);
};

} // namespace KDUpdater

#endif // KD_UPDATER_UPDATE_INFO_DATA_H
