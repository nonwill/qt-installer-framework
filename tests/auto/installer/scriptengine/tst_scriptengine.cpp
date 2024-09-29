#include <component.h>
#include <errors.h>
#include <packagemanagercore.h>
#include <packagemanagergui.h>
#include <scriptengine.h>

#include <QTest>
#include <QSet>
#include <QFile>
#include <QString>

using namespace QInstaller;

class TestGui : public QInstaller::PackageManagerGui
{
    Q_OBJECT

public:
    explicit TestGui(QInstaller::PackageManagerCore *core)
    : PackageManagerGui(core, 0)
    {
        setPage(PackageManagerCore::Introduction, new IntroductionPage(core));
        setPage(PackageManagerCore::ComponentSelection, new ComponentSelectionPage(core));
        setPage(PackageManagerCore::InstallationFinished, new FinishedPage(core));
    }

    virtual void init() {}

    void callProtectedDelayedExecuteControlScript(int id)
    {
        executeControlScript(id);
    }
};

class EmitSignalObject : public QObject
{
    Q_OBJECT

public:
    EmitSignalObject() {}
    ~EmitSignalObject() {}
    void produceSignal() { emit emitted(); }
signals:
    void emitted();
};


class tst_ScriptEngine : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_component = new Component(&m_core);
        // append the component to the package manager which deletes it at destructor
        // (it calls clearAllComponentLists which calls qDeleteAll(m_rootComponents);)
        m_core.appendRootComponent(m_component);

        m_component->setValue("AutoDependOn", "Script");
        m_component->setValue("Default", "Script");
        m_component->setValue(scName, "component.test.name");

        m_scriptEngine = m_core.componentScriptEngine();
    }

    void testBrokenJSMethodConnect()
    {
        EmitSignalObject emiter;
        m_scriptEngine->globalObject().setProperty(QLatin1String("emiter"),
            m_scriptEngine->newQObject(&emiter));

        QScriptValue context = m_scriptEngine->loadInConext(QLatin1String("BrokenConnect"),
            ":///data/broken_connect.qs");

        QVERIFY(context.isValid());

        if (m_scriptEngine->hasUncaughtException()) {
            QFAIL(qPrintable(QString::fromLatin1("ScriptEngine hasUncaughtException:\n %1").arg(
                uncaughtExceptionString(m_scriptEngine))));
        }

        const QString debugMesssage(
            "create Error-Exception: \"Fatal error while evaluating a script.\n\n"
            "ReferenceError: Can't find variable: foo\n\n"
            "Backtrace:\n"
#if QT_VERSION < 0x050000
                "\t<anonymous>()@:///data/broken_connect.qs:10\" ");
#else
            "\treceive() at :///data/broken_connect.qs:10\n"
            "\t<global>() at -1\" ");
