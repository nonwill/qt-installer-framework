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
#include "packagemanagercore_p.h"

#include "adminauthorization.h"
#include "binaryformat.h"
#include "component.h"
#include "scriptengine.h"
#include "componentmodel.h"
#include "errors.h"
#include "fileutils.h"
#include "fsengineclient.h"
#include "globals.h"
#include "graph.h"
#include "messageboxhandler.h"
#include "packagemanagercore.h"
#include "progresscoordinator.h"
#include "qprocesswrapper.h"
#include "qsettingswrapper.h"

#include "kdsavefile.h"
#include "kdselfrestarter.h"
#include "kdupdaterfiledownloaderfactory.h"
#include "kdupdaterupdatesourcesinfo.h"
#include "kdupdaterupdateoperationfactory.h"

#include <productkeycheck.h>

#include <QtConcurrentRun>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFuture>
#include <QtCore/QFutureWatcher>
#include <QtCore/QTemporaryFile>

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <errno.h>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace QInstaller {

class OperationTracer
{
public:
    OperationTracer(Operation *operation) : m_operation(0)
    {
        // don't create output for that hacky pseudo operation
        if (operation->name() != QLatin1String("MinimumProgress"))
            m_operation = operation;
    }
    void trace(const QString &state)
    {
        if (!m_operation)
            return;
        qDebug() << QString::fromLatin1("%1 %2 operation: %3").arg(state, m_operation->value(
            QLatin1String("component")).toString(), m_operation->name());
        qDebug() << QString::fromLatin1("\t- arguments: %1").arg(m_operation->arguments()
            .join(QLatin1String(", ")));
    }
    ~OperationTracer() {
        if (!m_operation)
            return;
        qDebug() << "Done";
    }
private:
    Operation *m_operation;
};

static bool runOperation(Operation *operation, PackageManagerCorePrivate::OperationType type)
{
    OperationTracer tracer(operation);
    switch (type) {
        case PackageManagerCorePrivate::Backup:
            tracer.trace(QLatin1String("backup"));
            operation->backup();
            return true;
        case PackageManagerCorePrivate::Perform:
            tracer.trace(QLatin1String("perform"));
            return operation->performOperation();
        case PackageManagerCorePrivate::Undo:
            tracer.trace(QLatin1String("undo"));
            return operation->undoOperation();
        default:
            Q_ASSERT(!"unexpected operation type");
    }
    return false;
}

/*!
    \internal
    Creates and initializes a FSEngineClientHandler -> makes us get admin rights for QFile operations
*/
static FSEngineClientHandler *sClientHandlerInstance = 0;
static FSEngineClientHandler *initFSEngineClientHandler()
{
    if (sClientHandlerInstance == 0) {
        sClientHandlerInstance = &FSEngineClientHandler::instance();

        // Initialize the created FSEngineClientHandler instance.
        const int port = 30000 + qrand() % 1000;
        sClientHandlerInstance->init(port);
        sClientHandlerInstance->setStartServerCommand(QCoreApplication::applicationFilePath(),
            QStringList() << QLatin1String("--startserver") << QString::number(port)
            << sClientHandlerInstance->authorizationKey(), true);
    }
    return sClientHandlerInstance;
}

static QStringList checkRunningProcessesFromList(const QStringList &processList)
{
    const QList<ProcessInfo> allProcesses = runningProcesses();
    QStringList stillRunningProcesses;
    foreach (const QString &process, processList) {
        if (!process.isEmpty() && PackageManagerCorePrivate::isProcessRunning(process, allProcesses))
            stillRunningProcesses.append(process);
    }
    return stillRunningProcesses;
}

static void deferredRename(const QString &oldName, const QString &newName, bool restart = false)
{
#ifdef Q_OS_WIN
    QStringList arguments;
    {
        QTemporaryFile f(QDir::temp().absoluteFilePath(QLatin1String("deferredrenameXXXXXX.vbs")));
        openForWrite(&f, f.fileName());
        f.setAutoRemove(false);

        arguments << QDir::toNativeSeparators(f.fileName()) << QDir::toNativeSeparators(oldName)
            << QDir::toNativeSeparators(QFileInfo(oldName).dir().absoluteFilePath(QFileInfo(newName)
            .fileName()));

        QTextStream batch(&f);
        batch << "Set fso = WScript.CreateObject(\"Scripting.FileSystemObject\")\n";
        batch << "Set tmp = WScript.CreateObject(\"WScript.Shell\")\n";
        batch << QString::fromLatin1("file = \"%1\"\n").arg(arguments[2]);
        batch << "on error resume next\n";

        batch << "while fso.FileExists(file)\n";
        batch << "    fso.DeleteFile(file)\n";
        batch << "    WScript.Sleep(1000)\n";
        batch << "wend\n";
        batch << QString::fromLatin1("fso.MoveFile \"%1\", file\n").arg(arguments[1]);
        if (restart)
            batch <<  QString::fromLatin1("tmp.exec \"%1 --updater\"\n").arg(arguments[2]);
        batch << "fso.DeleteFile(WScript.ScriptFullName)\n";
    }

    QProcessWrapper::startDetached(QLatin1String("cscript"), QStringList() << QLatin1String("//Nologo")
        << arguments[0]);
#else
        QFile::remove(newName);
        QFile::rename(oldName, newName);
        KDSelfRestarter::setRestartOnQuit(restart);
#endif
}


// -- PackageManagerCorePrivate

PackageManagerCorePrivate::PackageManagerCorePrivate(PackageManagerCore *core)
    : m_updateFinder(0)
    , m_updaterApplication(new DummyConfigurationInterface)
    , m_FSEngineClientHandler(0)
    , m_core(core)
    , m_updates(false)
    , m_repoFetched(false)
    , m_updateSourcesAdded(false)
    , m_componentsToInstallCalculated(false)
    , m_componentScriptEngine(0)
    , m_controlScriptEngine(0)
    , m_proxyFactory(0)
    , m_defaultModel(0)
    , m_updaterModel(0)
    , m_guiObject(0)
{
}

PackageManagerCorePrivate::PackageManagerCorePrivate(PackageManagerCore *core, qint64 magicInstallerMaker,
        const OperationList &performedOperations)
    : m_updateFinder(0)
    , m_updaterApplication(new DummyConfigurationInterface)
    , m_FSEngineClientHandler(initFSEngineClientHandler())
    , m_status(PackageManagerCore::Unfinished)
    , m_needsHardRestart(false)
    , m_testChecksum(false)
    , m_launchedAsRoot(AdminAuthorization::hasAdminRights())
    , m_completeUninstall(false)
    , m_needToWriteUninstaller(false)
    , m_performedOperationsOld(performedOperations)
    , m_dependsOnLocalInstallerBinary(false)
    , m_core(core)
    , m_updates(false)
    , m_repoFetched(false)
    , m_updateSourcesAdded(false)
    , m_magicBinaryMarker(magicInstallerMaker)
    , m_componentsToInstallCalculated(false)
    , m_componentScriptEngine(0)
    , m_controlScriptEngine(0)
    , m_proxyFactory(0)
    , m_defaultModel(0)
    , m_updaterModel(0)
    , m_guiObject(0)
{
    connect(this, SIGNAL(installationStarted()), m_core, SIGNAL(installationStarted()));
    connect(this, SIGNAL(installationFinished()), m_core, SIGNAL(installationFinished()));
    connect(this, SIGNAL(uninstallationStarted()), m_core, SIGNAL(uninstallationStarted()));
    connect(this, SIGNAL(uninstallationFinished()), m_core, SIGNAL(uninstallationFinished()));
}

PackageManagerCorePrivate::~PackageManagerCorePrivate()
{
    clearAllComponentLists();
    clearUpdaterComponentLists();
    clearComponentsToInstall();

    qDeleteAll(m_ownedOperations);
    qDeleteAll(m_performedOperationsOld);
    qDeleteAll(m_performedOperationsCurrentSession);

    // check for fake installer case
    if (m_FSEngineClientHandler)
        m_FSEngineClientHandler->setActive(false);

    delete m_updateFinder;
    delete m_proxyFactory;

    delete m_defaultModel;
    delete m_updaterModel;

    // at the moment the tabcontroller deletes the m_gui, this needs to be changed in the future
    // delete m_gui;
}

/*!
    Return true, if a process with \a name is running. On Windows, comparison is case-insensitive.
*/
/* static */
bool PackageManagerCorePrivate::isProcessRunning(const QString &name,
    const QList<ProcessInfo> &processes)
{
    QList<ProcessInfo>::const_iterator it;
    for (it = processes.constBegin(); it != processes.constEnd(); ++it) {
        if (it->name.isEmpty())
            continue;

#ifndef Q_OS_WIN
        if (it->name == name)
            return true;
        const QFileInfo fi(it->name);
        if (fi.fileName() == name || fi.baseName() == name)
            return true;
#else
        if (it->name.toLower() == name.toLower())
            return true;
        if (it->name.toLower() == QDir::toNativeSeparators(name.toLower()))
            return true;
        const QFileInfo fi(it->name);
        if (fi.fileName().toLower() == name.toLower() || fi.baseName().toLower() == name.toLower())
            return true;
#endif
    }
    return false;
}

/* static */
bool PackageManagerCorePrivate::performOperationThreaded(Operation *operation, OperationType type)
{
    QFutureWatcher<bool> futureWatcher;
    const QFuture<bool> future = QtConcurrent::run(runOperation, operation, type);

    QEventLoop loop;
    loop.connect(&futureWatcher, SIGNAL(finished()), SLOT(quit()), Qt::QueuedConnection);
    futureWatcher.setFuture(future);

    if (!future.isFinished())
        loop.exec();

    return future.result();
}

QString PackageManagerCorePrivate::targetDir() const
{
    return m_core->value(scTargetDir);
}

QString PackageManagerCorePrivate::configurationFileName() const
{
    return m_core->value(scTargetConfigurationFile, QLatin1String("components.xml"));
}

QString PackageManagerCorePrivate::componentsXmlPath() const
{
    return QDir::toNativeSeparators(QDir(QDir::cleanPath(targetDir()))
        .absoluteFilePath(configurationFileName()));
}

bool PackageManagerCorePrivate::buildComponentTree(QHash<QString, Component*> &components, bool loadScript)
{
    try {
        // append all components to their respective parents
        QHash<QString, Component*>::const_iterator it;
        for (it = components.begin(); it != components.end(); ++it) {
            if (statusCanceledOrFailed())
                return false;

            QString id = it.key();
            QInstaller::Component *component = it.value();
            while (!id.isEmpty() && component->parentComponent() == 0) {
                id = id.section(QLatin1Char('.'), 0, -2);
                if (components.contains(id))
                    components[id]->appendComponent(component);
            }
        }

        // append all components w/o parent to the direct list
        foreach (QInstaller::Component *component, components) {
            if (statusCanceledOrFailed())
                return false;

            if (component->parentComponent() == 0)
                m_core->appendRootComponent(component);
        }

        // after everything is set up, load the scripts
        foreach (QInstaller::Component *component, components) {
            if (statusCanceledOrFailed())
                return false;
            if (loadScript)
                component->loadComponentScript();
        }
        // now we can preselect components in the tree
        foreach (QInstaller::Component *component, components) {
            if (statusCanceledOrFailed())
                return false;

            // set the checked state for all components without child (means without tristate)
            if (component->isCheckable() && !component->isTristate()) {
                if (component->isDefault() && isInstaller())
                    component->setCheckState(Qt::Checked);
                else if (component->isInstalled())
                    component->setCheckState(Qt::Checked);
            }
        }
        std::sort(m_rootComponents.begin(), m_rootComponents.end(), Component::SortingPriorityGreaterThan());
    } catch (const Error &error) {
        clearAllComponentLists();
        emit m_core->finishAllComponentsReset(QList<QInstaller::Component*>());
        setStatus(PackageManagerCore::Failure, error.message());

        // TODO: make sure we remove all message boxes inside the library at some point.
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(), QLatin1String("Error"),
            tr("Error"), error.message());
        return false;
    }
    return true;
}

