/**************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#include <init.h>
#include <kdupdaterupdateoperations.h>
#include <utils.h>

#include <QDir>
#include <QObject>
#include <QTest>
#include <QFile>
#include <QDebug>

using namespace KDUpdater;
using namespace QInstaller;

class tst_copyoperationtest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        //QInstaller::init();
        m_testDestinationPath = qApp->applicationDirPath() + QDir::toNativeSeparators("/test");
        m_testDestinationFilePath = QDir(m_testDestinationPath).absoluteFilePath(QFileInfo(
            qApp->applicationFilePath()).fileName());
        if (QDir(m_testDestinationPath).exists()) {
            QFAIL("Remove test folder first!");
        }
    }

    void testMissingArguments()
    {
        CopyOperation op;

        QVERIFY(op.testOperation());
        QVERIFY(!op.performOperation());

        QCOMPARE(UpdateOperation::Error(op.error()), UpdateOperation::InvalidArguments);
        QCOMPARE(op.errorString(), QString("Invalid arguments: 0 arguments given, 2 expected."));

    }

    void testCopySomething_data()
    {
         QTest::addColumn<QString>("source");
         QTest::addColumn<QString>("destination");
         QTest::newRow("full path syntax") << qApp->applicationFilePath() << m_testDestinationFilePath;
         QTest::newRow("short destination syntax") << qApp->applicationFilePath() << m_testDestinationPath;
         QTest::newRow("short destination syntax with ending separator") << qApp->applicationFilePath()
            << m_testDestinationPath + QDir::separator();
    }

    void testCopySomething()
    {
        QFETCH(QString, source);
        QFETCH(QString, destination);

        QVERIFY2(QFileInfo(source).exists(), QString("Source '%1' does not exist.").arg(source).toLatin1());
        CopyOperation op;
        op.setArguments(QStringList() << source << destination);
        op.backup();
        QVERIFY2(op.performOperation(), op.errorString().toLatin1());

        QVERIFY2(QFileInfo(m_testDestinationFilePath).exists(), QString("Copying from '%1' to '%2' was "
            "not working: '%3' does not exist").arg(source, destination, m_testDestinationFilePath).toLatin1());
        QVERIFY2(op.undoOperation(), op.errorString().toLatin1());
        QVERIFY2(!QFileInfo(m_testDestinationFilePath).exists(), QString("Undo of copying from '%1' to "
            "'%2' was not working.").toLatin1());
    }

    void testCopyIfDestinationExist_data()
    {
        testCopySomething_data();
    }

    void testCopyIfDestinationExist()
    {
        QFETCH(QString, source);
        QFETCH(QString, destination);

        QByteArray testString("This file is generated by QTest\n");
        QFile testFile(m_testDestinationFilePath);
        testFile.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream out(&testFile);
        out << testString;
        testFile.close();

        QByteArray testFileHash = QInstaller::calculateHash(m_testDestinationFilePath, QCryptographicHash::Sha1);

        QVERIFY2(QFileInfo(source).exists(), QString("Source '%1' does not exist.").arg(source).toLatin1());
        CopyOperation op;
        op.setArguments(QStringList() << source << destination);
        op.backup();
        QVERIFY2(!op.value("backupOfExistingDestination").toString().isEmpty(), "The CopyOperation didn't saved any backup.");
        QVERIFY2(op.performOperation(), op.errorString().toLatin1());

        // checking that perform did something
        QByteArray currentFileHash = QInstaller::calculateHash(m_testDestinationFilePath, QCryptographicHash::Sha1);
        QVERIFY(testFileHash != currentFileHash);

        QVERIFY2(QFileInfo(m_testDestinationFilePath).exists(), QString("Copying from '%1' to '%2' was "
            "not working: '%3' does not exist").arg(source, destination, m_testDestinationFilePath).toLatin1());

        // undo should replace the new one with the old backuped one
        QVERIFY2(op.undoOperation(), op.errorString().toLatin1());
        currentFileHash = QInstaller::calculateHash(m_testDestinationFilePath, QCryptographicHash::Sha1);
        QVERIFY(testFileHash == currentFileHash);
    }
    void init()
    {
        QVERIFY2(!QFileInfo(m_testDestinationFilePath).exists(), QString("Destination '%1' should not exist "
            "to test the copy operation.").arg(m_testDestinationFilePath).toLatin1());
        QDir().mkpath(m_testDestinationPath);
    }

    void cleanup()
    {
        QFile(m_testDestinationFilePath).remove();
        QDir().rmpath(m_testDestinationPath);
    }
private:
    QString m_testDestinationPath;
    QString m_testDestinationFilePath;
};

QTEST_MAIN(tst_copyoperationtest)

#include "tst_copyoperationtest.moc"
