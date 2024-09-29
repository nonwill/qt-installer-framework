/**************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#ifndef ABSTRACTFILETASK_H
#define ABSTRACTFILETASK_H

#include "abstracttask.h"
#include "installer_global.h"

#include <QObject>
#include <QReadWriteLock>

#if QT_VERSION < 0x050000
#include <qtconcurrentexception.h>
#define QException QtConcurrent::Exception
#define QUnhandledException QtConcurrent::UnhandledException
#endif

namespace QInstaller {

namespace TaskRole {
enum
{
    Checksum,
    TaskItem,
    SourceFile,
    TargetFile,
    UserRole = 1000
};
}

class FileTaskItem : public AbstractTaskData
{
public:
    FileTaskItem() {}
    explicit FileTaskItem(const QString &s)
    {
        insert(TaskRole::SourceFile, s);
    }
    FileTaskItem(const QString &s, const QString &t)
    {
        insert(TaskRole::SourceFile, s);
        insert(TaskRole::TargetFile, t);
    }

    QString source() const { return value(TaskRole::SourceFile).toString(); }
    QString target() const { return value(TaskRole::TargetFile).toString(); }
};

class FileTaskResult : public AbstractTaskData
{
public:
    FileTaskResult() {}
    FileTaskResult(const QString &t, const QByteArray &c, const FileTaskItem &i)
    {
        insert(TaskRole::Checksum, c);
        insert(TaskRole::TargetFile, t);
        insert(TaskRole::TaskItem, QVariant::fromValue(i));
    }

    QString target() const { return value(TaskRole::TargetFile).toString(); }
    QByteArray checkSum() const { return value(TaskRole::Checksum).toByteArray(); }
    FileTaskItem taskItem() const { return value(TaskRole::TaskItem).value<FileTaskItem>(); }
};

class FileTaskException : public QException
{
public:
    FileTaskException() {}
    ~FileTaskException() throw() {}
    explicit FileTaskException(const QString &message)
        : m_message(message) {}

    void raise() const { throw *this; }
    QString message() const { return m_message; }
    FileTaskException *clone() const { return new FileTaskException(*this); }

private:
    QString m_message;
};

class INSTALLER_EXPORT AbstractFileTask : public AbstractTask<FileTaskResult>
{
    Q_OBJECT
    Q_DISABLE_COPY(AbstractFileTask)

public:
    AbstractFileTask();
    virtual ~AbstractFileTask() {}

    explicit AbstractFileTask(const QString &source);
    explicit AbstractFileTask(const FileTaskItem &item);
    AbstractFileTask(const QString &source, const QString &target);

    QList<FileTaskItem> taskItems() const;
    void setTaskItem(const FileTaskItem &item);

protected:
    void clearTaskItems();
    void addTaskItem(const FileTaskItem &item);
    void setTaskItems(const QList<FileTaskItem> &items);
    void addTaskItems(const QList<FileTaskItem> &items);

private:
    void registerMetaTypes();

private:
    QList<FileTaskItem> m_items;
    mutable QReadWriteLock m_lock;
};

}   // namespace QInstaller

Q_DECLARE_METATYPE(QInstaller::FileTaskItem)
Q_DECLARE_METATYPE(QInstaller::FileTaskResult)
Q_DECLARE_METATYPE(QInstaller::FileTaskException)

#endif // ABSTRACTFILETASK_H