void PackageManagerCorePrivate::cleanUpComponentEnvironment()
{
    // clean up already downloaded data, don't reset registered archives in offline installer case
    if (QInstallerCreator::BinaryFormatEngineHandler::instance() && !m_core->isInstaller())
        QInstallerCreator::BinaryFormatEngineHandler::instance()->resetRegisteredArchives();

    // there could be still some references to already deleted components,
    // so we need to remove the current component script engine
    delete m_componentScriptEngine;
    m_componentScriptEngine = 0;
}

ScriptEngine *PackageManagerCorePrivate::componentScriptEngine() const
{
    if (!m_componentScriptEngine)
        m_componentScriptEngine = new ScriptEngine(m_core);
    return m_componentScriptEngine;
}

ScriptEngine *PackageManagerCorePrivate::controlScriptEngine() const
{
    if (!m_controlScriptEngine)
        m_controlScriptEngine = new ScriptEngine(m_core);
    return m_controlScriptEngine;
}

void PackageManagerCorePrivate::clearAllComponentLists()
{
    QList<QInstaller::Component*> toDelete;

    toDelete << m_rootComponents;
    m_rootComponents.clear();

    m_rootDependencyReplacements.clear();

    const QList<QPair<Component*, Component*> > list = m_componentsToReplaceAllMode.values();
    for (int i = 0; i < list.count(); ++i)
        toDelete << list.at(i).second;
    m_componentsToReplaceAllMode.clear();
    m_componentsToInstallCalculated = false;

    qDeleteAll(toDelete);
    cleanUpComponentEnvironment();
}

void PackageManagerCorePrivate::clearUpdaterComponentLists()
{
    QSet<Component*> usedComponents =
        QSet<Component*>::fromList(m_updaterComponents + m_updaterComponentsDeps);

    const QList<QPair<Component*, Component*> > list = m_componentsToReplaceUpdaterMode.values();
    for (int i = 0; i < list.count(); ++i) {
        if (usedComponents.contains(list.at(i).second))
            qWarning() << "a replacement was already in the list - is that correct?";
        else
            usedComponents.insert(list.at(i).second);
    }

    m_updaterComponents.clear();
    m_updaterComponentsDeps.clear();

    m_updaterDependencyReplacements.clear();

    m_componentsToReplaceUpdaterMode.clear();
    m_componentsToInstallCalculated = false;

    qDeleteAll(usedComponents);
    cleanUpComponentEnvironment();
}

QList<Component *> &PackageManagerCorePrivate::replacementDependencyComponents()
{
    return (!isUpdater()) ? m_rootDependencyReplacements : m_updaterDependencyReplacements;
}

QHash<QString, QPair<Component*, Component*> > &PackageManagerCorePrivate::componentsToReplace()
{
    return (!isUpdater()) ? m_componentsToReplaceAllMode : m_componentsToReplaceUpdaterMode;
}

void PackageManagerCorePrivate::clearComponentsToInstall()
{
    m_visitedComponents.clear();
    m_toInstallComponentIds.clear();
    m_componentsToInstallError.clear();
    m_orderedComponentsToInstall.clear();
    m_toInstallComponentIdReasonHash.clear();
}

bool PackageManagerCorePrivate::appendComponentsToInstall(const QList<Component *> &components)
{
    if (components.isEmpty()) {
        qDebug() << "components list is empty in" << Q_FUNC_INFO;
        return true;
    }

    QList<Component*> relevantComponentForAutoDependOn;
    if (isUpdater())
        relevantComponentForAutoDependOn = m_updaterComponents + m_updaterComponentsDeps;
    else {
        foreach (QInstaller::Component *component, m_rootComponents)
            relevantComponentForAutoDependOn += component->descendantComponents();
    }

    QList<Component*> notAppendedComponents; // for example components with unresolved dependencies
    foreach (Component *component, components){
        if (m_toInstallComponentIds.contains(component->name())) {
            QString errorMessage = QString::fromLatin1("Recursion detected component(%1) already added with "
                "reason: \"%2\"").arg(component->name(), installReason(component));
            qDebug() << qPrintable(errorMessage);
            m_componentsToInstallError.append(errorMessage);
            Q_ASSERT_X(!m_toInstallComponentIds.contains(component->name()), Q_FUNC_INFO,
                qPrintable(errorMessage));
            return false;
        }

        if (component->dependencies().isEmpty())
            realAppendToInstallComponents(component);
        else
            notAppendedComponents.append(component);
    }

    foreach (Component *component, notAppendedComponents) {
        if (!appendComponentToInstall(component))
            return false;
    }

    QList<Component *> foundAutoDependOnList;
    // All regular dependencies are resolved. Now we are looking for auto depend on components.
    foreach (Component *component, relevantComponentForAutoDependOn) {
        // If a components is already installed or is scheduled for installation, no need to check for
        // auto depend installation.
        if ((!component->isInstalled() || component->updateRequested())
            && !m_toInstallComponentIds.contains(component->name())) {
            // If we figure out a component requests auto installation, keep it to resolve their deps as
            // well.
            if (component->isAutoDependOn(m_toInstallComponentIds)) {
                foundAutoDependOnList.append(component);
                insertInstallReason(component, tr("Component(s) added as automatic dependencies"));
            }
        }
    }

    if (!foundAutoDependOnList.isEmpty())
        return appendComponentsToInstall(foundAutoDependOnList);
    return true;
}

bool PackageManagerCorePrivate::appendComponentToInstall(Component *component)
{
    QSet<QString> allDependencies = component->dependencies().toSet();

    foreach (const QString &dependencyComponentName, allDependencies) {
        //componentByName return 0 if dependencyComponentName contains a version which is not available
        Component *dependencyComponent = m_core->componentByName(dependencyComponentName);
        if (dependencyComponent == 0) {
            QString errorMessage;
            if (!dependencyComponent)
                errorMessage = QString::fromLatin1("Cannot find missing dependency (%1) for %2.");
            errorMessage = errorMessage.arg(dependencyComponentName, component->name());
            qDebug() << qPrintable(errorMessage);
            m_componentsToInstallError.append(errorMessage);
            Q_ASSERT_X(false, Q_FUNC_INFO, qPrintable(errorMessage));
            return false;
        }

        if ((!dependencyComponent->isInstalled() || dependencyComponent->updateRequested())
            && !m_toInstallComponentIds.contains(dependencyComponent->name())) {
                if (m_visitedComponents.value(component).contains(dependencyComponent)) {
                    QString errorMessage = QString::fromLatin1("Recursion detected component (%1) already "
                        "added with reason: \"%2\"").arg(component->name(), installReason(component));
                    qDebug() << qPrintable(errorMessage);
                    m_componentsToInstallError = errorMessage;
                    Q_ASSERT_X(!m_visitedComponents.value(component).contains(dependencyComponent), Q_FUNC_INFO,
                        qPrintable(errorMessage));
                    return false;
                }
                m_visitedComponents[component].insert(dependencyComponent);

                // add needed dependency components to the next run
                insertInstallReason(dependencyComponent, tr("Added as dependency for %1.").arg(component->name()));

                if (!appendComponentToInstall(dependencyComponent))
                    return false;
        }
    }

    if (!m_toInstallComponentIds.contains(component->name())) {
        realAppendToInstallComponents(component);
        insertInstallReason(component, tr("Component(s) that have resolved Dependencies"));
    }
    return true;
}

QString PackageManagerCorePrivate::installReason(Component *component)
{
    const QString reason = m_toInstallComponentIdReasonHash.value(component->name());
    if (reason.isEmpty())
        return tr("Selected Component(s) without Dependencies");
    return m_toInstallComponentIdReasonHash.value(component->name());
}


void PackageManagerCorePrivate::initialize(const QHash<QString, QString> &params)
{
    m_coreCheckedHash.clear();
    m_data = PackageManagerCoreData(params);
    m_componentsToInstallCalculated = false;

#ifdef Q_OS_LINUX
    if (m_launchedAsRoot)
        m_data.setValue(scTargetDir, replaceVariables(m_data.settings().adminTargetDir()));
#endif

    if (!m_core->isInstaller()) {
#ifdef Q_OS_MAC
        readMaintenanceConfigFiles(QCoreApplication::applicationDirPath() + QLatin1String("/../../.."));
#else
        readMaintenanceConfigFiles(QCoreApplication::applicationDirPath());
#endif
    }

    foreach (Operation *currentOperation, m_performedOperationsOld)
        currentOperation->setValue(QLatin1String("installer"), QVariant::fromValue(m_core));

    disconnect(this, SIGNAL(installationStarted()), ProgressCoordinator::instance(), SLOT(reset()));
    connect(this, SIGNAL(installationStarted()), ProgressCoordinator::instance(), SLOT(reset()));
    disconnect(this, SIGNAL(uninstallationStarted()), ProgressCoordinator::instance(), SLOT(reset()));
    connect(this, SIGNAL(uninstallationStarted()), ProgressCoordinator::instance(), SLOT(reset()));

    m_updaterApplication.updateSourcesInfo()->setFileName(QString());
    KDUpdater::PackagesInfo &packagesInfo = *m_updaterApplication.packagesInfo();
    packagesInfo.setFileName(componentsXmlPath());

    // Note: force overwriting the application name and version in case we run as installer. Both will be
    //       set to wrong initial values if we install into an already existing installation. This can happen
    //       if the components.xml path has not been changed, but name or version of the new installer.
    if (isInstaller() || packagesInfo.applicationName().isEmpty()) {
        // TODO: this seems to be wrong, we should ask for ProductName defaulting to applicationName...
        packagesInfo.setApplicationName(m_data.settings().applicationName());
    }

    if (isInstaller() || packagesInfo.applicationVersion().isEmpty()) {
        // TODO: this seems to be wrong, we should ask for ProductVersion defaulting to applicationVersion...
        packagesInfo.setApplicationVersion(m_data.settings().applicationVersion());
    }

    if (isInstaller()) {
        // TODO: this seems to be wrong, we should ask for ProductName defaulting to applicationName...
        m_updaterApplication.addUpdateSource(m_data.settings().applicationName(),
            m_data.settings().applicationName(), QString(), QUrl(QLatin1String("resource://metadata/")), 0);
        m_updaterApplication.updateSourcesInfo()->setModified(false);
    }

    m_metadataJob.disconnect();
    m_metadataJob.setAutoDelete(false);
    m_metadataJob.setPackageManagerCore(m_core);
    connect(&m_metadataJob, SIGNAL(infoMessage(KDJob*, QString)), this,
        SLOT(infoMessage(KDJob*, QString)));
    connect(&m_metadataJob, SIGNAL(progress(KDJob *, quint64, quint64)), this,
        SLOT(infoProgress(KDJob *, quint64, quint64)));
    KDUpdater::FileDownloaderFactory::instance().setProxyFactory(m_core->proxyFactory());
}

bool PackageManagerCorePrivate::isOfflineOnly() const
{
    if (!isInstaller())
        return false;

    QSettingsWrapper confInternal(QLatin1String(":/config/config-internal.ini"), QSettingsWrapper::IniFormat);
    return confInternal.value(QLatin1String("offlineOnly"), false).toBool();
}

QString PackageManagerCorePrivate::installerBinaryPath() const
{
    return qApp->applicationFilePath();
}

bool PackageManagerCorePrivate::isInstaller() const
{
    return m_magicBinaryMarker == MagicInstallerMarker;
}

bool PackageManagerCorePrivate::isUninstaller() const
{
    return m_magicBinaryMarker == MagicUninstallerMarker;
}

bool PackageManagerCorePrivate::isUpdater() const
{
    return m_magicBinaryMarker == MagicUpdaterMarker;
}

bool PackageManagerCorePrivate::isPackageManager() const
{
    return m_magicBinaryMarker == MagicPackageManagerMarker;
}

