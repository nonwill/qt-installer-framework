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

#ifndef KD_UPDATER_UPDATE_FINDER_H
#define KD_UPDATER_UPDATE_FINDER_H

#include "kdupdatertask.h"

#include <QHash>
#include <QUrl>

namespace KDUpdater {

class Application;
class Update;

class KDTOOLS_EXPORT UpdateFinder : public Task
{
    Q_OBJECT
    class Private;

public:
    explicit UpdateFinder(Application *application);
    ~UpdateFinder();

    QList<Update *> updates() const;

private:
    void doRun();
    bool doStop();
    bool doPause();
    bool doResume();

    Update *constructUpdate(int priority, const QUrl &sourceInfoUrl, const QHash<QString, QVariant> &data,
        quint64 compressedSize, quint64 uncompressedSize) const;

private:
    Private *d;
    Q_PRIVATE_SLOT(d, void slotDownloadDone())
};

} // namespace KDUpdater

#endif // KD_UPDATER_UPDATE_FINDER_H
