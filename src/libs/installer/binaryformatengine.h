/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
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
**************************************************************************/

#ifndef BINARYFORMATENGINE_H
#define BINARYFORMATENGINE_H

#include "binaryformat.h"

namespace QInstallerCreator {

class BinaryFormatEngine : public QAbstractFileEngine
{
public:
    BinaryFormatEngine(const ComponentIndex &index, const QString &fileName);
    ~BinaryFormatEngine();

    void setFileName(const QString &file);

    Iterator *beginEntryList(QDir::Filters filters, const QStringList &filterNames);

    bool copy(const QString &newName);
    bool close();
    bool open(QIODevice::OpenMode mode);
    qint64 pos() const;
    qint64 read(char *data, qint64 maxlen);
    bool seek(qint64 offset);
    qint64 size() const;

    QString fileName(FileName file = DefaultName) const;
    FileFlags fileFlags(FileFlags type = FileInfoAll) const;
    QStringList entryList(QDir::Filters filters, const QStringList &filterNames) const;

protected:
    void setArchive(const QString &file);

private:
    const ComponentIndex m_index;
    bool m_hasComponent;
    bool m_hasArchive;
    Component m_component;
    QSharedPointer<Archive> m_archive;
    QString m_fileNamePath;
};

} // namespace QInstallerCreator

#endif