bool PackageManagerCorePrivate::statusCanceledOrFailed() const
{
    return m_status == PackageManagerCore::Canceled || m_status == PackageManagerCore::Failure;
}

void PackageManagerCorePrivate::setStatus(int status, const QString &error)
{
    m_error = error;
    if (!error.isEmpty())
        qDebug() << m_error;
    if (m_status != status) {
        m_status = status;
        emit m_core->statusChanged(PackageManagerCore::Status(m_status));
    }
}

QString PackageManagerCorePrivate::replaceVariables(const QString &str) const
{
    return m_data.replaceVariables(str);
}

QByteArray PackageManagerCorePrivate::replaceVariables(const QByteArray &ba) const
{
    return m_data.replaceVariables(ba);
}

/*!
    \internal
    Creates an update operation owned by the installer, not by any component.
 */
Operation *PackageManagerCorePrivate::createOwnedOperation(const QString &type)
{
    m_ownedOperations.append(KDUpdater::UpdateOperationFactory::instance().create(type));
    return m_ownedOperations.last();
}

/*!
    \internal
    Removes \a operation from the operations owned by the installer, returns the very same operation if the
    operation was found, otherwise 0.
 */
Operation *PackageManagerCorePrivate::takeOwnedOperation(Operation *operation)
{
    if (!m_ownedOperations.contains(operation))
        return 0;

    m_ownedOperations.removeAll(operation);
    return operation;
}

QString PackageManagerCorePrivate::uninstallerName() const
{
    QString filename = m_data.settings().uninstallerName();
#if defined(Q_OS_MAC)
    if (QFileInfo(QCoreApplication::applicationDirPath() + QLatin1String("/../..")).isBundle())
        filename += QLatin1String(".app/Contents/MacOS/") + filename;
#elif defined(Q_OS_WIN)
    filename += QLatin1String(".exe");
#endif
    return QString::fromLatin1("%1/%2").arg(targetDir()).arg(filename);
}

static QNetworkProxy readProxy(QXmlStreamReader &reader)
{
    QNetworkProxy proxy(QNetworkProxy::HttpProxy);
    while (reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("Host"))
            proxy.setHostName(reader.readElementText());
        else if (reader.name() == QLatin1String("Port"))
            proxy.setPort(reader.readElementText().toInt());
        else if (reader.name() == QLatin1String("Username"))
            proxy.setUser(reader.readElementText());
        else if (reader.name() == QLatin1String("Password"))
            proxy.setPassword(reader.readElementText());
        else
            reader.skipCurrentElement();
    }
    return proxy;
}

static QSet<Repository> readRepositories(QXmlStreamReader &reader, bool isDefault)
{
    QSet<Repository> set;
    while (reader.readNextStartElement()) {
        if (reader.name() == QLatin1String("Repository")) {
            Repository repo(QString(), isDefault);
            while (reader.readNextStartElement()) {
                if (reader.name() == QLatin1String("Host"))
                    repo.setUrl(reader.readElementText());
                else if (reader.name() == QLatin1String("Username"))
                    repo.setUsername(reader.readElementText());
                else if (reader.name() == QLatin1String("Password"))
                    repo.setPassword(reader.readElementText());
                else if (reader.name() == QLatin1String("DisplayName"))
                    repo.setDisplayName(reader.readElementText());
                else if (reader.name() == QLatin1String("Enabled"))
                    repo.setEnabled(bool(reader.readElementText().toInt()));
                else
                    reader.skipCurrentElement();
            }
            set.insert(repo);
        } else {
            reader.skipCurrentElement();
        }
    }
    return set;
}

void PackageManagerCorePrivate::writeMaintenanceConfigFiles()
{
    bool gainedAdminRights = false;
    // write current state (variables) to the uninstaller ini file
    const QString iniPath = targetDir() + QLatin1Char('/') + m_data.settings().uninstallerIniFile();
    {
        QFile tmp(iniPath); // force gaining admin rights in case we haven't done already and we need it
        if (!tmp.open(QIODevice::ReadWrite) || !tmp.isWritable()) {
            if (!m_FSEngineClientHandler->isActive())   // check if nobody did it before...
                gainedAdminRights = m_core->gainAdminRights();
        }
        tmp.close();
    }

    QVariantHash variables;
    QSettingsWrapper cfg(iniPath, QSettingsWrapper::IniFormat);
    foreach (const QString &key, m_data.keys()) {
        if (key != scRunProgramDescription && key != scRunProgram && key != scRunProgramArguments)
            variables.insert(key, m_data.value(key));
    }
    cfg.setValue(QLatin1String("Variables"), variables);

    QVariantList repos;
    foreach (const Repository &repo, m_data.settings().defaultRepositories())
        repos.append(QVariant().fromValue(repo));
    cfg.setValue(QLatin1String("DefaultRepositories"), repos);

    cfg.sync();
    if (cfg.status() != QSettingsWrapper::NoError) {
        if (gainedAdminRights)
            m_core->dropAdminRights();
        const QString reason = cfg.status() == QSettingsWrapper::AccessError ? tr("Access error")
            : tr("Format error");
        throw Error(tr("Could not write installer configuration to %1: %2").arg(iniPath, reason));
    }

    QFile file(targetDir() + QLatin1Char('/') + QLatin1String("network.xml"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QXmlStreamWriter writer(&file);
        writer.setCodec("UTF-8");
        writer.setAutoFormatting(true);
        writer.writeStartDocument();

        writer.writeStartElement(QLatin1String("Network"));
            writer.writeTextElement(QLatin1String("ProxyType"), QString::number(m_data.settings().proxyType()));
            writer.writeStartElement(QLatin1String("Ftp"));
                const QNetworkProxy &ftpProxy = m_data.settings().ftpProxy();
                writer.writeTextElement(QLatin1String("Host"), ftpProxy.hostName());
                writer.writeTextElement(QLatin1String("Port"), QString::number(ftpProxy.port()));
                writer.writeTextElement(QLatin1String("Username"), ftpProxy.user());
                writer.writeTextElement(QLatin1String("Password"), ftpProxy.password());
            writer.writeEndElement();
            writer.writeStartElement(QLatin1String("Http"));
                const QNetworkProxy &httpProxy = m_data.settings().httpProxy();
                writer.writeTextElement(QLatin1String("Host"), httpProxy.hostName());
                writer.writeTextElement(QLatin1String("Port"), QString::number(httpProxy.port()));
                writer.writeTextElement(QLatin1String("Username"), httpProxy.user());
                writer.writeTextElement(QLatin1String("Password"), httpProxy.password());
            writer.writeEndElement();

            writer.writeStartElement(QLatin1String("Repositories"));
            foreach (const Repository &repo, m_data.settings().userRepositories()) {
                writer.writeStartElement(QLatin1String("Repository"));
                    writer.writeTextElement(QLatin1String("Host"), repo.url().toString());
                    writer.writeTextElement(QLatin1String("Username"), repo.username());
                    writer.writeTextElement(QLatin1String("Password"), repo.password());
                    writer.writeTextElement(QLatin1String("Enabled"), QString::number(repo.isEnabled()));
                writer.writeEndElement();
            }
            writer.writeEndElement();
        writer.writeEndElement();
    }

    if (gainedAdminRights)
        m_core->dropAdminRights();
}

void PackageManagerCorePrivate::readMaintenanceConfigFiles(const QString &targetDir)
{
    QSettingsWrapper cfg(targetDir + QLatin1Char('/') + m_data.settings().uninstallerIniFile(),
        QSettingsWrapper::IniFormat);
    const QVariantHash vars = cfg.value(QLatin1String("Variables")).toHash();
    for (QHash<QString, QVariant>::ConstIterator it = vars.constBegin(); it != vars.constEnd(); ++it)
        m_data.setValue(it.key(), it.value().toString());

    QSet<Repository> repos;
    const QVariantList variants = cfg.value(QLatin1String("DefaultRepositories")).toList();
    foreach (const QVariant &variant, variants)
        repos.insert(variant.value<Repository>());
    if (!repos.isEmpty())
        m_data.settings().setDefaultRepositories(repos);

    QFile file(targetDir + QLatin1String("/network.xml"));
    if (!file.open(QIODevice::ReadOnly))
        return;

    QXmlStreamReader reader(&file);
    while (!reader.atEnd()) {
        switch (reader.readNext()) {
            case QXmlStreamReader::StartElement: {
                if (reader.name() == QLatin1String("Network")) {
                    while (reader.readNextStartElement()) {
                        const QStringRef name = reader.name();
                        if (name == QLatin1String("Ftp")) {
                            m_data.settings().setFtpProxy(readProxy(reader));
                        } else if (name == QLatin1String("Http")) {
                            m_data.settings().setHttpProxy(readProxy(reader));
                        } else if (reader.name() == QLatin1String("Repositories")) {
                            m_data.settings().addUserRepositories(readRepositories(reader, false));
                        } else if (name == QLatin1String("ProxyType")) {
                            m_data.settings().setProxyType(Settings::ProxyType(reader.readElementText().toInt()));
                        } else {
                            reader.skipCurrentElement();
                        }
                    }
                }
            }   break;

            case QXmlStreamReader::Invalid: {
                qDebug() << reader.errorString();
            }   break;

            default:
                break;
        }
    }
}

void PackageManagerCorePrivate::callBeginInstallation(const QList<Component*> &componentList)
{
    foreach (Component *component, componentList)
        component->beginInstallation();
}

void PackageManagerCorePrivate::stopProcessesForUpdates(const QList<Component*> &components)
{
    QStringList processList;
    foreach (const Component *component, components)
        processList << m_core->replaceVariables(component->stopProcessForUpdateRequests());

    std::sort(processList.begin(), processList.end());
    processList.erase(std::unique(processList.begin(), processList.end()), processList.end());
    if (processList.isEmpty())
        return;

    while (true) {
        const QStringList processes = checkRunningProcessesFromList(processList);
        if (processes.isEmpty())
            return;

        const QMessageBox::StandardButton button =
            MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("stopProcessesForUpdates"), tr("Stop Processes"), tr("These processes "
            "should be stopped to continue:\n\n%1").arg(QDir::toNativeSeparators(processes
            .join(QLatin1String("\n")))), QMessageBox::Retry | QMessageBox::Ignore
            | QMessageBox::Cancel, QMessageBox::Retry);
        if (button == QMessageBox::Ignore)
            return;
        if (button == QMessageBox::Cancel) {
            m_core->setCanceled();
            throw Error(tr("Installation canceled by user"));
        }
    }
}

int PackageManagerCorePrivate::countProgressOperations(const OperationList &operations)
{
    int operationCount = 0;
    foreach (Operation *operation, operations) {
        if (QObject *operationObject = dynamic_cast<QObject*> (operation)) {
            const QMetaObject *const mo = operationObject->metaObject();
            if (mo->indexOfSignal(QMetaObject::normalizedSignature("progressChanged(double)")) > -1)
                operationCount++;
        }
    }
    return operationCount;
}

int PackageManagerCorePrivate::countProgressOperations(const QList<Component*> &components)
{
    int operationCount = 0;
    foreach (Component *component, components)
        operationCount += countProgressOperations(component->operations());

    return operationCount;
}

void PackageManagerCorePrivate::connectOperationToInstaller(Operation *const operation, double operationPartSize)
{
    Q_ASSERT(operationPartSize);
    QObject *const operationObject = dynamic_cast< QObject*> (operation);
    if (operationObject != 0) {
        const QMetaObject *const mo = operationObject->metaObject();
        if (mo->indexOfSignal(QMetaObject::normalizedSignature("outputTextChanged(QString)")) > -1) {
            connect(operationObject, SIGNAL(outputTextChanged(QString)), ProgressCoordinator::instance(),
                SLOT(emitDetailTextChanged(QString)));
        }

        if (mo->indexOfSlot(QMetaObject::normalizedSignature("cancelOperation()")) > -1)
            connect(m_core, SIGNAL(installationInterrupted()), operationObject, SLOT(cancelOperation()));

        if (mo->indexOfSignal(QMetaObject::normalizedSignature("progressChanged(double)")) > -1) {
            ProgressCoordinator::instance()->registerPartProgress(operationObject,
                SIGNAL(progressChanged(double)), operationPartSize);
        }
    }
}