#endif
        try {
            // ignore Output from script
            setExpectedScriptOutput("function receive()");
            setExpectedScriptOutput(qPrintable(debugMesssage));
            emiter.produceSignal();
        } catch (const Error &error) {
            QVERIFY2(debugMesssage.contains(error.message()), "There was some unexpected error.");
        }
    }

    void testScriptPrint()
    {
        setExpectedScriptOutput("test");
        m_scriptEngine->evaluate("print(\"test\");");
        if (m_scriptEngine->hasUncaughtException()) {
            QFAIL(qPrintable(QString::fromLatin1("ScriptEngine hasUncaughtException:\n %1").arg(
                uncaughtExceptionString(m_scriptEngine))));
        }
    }

    void testExistingInstallerObject()
    {
        setExpectedScriptOutput("object");
        m_scriptEngine->evaluate("print(typeof installer)");
        if (m_scriptEngine->hasUncaughtException()) {
            QFAIL(qPrintable(QString::fromLatin1("ScriptEngine hasUncaughtException:\n %1").arg(
                uncaughtExceptionString(m_scriptEngine))));
        }
    }

    void testComponentByName()
    {
        const QString printComponentNameScript = QString::fromLatin1("var correctComponent = "
            "installer.componentByName('%1');\nprint(correctComponent.name);").arg(m_component->name());

        setExpectedScriptOutput("component.test.name");
        m_scriptEngine->evaluate(printComponentNameScript);
        if (m_scriptEngine->hasUncaughtException()) {
            QFAIL(qPrintable(QString::fromLatin1("ScriptEngine hasUncaughtException:\n %1").arg(
                uncaughtExceptionString(m_scriptEngine))));
        }
    }

    void testComponentByWrongName()
    {
        const QString printComponentNameScript = QString::fromLatin1( "var brokenComponent = "
            "installer.componentByName('%1');\nprint(brokenComponent.name);").arg("MyNotExistingComponentName");

        m_scriptEngine->evaluate(printComponentNameScript);
        QVERIFY(m_scriptEngine->hasUncaughtException());
    }

    void loadSimpleComponentScript()
    {
       try {
            // ignore retranslateUi which is called by loadComponentScript
            setExpectedScriptOutput("Component constructor - OK");
            setExpectedScriptOutput("retranslateUi - OK");
            m_component->loadComponentScript(":///data/component1.qs");

            setExpectedScriptOutput("retranslateUi - OK");
            m_component->languageChanged();

            setExpectedScriptOutput("createOperationsForPath - OK");
            m_component->createOperationsForPath(":///data/");

            setExpectedScriptOutput("createOperationsForArchive - OK");
            // ignore createOperationsForPath which is called by createOperationsForArchive
            setExpectedScriptOutput("createOperationsForPath - OK");
            m_component->createOperationsForArchive("test.7z");

            setExpectedScriptOutput("beginInstallation - OK");
            m_component->beginInstallation();

            setExpectedScriptOutput("createOperations - OK");
            m_component->createOperations();

            setExpectedScriptOutput("isAutoDependOn - OK");
            bool returnIsAutoDependOn = m_component->isAutoDependOn(QSet<QString>());
            QCOMPARE(returnIsAutoDependOn, false);

            setExpectedScriptOutput("isDefault - OK");
            bool returnIsDefault = m_component->isDefault();
            QCOMPARE(returnIsDefault, false);

        } catch (const Error &error) {
            QFAIL(qPrintable(error.message()));
        }
    }

    void loadBrokenComponentScript()
    {
        Component *testComponent = new Component(&m_core);
        testComponent->setValue(scName, "broken.component");

        // now m_core becomes the owner of testComponent
        // so it will delete it then at the destuctor
        m_core.appendRootComponent(testComponent);

        const QString debugMesssage(
            "create Error-Exception: \"Exception while loading the component script: ':///data/component2.qs\n\n"
            "ReferenceError: Can't find variable: broken\n\n"
            "Backtrace:\n"
#if QT_VERSION < 0x050000
                "\t<anonymous>()@:///data/component2.qs:5'\" ");
#else
            "\tComponent() at :///data/component2.qs:5\n"
            "\t<anonymous>() at :///data/component2.qs:7\n"
            "\t<global>() at :///data/component2.qs:7'\" ");
#endif
        try {
            // ignore Output from script
            setExpectedScriptOutput("script function: Component");
            setExpectedScriptOutput(qPrintable(debugMesssage));
            testComponent->loadComponentScript(":///data/component2.qs");
        } catch (const Error &error) {
            QVERIFY2(debugMesssage.contains(error.message()), "There was some unexpected error.");
        }
    }

    void loadSimpleAutoRunScript()
    {
        try {
            TestGui testGui(&m_core);
            setExpectedScriptOutput("Loaded control script \":///data/auto-install.qs\" ");
            testGui.loadControlScript(":///data/auto-install.qs");
            QCOMPARE(m_core.value("GuiTestValue"), QString("hello"));

            // show event calls automatically the first callback which does not exist
            setExpectedScriptOutput("Control script callback \"IntroductionPageCallback\" does not exist. ");
            testGui.show();

            // inside the auto-install script we are clicking the next button, with a not existing button
            QTest::ignoreMessage(QtWarningMsg, "Button with type:  \"unknown button\" not found! ");
            testGui.callProtectedDelayedExecuteControlScript(PackageManagerCore::ComponentSelection);

            setExpectedScriptOutput("FinishedPageCallback - OK");
            testGui.callProtectedDelayedExecuteControlScript(PackageManagerCore::InstallationFinished);
        } catch (const Error &error) {
            QFAIL(qPrintable(error.message()));
        }
    }

private:
    void setExpectedScriptOutput(const char *message)
    {
        // Using setExpectedScriptOutput(...); inside the test method
        // as a simple test that the scripts are called.
        QTest::ignoreMessage(QtDebugMsg, message);
    }

    PackageManagerCore m_core;
    Component *m_component;
    ScriptEngine *m_scriptEngine;

};


QTEST_MAIN(tst_ScriptEngine)

#include "tst_scriptengine.moc"
