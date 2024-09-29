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

#include <binaryformat.h>
#include <component.h>
#include <errors.h>
#include <fileutils.h>
#include <packagemanagercore.h>
#include <progresscoordinator.h>

#include <QDir>
#include <QTemporaryFile>
#include <QTest>

using namespace QInstaller;

class DummyComponent : public Component
{
public:
    DummyComponent(PackageManagerCore *core)
        : Component(core)
    {
        setCheckState(Qt::Checked);
    }

    void beginInstallation()
    {
        throw Error(tr("Force crash to test rollback!"));
    }

    ~DummyComponent()
    {
    }
};

class tst_PackageManagerCore : public QObject
{
    Q_OBJECT

private:
    void setIgnoreMessage(const QString &testDirectory)
    {
        const QString message = "\"\t- arguments: %1\" ";
        QTest::ignoreMessage(QtDebugMsg, "\"backup  operation: Mkdir\" ");
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(testDirectory)));
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(testDirectory)));
        QTest::ignoreMessage(QtDebugMsg, qPrintable(message.arg(testDirectory)));
        QTest::ignoreMessage(QtDebugMsg, "\"perform  operation: Mkdir\" ");
        QTest::ignoreMessage(QtDebugMsg, "Install size: 1 components ");
        QTest::ignoreMessage(QtDebugMsg, "create Error-Exception: \"Force crash to test rollback!\" ");
        QTest::ignoreMessage(QtDebugMsg, "\"created critical message box installationError: 'Error"
            "', Force crash to test rollback!\" ");
        QTest::ignoreMessage(QtDebugMsg, "ROLLING BACK operations= 1 ");
        QTest::ignoreMessage(QtDebugMsg, "\"undo  operation: Mkdir\" ");
        QTest::ignoreMessage(QtDebugMsg, "Done ");
        QTest::ignoreMessage(QtDebugMsg, "Done ");
        QTest::ignoreMessage(QtDebugMsg, "Done ");
    }

private slots:
    void testRollBackInstallationKeepTarget()
    {

        const QString testDirectory = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(testDirectory));

        setIgnoreMessage(testDirectory);

        PackageManagerCore core(MagicInstallerMarker);
        // cancel the installer in error case
        core.autoRejectMessageBoxes();
        core.appendRootComponent(new DummyComponent(&core));
        core.setValue(QLatin1String("TargetDir"), testDirectory);
        core.setValue(QLatin1String("RemoveTargetDir"), QLatin1String("true"));

        QVERIFY(core.calculateComponentsToInstall());
        {
            QTemporaryFile dummy(testDirectory + QLatin1String("/dummy"));
            dummy.open();

            core.runInstaller();

            QVERIFY(QDir(testDirectory).exists());
            QVERIFY(QFileInfo(dummy.fileName()).exists());
        }
        QDir().rmdir(testDirectory);
        ProgressCoordinator::instance()->reset();
    }

    void testRollBackInstallationRemoveTarget()
    {
        const QString testDirectory = QInstaller::generateTemporaryFileName();
        QVERIFY(QDir().mkpath(testDirectory));

        setIgnoreMessage(testDirectory);

        PackageManagerCore core(MagicInstallerMarker);
        // cancel the installer in error case
        core.autoRejectMessageBoxes();
        core.appendRootComponent(new DummyComponent(&core));
        core.setValue(QLatin1String("TargetDir"), testDirectory);
        core.setValue(QLatin1String("RemoveTargetDir"), QLatin1String("true"));

        QVERIFY(core.calculateComponentsToInstall());

        core.runInstaller();
        QVERIFY(!QDir(testDirectory).exists());
        ProgressCoordinator::instance()->reset();
    }

    void testRequiredDiskSpace()
    {
        // test installer
        PackageManagerCore core(MagicInstallerMarker);

        DummyComponent *root = new DummyComponent(&core);
        root->setValue(scName, "root");
        root->setValue(scUncompressedSize, QString::number(1000));
        core.appendRootComponent(root);

        DummyComponent *child1 = new DummyComponent(&core);
        child1->setValue(scName, "root.child1");
        child1->setValue(scUncompressedSize, QString::number(1500));
        root->appendComponent(child1);

        DummyComponent *child2 = new DummyComponent(&core);
        child2->setValue(scName, "root.child2");
        child2->setValue(scUncompressedSize, QString::number(250));
        root->appendComponent(child2);

        // install root, child1 (child2 remains uninstalled)
        root->setUninstalled();
        child1->setUninstalled();
        child2->setInstalled();
        core.calculateComponentsToInstall();
        QCOMPARE(core.requiredDiskSpace(), 2500ULL);

        // additionally install child2
        root->setInstalled();
        child1->setInstalled();
        child2->setUninstalled();
        core.componentsToInstallNeedsRecalculation();
        core.calculateComponentsToInstall();
        QCOMPARE(core.requiredDiskSpace(), 250ULL);
    }
};


QTEST_MAIN(tst_PackageManagerCore)

#include "tst_packagemanagercore.moc"