Operation *PackageManagerCorePrivate::createPathOperation(const QFileInfo &fileInfo,
    const QString &componentName)
{
    const bool isDir = fileInfo.isDir();
    // create an operation with the dir/ file as target, it will get deleted on undo
    Operation *operation = createOwnedOperation(QLatin1String(isDir ? "Mkdir" : "Copy"));
    if (isDir)
        operation->setValue(QLatin1String("createddir"), fileInfo.absoluteFilePath());
    operation->setValue(QLatin1String("component"), componentName);
    operation->setArguments(isDir ? QStringList() << fileInfo.absoluteFilePath()
        : QStringList() << QString() << fileInfo.absoluteFilePath());
    return operation;
}

/*!
    This creates fake operations which remove stuff which was registered for uninstallation afterwards
*/
void PackageManagerCorePrivate::registerPathesForUninstallation(
    const QList<QPair<QString, bool> > &pathsForUninstallation, const QString &componentName)
{
    if (pathsForUninstallation.isEmpty())
        return;

    QList<QPair<QString, bool> >::const_iterator it;
    for (it = pathsForUninstallation.begin(); it != pathsForUninstallation.end(); ++it) {
        const bool wipe = it->second;
        const QString path = replaceVariables(it->first);

        const QFileInfo fi(path);
        // create a copy operation with the file as target -> it will get deleted on undo
        Operation *op = createPathOperation(fi, componentName);
        if (fi.isDir())
            op->setValue(QLatin1String("forceremoval"), wipe ? scTrue : scFalse);
        addPerformed(takeOwnedOperation(op));

        // get recursive afterwards
        if (fi.isDir() && !wipe) {
            QDirIterator dirIt(path, QDir::Hidden | QDir::AllEntries | QDir::System
                | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (dirIt.hasNext()) {
                dirIt.next();
                op = createPathOperation(dirIt.fileInfo(), componentName);
                addPerformed(takeOwnedOperation(op));
            }
        }
    }
}

void PackageManagerCorePrivate::writeUninstallerBinary(QFile *const input, qint64 size, bool writeBinaryLayout)
{
    QString uninstallerRenamedName = uninstallerName() + QLatin1String(".new");
    qDebug() << "Writing uninstaller:" << uninstallerRenamedName;
    ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("Writing uninstaller."));

    KDSaveFile out(uninstallerRenamedName);
    openForWrite(&out, out.fileName()); // throws an exception in case of error

    if (!input->seek(0))
        throw Error(QObject::tr("Failed to seek in file %1: %2").arg(input->fileName(), input->errorString()));

    appendData(&out, input, size);
    if (writeBinaryLayout) {
#ifdef Q_OS_MAC
        QDir resourcePath(QFileInfo(uninstallerRenamedName).dir());
        if (!resourcePath.path().endsWith(QLatin1String("Contents/MacOS")))
            throw Error(tr("Uninstaller is not a bundle"));
        resourcePath.cdUp();
        resourcePath.cd(QLatin1String("Resources"));
        // It's a bit odd to have only the magic in the data file, but this simplifies
        // other code a lot (since installers don't have any appended data either)
        QString outPath = resourcePath.filePath(QLatin1String("installer.dat"));
        KDSaveFile dataOut(outPath);
        openForWrite(&dataOut, dataOut.fileName());
        appendInt64(&dataOut, 0);   // operations start
        appendInt64(&dataOut, 0);   // operations end
        appendInt64(&dataOut, 0);   // resource count
        appendInt64(&dataOut, 4 * sizeof(qint64));   // data block size
        appendInt64(&dataOut, QInstaller::MagicUninstallerMarker);
        appendInt64(&dataOut, QInstaller::MagicCookie);
        dataOut.setPermissions(dataOut.permissions() | QFile::WriteUser | QFile::ReadGroup | QFile::ReadOther);
        if (!dataOut.commit(KDSaveFile::OverwriteExistingFile))
            throw Error(tr("Could not write uninstaller data to %1: %2").arg(out.fileName(), out.errorString()));
#else
        appendInt64(&out, 0);   // operations start
        appendInt64(&out, 0);   // operations end
        appendInt64(&out, 0);   // resource count
        appendInt64(&out, 4 * sizeof(qint64));   // data block size
        appendInt64(&out, QInstaller::MagicUninstallerMarker);
        appendInt64(&out, QInstaller::MagicCookie);
#endif
    }
    out.setPermissions(out.permissions() | QFile::WriteUser | QFile::ReadGroup | QFile::ReadOther
        | QFile::ExeOther | QFile::ExeGroup | QFile::ExeUser);

    if (!out.commit(KDSaveFile::OverwriteExistingFile))
        throw Error(tr("Could not write uninstaller to %1: %2").arg(out.fileName(), out.errorString()));
}

void PackageManagerCorePrivate::writeUninstallerBinaryData(QIODevice *output, QFile *const input,
    const OperationList &performedOperations, const BinaryLayout &layout)
{
    const qint64 dataBlockStart = output->pos();

    QVector<Range<qint64> >resourceSegments;
    QVector<Range<qint64> >existingResourceSegments = layout.metadataResourceSegments;

    const QString newDefaultResource = m_core->value(QString::fromLatin1("DefaultResourceReplacement"));
    if (!newDefaultResource.isEmpty()) {
        QFile file(newDefaultResource);
        if (file.open(QIODevice::ReadOnly)) {
            resourceSegments.append(Range<qint64>::fromStartAndLength(output->pos(), file.size()));
            appendData(output, &file, file.size());
            existingResourceSegments.remove(0);

            file.remove();  // clear all possible leftovers
            m_core->setValue(QString::fromLatin1("DefaultResourceReplacement"), QString());
        } else {
            qWarning() << QString::fromLatin1("Could not replace default resource with '%1'.")
                .arg(newDefaultResource);
        }
    }

    foreach (const Range<qint64> &segment, existingResourceSegments) {
        input->seek(segment.start());
        resourceSegments.append(Range<qint64>::fromStartAndLength(output->pos(), segment.length()));
        appendData(output, input, segment.length());
    }

    const qint64 operationsStart = output->pos();
    appendInt64(output, performedOperations.count());
    foreach (Operation *operation, performedOperations) {
        // the installer can't be put into XML, remove it first
        operation->clearValue(QLatin1String("installer"));

        appendString(output, operation->name());
        appendString(output, operation->toXml().toString());

        // for the ui not to get blocked
        qApp->processEvents();
    }
    appendInt64(output, performedOperations.count());
    const qint64 operationsEnd = output->pos();

    // we don't save any component-indexes.
    const qint64 numComponents = 0;
    appendInt64(output, numComponents); // for the indexes
    // we don't save any components.
    const qint64 compIndexStart = output->pos();
    appendInt64(output, numComponents); // and 2 times number of components,
    appendInt64(output, numComponents); // one before and one after the components
    const qint64 compIndexEnd = output->pos();

    appendInt64Range(output, Range<qint64>::fromStartAndEnd(compIndexStart, compIndexEnd)
        .moved(-dataBlockStart));
    foreach (const Range<qint64> segment, resourceSegments)
        appendInt64Range(output, segment.moved(-dataBlockStart));
    appendInt64Range(output, Range<qint64>::fromStartAndEnd(operationsStart, operationsEnd)
        .moved(-dataBlockStart));
    appendInt64(output, layout.resourceCount);
    // data block size, from end of .exe to end of file
    appendInt64(output, output->pos() + 3 * sizeof(qint64) - dataBlockStart);
    appendInt64(output, MagicUninstallerMarker);
}

void PackageManagerCorePrivate::writeUninstaller(OperationList performedOperations)
{
    bool gainedAdminRights = false;
    QTemporaryFile tempAdminFile(targetDir() + QLatin1String("/testjsfdjlkdsjflkdsjfldsjlfds")
        + QString::number(qrand() % 1000));
    if (!tempAdminFile.open() || !tempAdminFile.isWritable()) {
        m_core->gainAdminRights();
        gainedAdminRights = true;
    }

    const QString targetAppDirPath = QFileInfo(uninstallerName()).path();
    if (!QDir().exists(targetAppDirPath)) {
        // create the directory containing the uninstaller (like a bundle structor, on Mac...)
        Operation *op = createOwnedOperation(QLatin1String("Mkdir"));
        op->setArguments(QStringList() << targetAppDirPath);
        performOperationThreaded(op, Backup);
        performOperationThreaded(op);
        performedOperations.append(takeOwnedOperation(op));
    }

#ifdef Q_OS_MAC
    // if it is a bundle, we need some stuff in it...
    const QString sourceAppDirPath = QCoreApplication::applicationDirPath();
    if (isInstaller() && QFileInfo(sourceAppDirPath + QLatin1String("/../..")).isBundle()) {
        Operation *op = createOwnedOperation(QLatin1String("Copy"));
        op->setArguments(QStringList() << (sourceAppDirPath + QLatin1String("/../PkgInfo"))
            << (targetAppDirPath + QLatin1String("/../PkgInfo")));
        performOperationThreaded(op, Backup);
        performOperationThreaded(op);

        // copy Info.plist to target directory
        op = createOwnedOperation(QLatin1String("Copy"));
        op->setArguments(QStringList() << (sourceAppDirPath + QLatin1String("/../Info.plist"))
            << (targetAppDirPath + QLatin1String("/../Info.plist")));
        performOperationThreaded(op, Backup);
        performOperationThreaded(op);

        // patch the Info.plist after copying it
        QFile sourcePlist(sourceAppDirPath + QLatin1String("/../Info.plist"));
        openForRead(&sourcePlist, sourcePlist.fileName());
        QFile targetPlist(targetAppDirPath + QLatin1String("/../Info.plist"));
        openForWrite(&targetPlist, targetPlist.fileName());

        QTextStream in(&sourcePlist);
        QTextStream out(&targetPlist);
        const QString before = QLatin1String("<string>") + QFileInfo(QCoreApplication::applicationFilePath())
            .fileName() + QLatin1String("</string>");
        const QString after = QLatin1String("<string>") + QFileInfo(uninstallerName()).baseName()
            + QLatin1String("</string>");
        while (!in.atEnd())
            out << in.readLine().replace(before, after) << endl;

        // copy qt_menu.nib if it exists
        op = createOwnedOperation(QLatin1String("Mkdir"));
        op->setArguments(QStringList() << (targetAppDirPath + QLatin1String("/../Resources/qt_menu.nib")));
        if (!op->performOperation()) {
            qDebug() << "ERROR in Mkdir operation:" << op->errorString();
        }

        op = createOwnedOperation(QLatin1String("CopyDirectory"));
        op->setArguments(QStringList() << (sourceAppDirPath + QLatin1String("/../Resources/qt_menu.nib"))
            << (targetAppDirPath + QLatin1String("/../Resources/qt_menu.nib")));
        performOperationThreaded(op);

        op = createOwnedOperation(QLatin1String("Mkdir"));
        op->setArguments(QStringList() << (QFileInfo(targetAppDirPath).path() + QLatin1String("/Resources")));
        performOperationThreaded(op, Backup);
        performOperationThreaded(op);

        // copy application icons if it exists
        const QString icon = QFileInfo(QCoreApplication::applicationFilePath()).baseName()
            + QLatin1String(".icns");
        op = createOwnedOperation(QLatin1String("Copy"));
        op->setArguments(QStringList() << (sourceAppDirPath + QLatin1String("/../Resources/") + icon)
            << (targetAppDirPath + QLatin1String("/../Resources/") + icon));
        performOperationThreaded(op, Backup);
        performOperationThreaded(op);

        // finally, copy everything within Frameworks and plugins
        op = createOwnedOperation(QLatin1String("CopyDirectory"));
        op->setArguments(QStringList() << (sourceAppDirPath + QLatin1String("/../Frameworks"))
            << (targetAppDirPath + QLatin1String("/../Frameworks")));
        performOperationThreaded(op);

        op = createOwnedOperation(QLatin1String("CopyDirectory"));
        op->setArguments(QStringList() << (sourceAppDirPath + QLatin1String("/../plugins"))
            << (targetAppDirPath + QLatin1String("/../plugins")));
        performOperationThreaded(op);
    }
#endif

    try {
        // 1 - check if we have a installer base replacement
        //   |--- if so, write out the new tool and remove the replacement
        //   |--- remember to restart and that we need to replace the original binary
        //
        // 2 - if we do not have a replacement, try to open the binary data file as input
        //   |--- try to read the binary layout
        //      |--- on error (see 2.1)
        //          |--- remember we might to append uncompressed resource data (see 3)
        //          |--- set the installer or maintenance binary as input to take over binary data
        //          |--- in case we did not have a replacement, write out an new maintenance tool binary
        //              |--- remember that we need to replace the original binary
        //
        // 3 - open a new binary data file
        //   |--- try to write out the binary data based on the loaded input file (see 2)
        //      |--- on error (see 3.1)
        //          |--- if we wrote a new maintenance tool, take this as output - if not, write out a new
        //                  one and set it as output file, remember we did this
        //          |--- append the binary data based on the loaded input file (see 2), make sure we force
        //                 uncompressing the resource section if we read from a binary data file (see 4.1).
        //
        // 4 - force a deferred rename on the .dat file (see 4.1)
        // 5 - force a deferred rename on the maintenance file (see 5.1)

        // 2.1 - Error cases are: no data file (in fact we are the installer or an old installation),
        //          could not find the data file magic cookie (unknown .dat file), failed to read binary
        //          layout (mostly likely the resource section or we couldn't seek inside the file)
        //
        // 3.1 - most likely the commit operation will fail
        // 4.1 - if 3 failed, this makes sure the .dat file will get removed and on the next run all
        //          binary data is read from the maintenance tool, otherwise it get replaced be the new one
        // 5.1 - this will only happen -if- we wrote out a new binary

        bool newBinaryWritten = false;
        bool replacementExists = false;
        const QString installerBaseBinary = replaceVariables(m_installerBaseBinaryUnreplaced);
        if (!installerBaseBinary.isEmpty() && QFileInfo(installerBaseBinary).exists()) {
            qDebug() << "Got a replacement installer base binary:" << installerBaseBinary;

            QFile replacementBinary(installerBaseBinary);
            try {
                openForRead(&replacementBinary, replacementBinary.fileName());
                writeUninstallerBinary(&replacementBinary, replacementBinary.size(), true);
                qDebug() << "Wrote the binary with the new replacement.";

                newBinaryWritten = true;
                replacementExists = true;
            } catch (const Error &error) {
                qDebug() << error.message();
            }

            if (!replacementBinary.remove()) {
                // Is there anything more sensible we can do with this error? I think not. It's not serious
                // enough for throwing / aborting the process.
                qDebug() << QString::fromLatin1("Could not remove installer base binary '%1' after updating "
                    "the uninstaller: %2").arg(installerBaseBinary, replacementBinary.errorString());
            } else {
                qDebug() << QString::fromLatin1("Removed installer base binary '%1' after updating the "
                    "uninstaller/ maintenance tool.").arg(installerBaseBinary);
            }
            m_installerBaseBinaryUnreplaced.clear();
        } else if (!installerBaseBinary.isEmpty() && !QFileInfo(installerBaseBinary).exists()) {
            qWarning() << QString::fromLatin1("The current uninstaller/ maintenance tool could not be "
                "updated. '%1' does not exist. Please fix the 'setInstallerBaseBinary(<temp_installer_base_"
                "binary_path>)' call in your script.").arg(installerBaseBinary);
        }

        QFile input;
        BinaryLayout layout;
        const QString dataFile = targetDir() + QLatin1Char('/') + m_data.settings().uninstallerName()
            + QLatin1String(".dat");
        for(;;) {
          try {
            if (!isInstaller()) {
                input.setFileName(dataFile);
                openForRead(&input, input.fileName());
                layout = BinaryContent::readBinaryLayout(&input, findMagicCookie(&input, MagicCookieDat));
                break;
            }
            qWarning() << tr("Found a binary data file, but we are the installer and we should read the "
                "binary resource from our very own binary!");
            } catch (const Error &/*error*/) {}

#ifdef Q_OS_MAC
            // On Mac, data is always in a separate file so that the binary can be signed
            QString binaryName = isInstaller() ? installerBinaryPath() : uninstallerName();
            QDir dataPath(QFileInfo(binaryName).dir());
            dataPath.cdUp();
            dataPath.cd(QLatin1String("Resources"));
            input.setFileName(dataPath.filePath(QLatin1String("installer.dat")));

            openForRead(&input, input.fileName());
            layout = BinaryContent::readBinaryLayout(&input, findMagicCookie(&input, MagicCookie));

            if (!newBinaryWritten) {
                newBinaryWritten = true;
                QFile tmp(binaryName);
                openForRead(&tmp, tmp.fileName());
                writeUninstallerBinary(&tmp, tmp.size(), true);
            }
#else
            input.setFileName(isInstaller() ? installerBinaryPath() : uninstallerName());
            openForRead(&input, input.fileName());
            layout = BinaryContent::readBinaryLayout(&input, findMagicCookie(&input, MagicCookie));
            if (!newBinaryWritten) {
                newBinaryWritten = true;
                writeUninstallerBinary(&input, layout.endOfData - layout.dataBlockSize, true);
            }
#endif
            break;
        }

        performedOperations = sortOperationsBasedOnComponentDependencies(performedOperations);
        m_core->setValue(QLatin1String("installedOperationAreSorted"), QLatin1String("true"));

        try {
            KDSaveFile file(dataFile + QLatin1String(".new"));
            openForWrite(&file, file.fileName());

            writeUninstallerBinaryData(&file, &input, performedOperations, layout);
            appendInt64(&file, MagicCookieDat);
            file.setPermissions(file.permissions() | QFile::WriteUser | QFile::ReadGroup
                | QFile::ReadOther);
            if (!file.commit(KDSaveFile::OverwriteExistingFile)) {
                throw Error(tr("Could not write uninstaller binary data to %1: %2").arg(file.fileName(),
                    file.errorString()));
            }
        } catch (const Error &/*error*/) {
            if (!newBinaryWritten) {
                newBinaryWritten = true;
                QFile tmp(isInstaller() ? installerBinaryPath() : uninstallerName());
                openForRead(&tmp, tmp.fileName());
                BinaryLayout tmpLayout = BinaryContent::readBinaryLayout(&tmp, findMagicCookie(&tmp, MagicCookie));
                writeUninstallerBinary(&tmp, tmpLayout.endOfData - tmpLayout.dataBlockSize, false);
            }

            QFile file(uninstallerName() + QLatin1String(".new"));
            openForAppend(&file, file.fileName());
            file.seek(file.size());
            writeUninstallerBinaryData(&file, &input, performedOperations, layout);
            appendInt64(&file, MagicCookie);
        }
        input.close();
        writeMaintenanceConfigFiles();
        deferredRename(dataFile + QLatin1String(".new"), dataFile, false);

        if (newBinaryWritten) {
            const bool restart = replacementExists && isUpdater() && (!statusCanceledOrFailed()) && m_needsHardRestart;
            deferredRename(uninstallerName() + QLatin1String(".new"), uninstallerName(), restart);
            qDebug() << "Maintenance tool restart:" << (restart ? "true." : "false.");
        }
    } catch (const Error &err) {
        setStatus(PackageManagerCore::Failure);
        if (gainedAdminRights)
            m_core->dropAdminRights();
        m_needToWriteUninstaller = false;
        throw err;
    }

    if (gainedAdminRights)
        m_core->dropAdminRights();

    commitSessionOperations();

    m_needToWriteUninstaller = false;
}

QString PackageManagerCorePrivate::registerPath() const
{
#ifdef Q_OS_WIN
    const QString productName = m_data.value(QLatin1String("ProductName")).toString();
    if (productName.isEmpty())
        throw Error(tr("ProductName should be set"));

    QString path = QLatin1String("HKEY_CURRENT_USER");
    if (m_data.value(QLatin1String("AllUsers")).toString() == scTrue)
        path = QLatin1String("HKEY_LOCAL_MACHINE");

    return path + QLatin1String("\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\")
        + productName;
#endif
    return QString();
}

bool PackageManagerCorePrivate::runInstaller()
{
    bool adminRightsGained = false;
    try {
        setStatus(PackageManagerCore::Running);
        emit installationStarted(); // resets also the ProgressCoordninator

        // to have some progress for writeUninstaller
        ProgressCoordinator::instance()->addReservePercentagePoints(1);

        const QString target = QDir::cleanPath(targetDir().replace(QLatin1Char('\\'), QLatin1Char('/')));
        if (target.isEmpty())
            throw Error(tr("Variable 'TargetDir' not set."));

        if (!QDir(target).exists()) {
            const QString &pathToTarget = target.mid(0, target.lastIndexOf(QLatin1Char('/')));
            if (!QDir(pathToTarget).exists()) {
                Operation *pathToTargetOp = createOwnedOperation(QLatin1String("Mkdir"));
                pathToTargetOp->setArguments(QStringList() << pathToTarget);
                if (!performOperationThreaded(pathToTargetOp)) {
                    adminRightsGained = m_core->gainAdminRights();
                    if (!performOperationThreaded(pathToTargetOp))
                        throw Error(pathToTargetOp->errorString());
                }
            }
        } else if (QDir(target).exists()) {
            QTemporaryFile tempAdminFile(target + QLatin1String("/adminrights"));
            if (!tempAdminFile.open() || !tempAdminFile.isWritable())
                adminRightsGained = m_core->gainAdminRights();
        }

        // add the operation to create the target directory
        Operation *mkdirOp = createOwnedOperation(QLatin1String("Mkdir"));
        mkdirOp->setArguments(QStringList() << target);
        mkdirOp->setValue(QLatin1String("forceremoval"), true);
        mkdirOp->setValue(QLatin1String("uninstall-only"), true);

        performOperationThreaded(mkdirOp, Backup);
        if (!performOperationThreaded(mkdirOp)) {
            // if we cannot create the target dir, we try to activate the admin rights
            adminRightsGained = m_core->gainAdminRights();
            if (!performOperationThreaded(mkdirOp))
                throw Error(mkdirOp->errorString());
        }
        const QString remove = m_core->value(scRemoveTargetDir);
        if (QVariant(remove).toBool())
            addPerformed(takeOwnedOperation(mkdirOp));

        // to show that there was some work
        ProgressCoordinator::instance()->addManualPercentagePoints(1);
        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("Preparing the installation..."));

        const QList<Component*> componentsToInstall = m_core->orderedComponentsToInstall();
        qDebug() << "Install size:" << componentsToInstall.size() << "components";

        callBeginInstallation(componentsToInstall);
        stopProcessesForUpdates(componentsToInstall);

        if (m_dependsOnLocalInstallerBinary && !KDUpdater::pathIsOnLocalDevice(qApp->applicationFilePath())) {
            throw Error(tr("It is not possible to install from network location"));
        }

        if (!adminRightsGained) {
            foreach (Component *component, m_core->orderedComponentsToInstall()) {
                if (component->value(scRequiresAdminRights, scFalse) == scFalse)
                    continue;

                m_core->gainAdminRights();
                m_core->dropAdminRights();
                break;
            }
        }

        const double downloadPartProgressSize = double(1) / double(3);
        double componentsInstallPartProgressSize = double(2) / double(3);
        const int downloadedArchivesCount = m_core->downloadNeededArchives(downloadPartProgressSize);

        // if there was no download we have the whole progress for installing components
        if (!downloadedArchivesCount)
            componentsInstallPartProgressSize = double(1);

        // Force an update on the components xml as the install dir might have changed.
        KDUpdater::PackagesInfo &info = *m_updaterApplication.packagesInfo();
        info.setFileName(componentsXmlPath());
        // Clear the packages as we might install into an already existing installation folder.
        info.clearPackageInfoList();
        // also update the application name and version, might be set from a script as well
        info.setApplicationName(m_data.value(QLatin1String("ProductName"),
            m_data.settings().applicationName()).toString());
        info.setApplicationVersion(m_data.value(QLatin1String("ProductVersion"),
            m_data.settings().applicationVersion()).toString());

        const int progressOperationCount = countProgressOperations(componentsToInstall)
            // add one more operation as we support progress
            + (PackageManagerCore::createLocalRepositoryFromBinary() ? 1 : 0);
        double progressOperationSize = componentsInstallPartProgressSize / progressOperationCount;

        foreach (Component *component, componentsToInstall)
            installComponent(component, progressOperationSize, adminRightsGained);

        if (m_core->isOfflineOnly() && PackageManagerCore::createLocalRepositoryFromBinary()) {
            emit m_core->titleMessageChanged(tr("Creating local repository"));
            ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(QString());
            ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("Creating local repository"));

            Operation *createRepo = createOwnedOperation(QLatin1String("CreateLocalRepository"));
            if (createRepo) {
                createRepo->setValue(QLatin1String("uninstall-only"), true);
                createRepo->setValue(QLatin1String("installer"), QVariant::fromValue(m_core));
                createRepo->setArguments(QStringList() << QCoreApplication::applicationFilePath()
                    << target + QLatin1String("/repository"));

                connectOperationToInstaller(createRepo, progressOperationSize);

                bool success = performOperationThreaded(createRepo);
                if (!success) {
                    adminRightsGained = m_core->gainAdminRights();
                    success = performOperationThreaded(createRepo);
                }

                if (success) {
                    QSet<Repository> repos;
                    foreach (Repository repo, m_data.settings().defaultRepositories()) {
                        repo.setEnabled(false);
                        repos.insert(repo);
                    }
                    repos.insert(Repository(QUrl::fromUserInput(createRepo
                        ->value(QLatin1String("local-repo")).toString()), true));
                    m_data.settings().setDefaultRepositories(repos);
                    addPerformed(takeOwnedOperation(createRepo));
                } else {
                    MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                        QLatin1String("installationError"), tr("Error"), createRepo->errorString());
                    createRepo->undoOperation();
                }
            }
        }

        emit m_core->titleMessageChanged(tr("Creating Uninstaller"));

        writeUninstaller(m_performedOperationsOld + m_performedOperationsCurrentSession);
        registerUninstaller();

        // fake a possible wrong value to show a full progress bar
        const int progress = ProgressCoordinator::instance()->progressInPercentage();
        // usually this should be only the reserved one from the beginning
        if (progress < 100)
            ProgressCoordinator::instance()->addManualPercentagePoints(100 - progress);
        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("\nInstallation finished!"));

        if (adminRightsGained)
            m_core->dropAdminRights();
        setStatus(PackageManagerCore::Success);
        emit installationFinished();
    } catch (const Error &err) {
        if (m_core->status() != PackageManagerCore::Canceled) {
            setStatus(PackageManagerCore::Failure);
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationError"), tr("Error"), err.message());
            qDebug() << "ROLLING BACK operations=" << m_performedOperationsCurrentSession.count();
        }

        m_core->rollBackInstallation();

        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("\nInstallation aborted!"));
        if (adminRightsGained)
            m_core->dropAdminRights();
        emit installationFinished();
        return false;
    }
    return true;
}

bool PackageManagerCorePrivate::runPackageUpdater()
{
    bool adminRightsGained = false;
    if (m_completeUninstall) {
        return runUninstaller();
    }
    try {
        setStatus(PackageManagerCore::Running);
        emit installationStarted(); //resets also the ProgressCoordninator

        //to have some progress for the cleanup/write component.xml step
        ProgressCoordinator::instance()->addReservePercentagePoints(1);

        const QString packagesXml = componentsXmlPath();
        // check if we need admin rights and ask before the action happens
        if (!QFileInfo(installerBinaryPath()).isWritable() || !QFileInfo(packagesXml).isWritable())
            adminRightsGained = m_core->gainAdminRights();

        const QList<Component *> componentsToInstall = m_core->orderedComponentsToInstall();
        qDebug() << "Install size:" << componentsToInstall.size() << "components";

        callBeginInstallation(componentsToInstall);
        stopProcessesForUpdates(componentsToInstall);

        if (m_dependsOnLocalInstallerBinary && !KDUpdater::pathIsOnLocalDevice(qApp->applicationFilePath())) {
            throw Error(tr("It is not possible to run that operation from a network location"));
        }

        bool updateAdminRights = false;
        if (!adminRightsGained) {
            foreach (Component *component, componentsToInstall) {
                if (component->value(scRequiresAdminRights, scFalse) == scFalse)
                    continue;

                updateAdminRights = true;
                break;
            }
        }

        OperationList undoOperations;
        OperationList nonRevertedOperations;
        QHash<QString, Component *> componentsByName;

        // order the operations in the right component dependency order
        // next loop will save the needed operations in reverse order for uninstallation
        OperationList performedOperationsOld = m_performedOperationsOld;
        if (m_core->value(QLatin1String("installedOperationAreSorted")) != QLatin1String("true"))
            performedOperationsOld = sortOperationsBasedOnComponentDependencies(m_performedOperationsOld);

        // build a list of undo operations based on the checked state of the component
        foreach (Operation *operation, performedOperationsOld) {
            const QString &name = operation->value(QLatin1String("component")).toString();
            Component *component = componentsByName.value(name, 0);
            if (!component)
                component = m_core->componentByName(name);
            if (component)
                componentsByName.insert(name, component);

            if (isUpdater()) {
                // We found the component, the component is not scheduled for update, the dependency solver
                // did not add the component as install dependency and there is no replacement, keep it.
                if ((component && !component->updateRequested() && !componentsToInstall.contains(component)
                    && !m_componentsToReplaceUpdaterMode.contains(name))) {
                        nonRevertedOperations.append(operation);
                        continue;
                }

                // There is a replacement, but the replacement is not scheduled for update, keep it as well.
                if (m_componentsToReplaceUpdaterMode.contains(name)
                    && !m_componentsToReplaceUpdaterMode.value(name).first->updateRequested()) {
                        nonRevertedOperations.append(operation);
                        continue;
                }
            } else if (isPackageManager()) {
                // We found the component, the component is still checked and the dependency solver did not
                // add the component as install dependency, keep it.
                if (component && component->isSelected() && !componentsToInstall.contains(component)) {
                    nonRevertedOperations.append(operation);
                    continue;
                }

                // There is a replacement, but the replacement is not scheduled for update, keep it as well.
                if (m_componentsToReplaceAllMode.contains(name)
                    && !m_componentsToReplaceAllMode.value(name).first->installationRequested()) {
                        nonRevertedOperations.append(operation);
                        continue;
                }
            } else {
                Q_ASSERT_X(false, Q_FUNC_INFO, "Invalid package manager mode!");
            }

            // Filter out the create target dir undo operation, it's only needed for full uninstall.
            // Note: We filter for unnamed operations as well, since old installations had the remove target
            //  dir operation without the "uninstall-only", which will result in a complete uninstallation
            //  during an update for the maintenance tool.
            if (operation->value(QLatin1String("uninstall-only")).toBool()
                || operation->value(QLatin1String("component")).toString().isEmpty()) {
                    nonRevertedOperations.append(operation);
                    continue;
            }

            // uninstallation should be in reverse order so prepend it here
            undoOperations.prepend(operation);
            updateAdminRights |= operation->value(QLatin1String("admin")).toBool();
        }

        // we did not request admin rights till we found out that a component/ undo needs admin rights
        if (updateAdminRights && !adminRightsGained) {
            m_core->gainAdminRights();
            m_core->dropAdminRights();
        }

        double undoOperationProgressSize = 0;
        const double downloadPartProgressSize = double(2) / double(5);
        double componentsInstallPartProgressSize = double(3) / double(5);
        if (undoOperations.count() > 0) {
            undoOperationProgressSize = double(1) / double(5);
            componentsInstallPartProgressSize = downloadPartProgressSize;
            undoOperationProgressSize /= countProgressOperations(undoOperations);
        }

        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("Preparing the installation..."));

        // following, we download the needed archives
        m_core->downloadNeededArchives(downloadPartProgressSize);

        if (undoOperations.count() > 0) {
            ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("Removing deselected components..."));
            runUndoOperations(undoOperations, undoOperationProgressSize, adminRightsGained, true);
        }
        m_performedOperationsOld = nonRevertedOperations; // these are all operations left: those not reverted

        const double progressOperationCount = countProgressOperations(componentsToInstall);
        const double progressOperationSize = componentsInstallPartProgressSize / progressOperationCount;

        foreach (Component *component, componentsToInstall)
            installComponent(component, progressOperationSize, adminRightsGained);

        emit m_core->titleMessageChanged(tr("Creating Uninstaller"));

        commitSessionOperations(); //end session, move ops to "old"
        m_needToWriteUninstaller = true;

        // fake a possible wrong value to show a full progress bar
        const int progress = ProgressCoordinator::instance()->progressInPercentage();
        // usually this should be only the reserved one from the beginning
        if (progress < 100)
            ProgressCoordinator::instance()->addManualPercentagePoints(100 - progress);
        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("\nUpdate finished!"));

        if (adminRightsGained)
            m_core->dropAdminRights();
        setStatus(PackageManagerCore::Success);
        emit installationFinished();
    } catch (const Error &err) {
        if (m_core->status() != PackageManagerCore::Canceled) {
            setStatus(PackageManagerCore::Failure);
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationError"), tr("Error"), err.message());
            qDebug() << "ROLLING BACK operations=" << m_performedOperationsCurrentSession.count();
        }

        m_core->rollBackInstallation();

        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("\nUpdate aborted!"));
        if (adminRightsGained)
            m_core->dropAdminRights();
        emit installationFinished();
        return false;
    }
    return true;
}

bool PackageManagerCorePrivate::runUninstaller()
{
    bool adminRightsGained = false;
    try {
        setStatus(PackageManagerCore::Running);
        emit uninstallationStarted();

        // check if we need administration rights and ask before the action happens
        if (!QFileInfo(installerBinaryPath()).isWritable())
            adminRightsGained = m_core->gainAdminRights();

        OperationList undoOperations;
        bool updateAdminRights = false;
        foreach (Operation *op, m_performedOperationsOld) {
            undoOperations.prepend(op);
            updateAdminRights |= op->value(QLatin1String("admin")).toBool();
        }

        // we did not request administration rights till we found out that a undo needs administration rights
        if (updateAdminRights && !adminRightsGained) {
            m_core->gainAdminRights();
            m_core->dropAdminRights();
        }

        const int uninstallOperationCount = countProgressOperations(undoOperations);
        const double undoOperationProgressSize = double(1) / double(uninstallOperationCount);

        runUndoOperations(undoOperations, undoOperationProgressSize, adminRightsGained, false);
        // No operation delete here, as all old undo operations are deleted in the destructor.

        // this will also delete the TargetDir on Windows
        deleteUninstaller();

        if (QVariant(m_core->value(scRemoveTargetDir)).toBool()) {
            // on !Windows, we need to remove TargetDir manually
            qDebug() << "Complete uninstallation is chosen";
            const QString target = targetDir();
            if (!target.isEmpty()) {
                if (updateAdminRights && !adminRightsGained) {
                    // we were root at least once, so we remove the target dir as root
                    m_core->gainAdminRights();
                    removeDirectoryThreaded(target, true);
                    m_core->dropAdminRights();
                } else {
                    removeDirectoryThreaded(target, true);
                }
            }
        }

        unregisterUninstaller();
        m_needToWriteUninstaller = false;

        setStatus(PackageManagerCore::Success);
        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(
            tr("\nUninstallation completed successfully!"));
        if (adminRightsGained)
            m_core->dropAdminRights();
        emit uninstallationFinished();
    } catch (const Error &err) {
        if (m_core->status() != PackageManagerCore::Canceled) {
            setStatus(PackageManagerCore::Failure);
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationError"), tr("Error"), err.message());
        }

        ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("\nUninstallation aborted!"));
        if (adminRightsGained)
            m_core->dropAdminRights();
        emit uninstallationFinished();
        return false;
    }
    return true;
}

void PackageManagerCorePrivate::installComponent(Component *component, double progressOperationSize,
    bool adminRightsGained)
{
    const OperationList operations = component->operations();
    if (!component->operationsCreatedSuccessfully())
        m_core->setCanceled();

    const int opCount = operations.count();
    // show only components which do something, MinimumProgress is only for progress calculation safeness
    if (opCount > 1 || (opCount == 1 && operations.at(0)->name() != QLatin1String("MinimumProgress"))) {
            ProgressCoordinator::instance()->emitLabelAndDetailTextChanged(tr("\nInstalling component %1")
                .arg(component->displayName()));
    }

    foreach (Operation *operation, operations) {
        if (statusCanceledOrFailed())
            throw Error(tr("Installation canceled by user"));

        // maybe this operations wants us to be admin...
        bool becameAdmin = false;
        if (!adminRightsGained && operation->value(QLatin1String("admin")).toBool()) {
            becameAdmin = m_core->gainAdminRights();
            qDebug() << operation->name() << "as admin:" << becameAdmin;
        }

        connectOperationToInstaller(operation, progressOperationSize);
        connectOperationCallMethodRequest(operation);

        // allow the operation to backup stuff before performing the operation
        performOperationThreaded(operation, PackageManagerCorePrivate::Backup);

        bool ignoreError = false;
        bool ok = performOperationThreaded(operation);
        while (!ok && !ignoreError && m_core->status() != PackageManagerCore::Canceled) {
            qDebug() << QString::fromLatin1("Operation '%1' with arguments: '%2' failed: %3")
                .arg(operation->name(), operation->arguments().join(QLatin1String("; ")),
                operation->errorString());
            const QMessageBox::StandardButton button =
                MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("installationErrorWithRetry"), tr("Installer Error"),
                tr("Error during installation process (%1):\n%2").arg(component->name(),
                operation->errorString()),
                QMessageBox::Retry | QMessageBox::Ignore | QMessageBox::Cancel, QMessageBox::Retry);

            if (button == QMessageBox::Retry)
                ok = performOperationThreaded(operation);
            else if (button == QMessageBox::Ignore)
                ignoreError = true;
            else if (button == QMessageBox::Cancel)
                m_core->interrupt();
        }

        if (ok || operation->error() > Operation::InvalidArguments) {
            // Remember that the operation was performed, that allows us to undo it if a following operation
            // fails or if this operation failed but still needs an undo call to cleanup.
            addPerformed(operation);
        }

        if (becameAdmin)
            m_core->dropAdminRights();

        if (!ok && !ignoreError)
            throw Error(operation->errorString());

        if (component->value(scEssential, scFalse) == scTrue)
            m_needsHardRestart = true;
    }

    registerPathesForUninstallation(component->pathsForUninstallation(), component->name());

    if (!component->stopProcessForUpdateRequests().isEmpty()) {
        Operation *stopProcessForUpdatesOp = KDUpdater::UpdateOperationFactory::instance()
            .create(QLatin1String("FakeStopProcessForUpdate"));
        const QStringList arguments(component->stopProcessForUpdateRequests().join(QLatin1String(",")));
        stopProcessForUpdatesOp->setArguments(arguments);
        addPerformed(stopProcessForUpdatesOp);
        stopProcessForUpdatesOp->setValue(QLatin1String("component"), component->name());
    }

    // now mark the component as installed
    KDUpdater::PackagesInfo &packages = *m_updaterApplication.packagesInfo();
    packages.installPackage(component->name(), component->value(scVersion), component->value(scDisplayName),
        component->value(scDescription), component->dependencies(), component->forcedInstallation(),
        component->isVirtual(), component->value(scUncompressedSize).toULongLong(),
        component->value(scInheritVersion));
    packages.writeToDisk();

    component->setInstalled();
    component->markAsPerformedInstallation();
}

// -- private

void PackageManagerCorePrivate::deleteUninstaller()
{
#ifdef Q_OS_WIN
    // Since Windows does not support that the uninstaller deletes itself we  have to go with a rather dirty
    // hack. What we do is to create a batchfile that will try to remove the uninstaller once per second. Then
    // we start that batchfile detached, finished our job and close ourselves. Once that's done the batchfile
    // will succeed in deleting our uninstall.exe and, if the installation directory was created but us and if
    // it's empty after the uninstall, deletes the installation-directory.
    const QString batchfile = QDir::toNativeSeparators(QFileInfo(QDir::tempPath(),
        QLatin1String("uninstall.vbs")).absoluteFilePath());
    QFile f(batchfile);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        throw Error(tr("Cannot prepare uninstall"));

    QTextStream batch(&f);
    batch << "Set fso = WScript.CreateObject(\"Scripting.FileSystemObject\")\n";
    batch << "file = WScript.Arguments.Item(0)\n";
    batch << "folderpath = WScript.Arguments.Item(1)\n";
    batch << "Set folder = fso.GetFolder(folderpath)\n";
    batch << "on error resume next\n";

    batch << "while fso.FileExists(file)\n";
    batch << "    fso.DeleteFile(file)\n";
    batch << "    WScript.Sleep(1000)\n";
    batch << "wend\n";
//    batch << "if folder.SubFolders.Count = 0 and folder.Files.Count = 0 then\n";
    batch << "    Set folder = Nothing\n";
    batch << "    fso.DeleteFolder folderpath, true\n";
//    batch << "end if\n";
    batch << "fso.DeleteFile(WScript.ScriptFullName)\n";

    f.close();

    QStringList arguments;
    arguments << QLatin1String("//Nologo") << batchfile; // execute the batchfile
    arguments << QDir::toNativeSeparators(QFileInfo(installerBinaryPath()).absoluteFilePath());
    if (!m_performedOperationsOld.isEmpty()) {
        const Operation *const op = m_performedOperationsOld.first();
        if (op->name() == QLatin1String("Mkdir")) // the target directory name
            arguments << QDir::toNativeSeparators(QFileInfo(op->arguments().first()).absoluteFilePath());
    }

    if (!QProcessWrapper::startDetached(QLatin1String("cscript"), arguments, QDir::rootPath()))
        throw Error(tr("Cannot start uninstall"));
#else
    // every other platform has no problem if we just delete ourselves now
    QFile uninstaller(QFileInfo(installerBinaryPath()).absoluteFilePath());
    uninstaller.remove();
# ifdef Q_OS_MAC
    const QLatin1String cdUp("/../../..");
    if (QFileInfo(QFileInfo(installerBinaryPath() + cdUp).absoluteFilePath()).isBundle()) {
        removeDirectoryThreaded(QFileInfo(installerBinaryPath() + cdUp).absoluteFilePath());
        QFile::remove(QFileInfo(installerBinaryPath() + cdUp).absolutePath()
            + QLatin1String("/") + configurationFileName());
    } else
# endif
#endif
    {
        // finally remove the components.xml, since it still exists now
        QFile::remove(QFileInfo(installerBinaryPath()).absolutePath() + QLatin1String("/")
            + configurationFileName());
    }
}

void PackageManagerCorePrivate::registerUninstaller()
{
#ifdef Q_OS_WIN
    QSettingsWrapper settings(registerPath(), QSettingsWrapper::NativeFormat);
    settings.setValue(scDisplayName, m_data.value(QLatin1String("ProductName")));
    settings.setValue(QLatin1String("DisplayVersion"), m_data.value(QLatin1String("ProductVersion")));
    const QString uninstaller = QDir::toNativeSeparators(uninstallerName());
    settings.setValue(QLatin1String("DisplayIcon"), uninstaller);
    settings.setValue(scPublisher, m_data.value(scPublisher));
    settings.setValue(QLatin1String("UrlInfoAbout"), m_data.value(QLatin1String("Url")));
    settings.setValue(QLatin1String("Comments"), m_data.value(scTitle));
    settings.setValue(QLatin1String("InstallDate"), QDateTime::currentDateTime().toString());
    settings.setValue(QLatin1String("InstallLocation"), QDir::toNativeSeparators(targetDir()));
    settings.setValue(QLatin1String("UninstallString"), uninstaller);
    settings.setValue(QLatin1String("ModifyPath"), uninstaller + QLatin1String(" --manage-packages"));
    settings.setValue(QLatin1String("EstimatedSize"), QFileInfo(installerBinaryPath()).size());
    settings.setValue(QLatin1String("NoModify"), 0);
    settings.setValue(QLatin1String("NoRepair"), 1);
#endif
}

void PackageManagerCorePrivate::unregisterUninstaller()
{
#ifdef Q_OS_WIN
    QSettingsWrapper settings(registerPath(), QSettingsWrapper::NativeFormat);
    settings.remove(QString());
#endif
}

void PackageManagerCorePrivate::runUndoOperations(const OperationList &undoOperations, double progressSize,
    bool adminRightsGained, bool deleteOperation)
{
    KDUpdater::PackagesInfo &packages = *m_updaterApplication.packagesInfo();
    try {
        foreach (Operation *undoOperation, undoOperations) {
            if (statusCanceledOrFailed())
                throw Error(tr("Installation canceled by user"));

            bool becameAdmin = false;
            if (!adminRightsGained && undoOperation->value(QLatin1String("admin")).toBool())
                becameAdmin = m_core->gainAdminRights();

            connectOperationToInstaller(undoOperation, progressSize);
            qDebug() << "undo operation=" << undoOperation->name();

            bool ignoreError = false;
            bool ok = performOperationThreaded(undoOperation, PackageManagerCorePrivate::Undo);

            const QString componentName = undoOperation->value(QLatin1String("component")).toString();

            if (!componentName.isEmpty()) {
                while (!ok && !ignoreError && m_core->status() != PackageManagerCore::Canceled) {
                    const QMessageBox::StandardButton button =
                        MessageBoxHandler::warning(MessageBoxHandler::currentBestSuitParent(),
                        QLatin1String("installationErrorWithRetry"), tr("Installer Error"),
                        tr("Error during uninstallation process:\n%1").arg(undoOperation->errorString()),
                        QMessageBox::Retry | QMessageBox::Ignore, QMessageBox::Retry);

                    if (button == QMessageBox::Retry)
                        ok = performOperationThreaded(undoOperation, Undo);
                    else if (button == QMessageBox::Ignore)
                        ignoreError = true;
                }
                Component *component = m_core->componentByName(componentName);
                if (!component)
                    component = componentsToReplace().value(componentName).second;
                if (component) {
                    component->setUninstalled();
                    packages.removePackage(component->name());
                }
            }

            if (becameAdmin)
                m_core->dropAdminRights();

            if (deleteOperation)
                delete undoOperation;
        }
    } catch (const Error &error) {
        packages.writeToDisk();
        throw Error(error.message());
    } catch (...) {
        packages.writeToDisk();
        throw Error(tr("Unknown error"));
    }
    packages.writeToDisk();
}

PackagesList PackageManagerCorePrivate::remotePackages()
{
    if (m_updates && m_updateFinder)
        return m_updateFinder->updates();

    m_updates = false;
    delete m_updateFinder;

    m_updateFinder = new KDUpdater::UpdateFinder(&m_updaterApplication);
    m_updateFinder->setAutoDelete(false);
    m_updateFinder->run();

    if (m_updateFinder->updates().isEmpty()) {
        setStatus(PackageManagerCore::Failure, tr("Could not retrieve remote tree: %1.")
            .arg(m_updateFinder->errorString()));
        return PackagesList();
    }

    m_updates = true;
    return m_updateFinder->updates();
}

/*!
    Returns a hash containing the installed package name and it's associated package information. If
    the application is running in installer mode or the local components file could not be parsed, the
    hash is empty.
*/
LocalPackagesHash PackageManagerCorePrivate::localInstalledPackages()
{
    LocalPackagesHash installedPackages;

    if (!isInstaller()) {
        KDUpdater::PackagesInfo &packagesInfo = *m_updaterApplication.packagesInfo();
        if (!packagesInfo.isValid()) {
            packagesInfo.setFileName(componentsXmlPath());
            if (packagesInfo.applicationName().isEmpty())
                packagesInfo.setApplicationName(m_data.settings().applicationName());
            if (packagesInfo.applicationVersion().isEmpty())
                packagesInfo.setApplicationVersion(m_data.settings().applicationVersion());
        }

        if (packagesInfo.error() != KDUpdater::PackagesInfo::NoError)
            setStatus(PackageManagerCore::Failure, tr("Failure to read packages from: %1.").arg(componentsXmlPath()));

        foreach (const LocalPackage &package, packagesInfo.packageInfos()) {
            if (statusCanceledOrFailed())
                break;
            installedPackages.insert(package.name, package);
        }
     }

    return installedPackages;
}

bool PackageManagerCorePrivate::fetchMetaInformationFromRepositories()
{
    if (m_repoFetched)
        return m_repoFetched;

    m_updates = false;
    m_repoFetched = false;
    m_updateSourcesAdded = false;

    try {
        m_metadataJob.start();
        m_metadataJob.waitForFinished();
    } catch (Error &error) {
        setStatus(PackageManagerCore::Failure, tr("Could not retrieve meta information: %1")
            .arg(error.message()));
        return m_repoFetched;
    }

    if (m_metadataJob.error() != KDJob::NoError) {
        switch (m_metadataJob.error()) {
            case QInstaller::UserIgnoreError:
                break;  // we can simply ignore this error, the user knows about it
            default:
                setStatus(PackageManagerCore::Failure, m_metadataJob.errorString());
                return m_repoFetched;
        }
    }

    m_repoFetched = true;
    return m_repoFetched;
}

bool PackageManagerCorePrivate::addUpdateResourcesFromRepositories(bool parseChecksum)
{
    if (m_updateSourcesAdded)
        return m_updateSourcesAdded;

    const QList<Metadata> metadata = m_metadataJob.metadata();
    if (metadata.isEmpty()) {
        m_updateSourcesAdded = true;
        return m_updateSourcesAdded;
    }

    // forces an refresh / clear on all update sources
    m_updaterApplication.updateSourcesInfo()->refresh();
    if (isInstaller()) {
        m_updaterApplication.addUpdateSource(m_data.settings().applicationName(),
            m_data.settings().applicationName(), QString(),
            QUrl(QLatin1String("resource://metadata/")), 0);
        m_updaterApplication.updateSourcesInfo()->setModified(false);
    }

    m_updates = false;
    m_updateSourcesAdded = false;

    const QString &appName = m_data.settings().applicationName();
    foreach (const Metadata &data, metadata) {
        if (statusCanceledOrFailed())
            return false;

        if (data.directory.isEmpty())
            continue;

        if (parseChecksum) {
            const QString updatesXmlPath = data.directory + QLatin1String("/Updates.xml");
            QFile updatesFile(updatesXmlPath);
            try {
                openForRead(&updatesFile, updatesFile.fileName());
            } catch(const Error &e) {
                qDebug() << "Error opening Updates.xml:" << e.message();
                setStatus(PackageManagerCore::Failure, tr("Could not add temporary update source information."));
                return false;
            }

            int line = 0;
            int column = 0;
            QString error;
            QDomDocument doc;
            if (!doc.setContent(&updatesFile, &error, &line, &column)) {
                qDebug() << QString::fromLatin1("Parse error in file %4: %1 at line %2 col %3").arg(error,
                    QString::number(line), QString::number(column), updatesFile.fileName());
                setStatus(PackageManagerCore::Failure, tr("Could not add temporary update source information."));
                return false;
            }

            const QDomNode checksum = doc.documentElement().firstChildElement(QLatin1String("Checksum"));
            if (!checksum.isNull())
                m_core->setTestChecksum(checksum.toElement().text().toLower() == scTrue);
        }
        m_updaterApplication.addUpdateSource(appName, appName, QString(),
            QUrl::fromLocalFile(data.directory), 1);
    }
    m_updaterApplication.updateSourcesInfo()->setModified(false);

    if (m_updaterApplication.updateSourcesInfo()->updateSourceInfoCount() == 0) {
        setStatus(PackageManagerCore::Failure, tr("Could not find any update source information."));
        return false;
    }

    m_updateSourcesAdded = true;
    return m_updateSourcesAdded;
}

void PackageManagerCorePrivate::realAppendToInstallComponents(Component *component)
{
    if (!component->isInstalled() || component->updateRequested()) {
        // remove the checkState method if we don't use selected in scripts
        setCheckedState(component, Qt::Checked);

        m_orderedComponentsToInstall.append(component);
        m_toInstallComponentIds.insert(component->name());
    }
}

void PackageManagerCorePrivate::insertInstallReason(Component *component, const QString &reason)
{
    // keep the first reason
    if (m_toInstallComponentIdReasonHash.value(component->name()).isEmpty())
        m_toInstallComponentIdReasonHash.insert(component->name(), reason);
}

bool PackageManagerCorePrivate::appendComponentToUninstall(Component *component)
{
    // remove all already resolved dependees
    QSet<Component *> dependees = m_core->dependees(component).toSet().subtract(m_componentsToUninstall);
    if (dependees.isEmpty()) {
        setCheckedState(component, Qt::Unchecked);
        m_componentsToUninstall.insert(component);
        return true;
    }

    QSet<Component *> dependeesToResolve;
    foreach (Component *dependee, dependees) {
        if (dependee->isInstalled()) {
            // keep them as already resolved
            setCheckedState(dependee, Qt::Unchecked);
            m_componentsToUninstall.insert(dependee);
            // gather possible dependees, keep them to resolve it later
            dependeesToResolve.unite(m_core->dependees(dependee).toSet());
        }
    }

    bool allResolved = true;
    foreach (Component *dependee, dependeesToResolve)
        allResolved &= appendComponentToUninstall(dependee);

    return allResolved;
}

bool PackageManagerCorePrivate::appendComponentsToUninstall(const QList<Component*> &components)
{
    if (components.isEmpty()) {
        qDebug() << "components list is empty in" << Q_FUNC_INFO;
        return true;
    }

    bool allResolved = true;
    foreach (Component *component, components) {
        if (component->isInstalled()) {
            setCheckedState(component, Qt::Unchecked);
            m_componentsToUninstall.insert(component);
            allResolved &= appendComponentToUninstall(component);
        }
    }

    QSet<Component*> installedComponents;
    foreach (const QString &name, localInstalledPackages().keys()) {
        if (Component *component = m_core->componentByName(name)) {
            if (!component->uninstallationRequested())
                installedComponents.insert(component);
        }
    }

    QList<Component*> autoDependOnList;
    if (allResolved) {
        // All regular dependees are resolved. Now we are looking for auto depend on components.
        foreach (Component *component, installedComponents) {
            // If a components is installed and not yet scheduled for un-installation, check for auto depend.
            if (component->isInstalled() && !m_componentsToUninstall.contains(component)) {
                QStringList autoDependencies = component->autoDependencies();
                if (autoDependencies.isEmpty())
                    continue;

                // This code needs to be enabled once the scripts use isInstalled, installationRequested and
                // uninstallationRequested...
                if (autoDependencies.first().compare(QLatin1String("script"), Qt::CaseInsensitive) == 0) {
                    //QScriptValue valueFromScript;
                    //try {
                    //    valueFromScript = callScriptMethod(QLatin1String("isAutoDependOn"));
                    //} catch (const Error &error) {
                    //    // keep the component, should do no harm
                    //    continue;
                    //}

                    //if (valueFromScript.isValid() && !valueFromScript.toBool())
                    //    autoDependOnList.append(component);
                    continue;
                }

                foreach (Component *c, installedComponents) {
                    const QString replaces = c->value(scReplaces);
                    const QStringList possibleNames = replaces.split(QInstaller::commaRegExp(),
                        QString::SkipEmptyParts) << c->name();
                    foreach (const QString &possibleName, possibleNames)
                        autoDependencies.removeAll(possibleName);
                }

                // A component requested auto installation, keep it to resolve their dependencies as well.
                if (!autoDependencies.isEmpty())
                    autoDependOnList.append(component);
            }
        }
    }

    if (!autoDependOnList.isEmpty())
        return appendComponentsToUninstall(autoDependOnList);
    return allResolved;
}

void PackageManagerCorePrivate::resetComponentsToUserCheckedState()
{
    if (m_coreCheckedHash.isEmpty())
        return;

    foreach (Component *component, m_coreCheckedHash.keys())
        component->setCheckState(m_coreCheckedHash.value(component));

    m_coreCheckedHash.clear();
    m_componentsToInstallCalculated = false;
}

void PackageManagerCorePrivate::setCheckedState(Component *component, Qt::CheckState state)
{
    m_coreCheckedHash.insert(component, component->checkState());
    component->setCheckState(state);
}

void PackageManagerCorePrivate::connectOperationCallMethodRequest(Operation *const operation)
{
    QObject *const operationObject = dynamic_cast<QObject *> (operation);
    if (operationObject != 0) {
        const QMetaObject *const mo = operationObject->metaObject();
        if (mo->indexOfSignal(QMetaObject::normalizedSignature("requestBlockingExecution(QString)")) > -1) {
            connect(operationObject, SIGNAL(requestBlockingExecution(QString)),
                    this, SLOT(handleMethodInvocationRequest(QString)), Qt::BlockingQueuedConnection);
        }
    }
}

OperationList PackageManagerCorePrivate::sortOperationsBasedOnComponentDependencies(const OperationList &operationList)
{
    OperationList sortedOperations;
    QHash<QString, OperationList> componentOperationHash;

    // sort component unrelated operations to the beginning
    foreach (Operation *operation, operationList) {
        const QString componentName = operation->value(QLatin1String("component")).toString();
        if (componentName.isEmpty())
            sortedOperations.append(operation);
        else {
            OperationList componentOperationList = componentOperationHash.value(componentName);
            componentOperationList.append(operation);
            componentOperationHash.insert(operation->value(QLatin1String("component")).toString(),
                componentOperationList);
        }
    }

    // create the complete component graph
    Graph<QString> componentGraph;
    const QRegExp dash(QLatin1String("-.*"));
    foreach (const Component* componentNode, m_core->availableComponents()) {
        componentGraph.addNode(componentNode->name());
        const QStringList dependencies = componentNode->dependencies().replaceInStrings(dash,QString());
        componentGraph.addEdges(componentNode->name(), dependencies);
    }

    const QStringList resolvedComponents = componentGraph.sort();
    if (componentGraph.hasCycle()) {
        throw Error(tr("Dependency cycle between components detected: '%1' and '%2'.")
            .arg(componentGraph.cycle().first, componentGraph.cycle().second));
    }
    foreach (const QString &componentName, resolvedComponents)
        sortedOperations.append(componentOperationHash.value(componentName));

    return sortedOperations;
}

void PackageManagerCorePrivate::handleMethodInvocationRequest(const QString &invokableMethodName)
{
    QObject *obj = QObject::sender();
    if (obj != 0)
        QMetaObject::invokeMethod(obj, qPrintable(invokableMethodName));
}

} // namespace QInstaller
