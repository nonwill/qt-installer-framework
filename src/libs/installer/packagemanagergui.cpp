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
#include "packagemanagergui.h"

#include "component.h"
#include "componentmodel.h"
#include "errors.h"
#include "fileutils.h"
#include "messageboxhandler.h"
#include "packagemanagercore.h"
#include "progresscoordinator.h"
#include "performinstallationform.h"
#include "settings.h"
#include "utils.h"
#include "scriptengine.h"

#include "kdsysinfo.h"

#include <QtCore/QDir>
#include <QtCore/QPair>
#include <QtCore/QProcess>
#include <QtCore/QSettings>
#include <QtCore/QTimer>

#include <QCheckBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QTextBrowser>
#include <QTreeView>
#include <QVBoxLayout>
#include <QShowEvent>

using namespace KDUpdater;
using namespace QInstaller;

/*
TRANSLATOR QInstaller::PackageManagerCore;
*/
/*
TRANSLATOR QInstaller::PackageManagerGui
*/
/*
TRANSLATOR QInstaller::PackageManagerPage
*/
/*
TRANSLATOR QInstaller::IntroductionPage
*/
/*
TRANSLATOR QInstaller::LicenseAgreementPage
*/
/*
TRANSLATOR QInstaller::ComponentSelectionPage
*/
/*
TRANSLATOR QInstaller::TargetDirectoryPage
*/
/*
TRANSLATOR QInstaller::StartMenuDirectoryPage
*/
/*
TRANSLATOR QInstaller::ReadyForInstallationPage
*/
/*
TRANSLATOR QInstaller::PerformInstallationPage
*/
/*
TRANSLATOR QInstaller::FinishedPage
*/

class DynamicInstallerPage : public PackageManagerPage
{
public:
    explicit DynamicInstallerPage(QWidget *widget, PackageManagerCore *core = 0)
        : PackageManagerPage(core)
        , m_widget(widget)
    {
        setObjectName(QLatin1String("Dynamic") + widget->objectName());
        setPixmap(QWizard::WatermarkPixmap, QPixmap());

        setLayout(new QVBoxLayout);
        setColoredSubTitle(QLatin1String(" "));
        setColoredTitle(widget->windowTitle());
        m_widget->setProperty("complete", true);
        m_widget->setProperty("final", false);
        widget->installEventFilter(this);
        layout()->addWidget(widget);
    }

    QWidget *widget() const
    {
        return m_widget;
    }

    bool isComplete() const
    {
        return m_widget->property("complete").toBool();
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event)
    {
        if (obj == m_widget) {
            switch(event->type()) {
            case QEvent::WindowTitleChange:
                setColoredTitle(m_widget->windowTitle());
                break;

            case QEvent::DynamicPropertyChange:
                emit completeChanged();
                if (m_widget->property("final").toBool() != isFinalPage())
                    setFinalPage(m_widget->property("final").toBool());
                break;

            default:
                break;
            }
        }
        return PackageManagerPage::eventFilter(obj, event);
    }

private:
    QWidget *const m_widget;
};


// -- PackageManagerGui::Private

class PackageManagerGui::Private
{
public:
    Private()
        : m_modified(false)
        , m_autoSwitchPage(true)
        , m_showSettingsButton(false)
    {
        m_wizardButtonTypes.insert(QWizard::BackButton, QLatin1String("QWizard::BackButton"));
        m_wizardButtonTypes.insert(QWizard::NextButton, QLatin1String("QWizard::NextButton"));
        m_wizardButtonTypes.insert(QWizard::CommitButton, QLatin1String("QWizard::CommitButton"));
        m_wizardButtonTypes.insert(QWizard::FinishButton, QLatin1String("QWizard::FinishButton"));
        m_wizardButtonTypes.insert(QWizard::CancelButton, QLatin1String("QWizard::CancelButton"));
        m_wizardButtonTypes.insert(QWizard::HelpButton, QLatin1String("QWizard::HelpButton"));
        m_wizardButtonTypes.insert(QWizard::CustomButton1, QLatin1String("QWizard::CustomButton1"));
        m_wizardButtonTypes.insert(QWizard::CustomButton2, QLatin1String("QWizard::CustomButton2"));
        m_wizardButtonTypes.insert(QWizard::CustomButton3, QLatin1String("QWizard::CustomButton3"));
        m_wizardButtonTypes.insert(QWizard::Stretch, QLatin1String("QWizard::Stretch"));
    }

    QString buttonType(int wizardButton)
    {
        return m_wizardButtonTypes.value(static_cast<QWizard::WizardButton>(wizardButton),
            QLatin1String("unknown button"));
    }


    bool m_modified;
    bool m_autoSwitchPage;
    bool m_showSettingsButton;
    QHash<int, QWizardPage*> m_defaultPages;
    QHash<int, QString> m_defaultButtonText;

    QScriptValue m_controlScriptContext;
    QHash<QWizard::WizardButton, QString> m_wizardButtonTypes;
};


// -- PackageManagerGui

/*!
    \class QInstaller::PackageManagerGui
    Is the "gui" object in a none interactive installation
*/
PackageManagerGui::PackageManagerGui(PackageManagerCore *core, QWidget *parent)
    : QWizard(parent)
    , d(new Private)
    , m_core(core)
{
    if (m_core->isInstaller())
        setWindowTitle(tr("%1 Setup").arg(m_core->value(scTitle)));
    else
        setWindowTitle(tr("Maintain %1").arg(m_core->value(scTitle)));
    setWindowFlags(windowFlags() &~ Qt::WindowContextHelpButtonHint);

#ifndef Q_OS_MAC
    setWizardStyle(QWizard::ModernStyle);
    setSizeGripEnabled(true);
    setWindowIcon(QIcon(m_core->settings().installerWindowIcon()));
#else
    setPixmap(QWizard::BackgroundPixmap, m_core->settings().background());
#endif

    if (!m_core->settings().wizardStyle().isEmpty())
        setWizardStyle(getStyle(m_core->settings().wizardStyle()));

    setOption(QWizard::NoBackButtonOnStartPage);
    setOption(QWizard::NoBackButtonOnLastPage);

    connect(this, SIGNAL(rejected()), m_core, SLOT(setCanceled()));
    connect(this, SIGNAL(interrupted()), m_core, SLOT(interrupt()));

    // both queued to show the finished page once everything is done
    connect(m_core, SIGNAL(installationFinished()), this, SLOT(showFinishedPage()),
        Qt::QueuedConnection);
    connect(m_core, SIGNAL(uninstallationFinished()), this, SLOT(showFinishedPage()),
        Qt::QueuedConnection);

    connect(this, SIGNAL(currentIdChanged(int)), this, SLOT(executeControlScript(int)));
    connect(this, SIGNAL(currentIdChanged(int)), m_core, SIGNAL(currentPageChanged(int)));
    connect(button(QWizard::FinishButton), SIGNAL(clicked()), this, SIGNAL(finishButtonClicked()));
    connect(button(QWizard::FinishButton), SIGNAL(clicked()), m_core, SIGNAL(finishButtonClicked()));

    // make sure the QUiLoader's retranslateUi is executed first, then the script
    connect(this, SIGNAL(languageChanged()), m_core, SLOT(languageChanged()), Qt::QueuedConnection);
    connect(this, SIGNAL(languageChanged()), this, SLOT(onLanguageChanged()), Qt::QueuedConnection);

    connect(m_core,
        SIGNAL(wizardPageInsertionRequested(QWidget*,QInstaller::PackageManagerCore::WizardPage)),
        this, SLOT(wizardPageInsertionRequested(QWidget*,QInstaller::PackageManagerCore::WizardPage)));
    connect(m_core, SIGNAL(wizardPageRemovalRequested(QWidget*)), this,
        SLOT(wizardPageRemovalRequested(QWidget*)));
    connect(m_core,
        SIGNAL(wizardWidgetInsertionRequested(QWidget*,QInstaller::PackageManagerCore::WizardPage)),
        this, SLOT(wizardWidgetInsertionRequested(QWidget*,QInstaller::PackageManagerCore::WizardPage)));
    connect(m_core, SIGNAL(wizardWidgetRemovalRequested(QWidget*)), this,
        SLOT(wizardWidgetRemovalRequested(QWidget*)));
    connect(m_core, SIGNAL(wizardPageVisibilityChangeRequested(bool,int)), this,
        SLOT(wizardPageVisibilityChangeRequested(bool,int)), Qt::QueuedConnection);

    connect(m_core,
        SIGNAL(setValidatorForCustomPageRequested(QInstaller::Component*,QString,QString)), this,
        SLOT(setValidatorForCustomPageRequested(QInstaller::Component*,QString,QString)));

    connect(m_core, SIGNAL(setAutomatedPageSwitchEnabled(bool)), this,
        SLOT(setAutomatedPageSwitchEnabled(bool)));

    connect(this, SIGNAL(customButtonClicked(int)), this, SLOT(customButtonClicked(int)));

    for (int i = QWizard::BackButton; i < QWizard::CustomButton1; ++i)
        d->m_defaultButtonText.insert(i, buttonText(QWizard::WizardButton(i)));

    m_core->setGuiObject(this);

    setMinimumWidth(height() * 1.2);
}

PackageManagerGui::~PackageManagerGui()
{
    delete d;
}

QWizard::WizardStyle PackageManagerGui::getStyle(const QString &name)
{
    if (name == QLatin1String("Classic"))
        return QWizard::ClassicStyle;

    if (name == QLatin1String("Modern"))
        return QWizard::ModernStyle;

    if (name == QLatin1String("Mac"))
        return QWizard::MacStyle;

    if (name == QLatin1String("Aero"))
        return QWizard::AeroStyle;
    return QWizard::ModernStyle;
}

void PackageManagerGui::setAutomatedPageSwitchEnabled(bool request)
{
    d->m_autoSwitchPage = request;
}

QString PackageManagerGui::defaultButtonText(int wizardButton) const
{
    return d->m_defaultButtonText.value(wizardButton);
}

/*
    Check if we need to "transform" the finish button into a cancel button, caused by the misuse of
    cancel as the finish button on the FinishedPage. This is only a problem if we run as updater or
    package manager, as then there will be two button shown on the last page with the cancel button
    renamed to "Finish".
*/
static bool swapFinishButton(PackageManagerCore *core, int currentId, int button)
{
    if (button != QWizard::FinishButton)
        return false;

    if (currentId != PackageManagerCore::InstallationFinished)
        return false;

    if (core->isInstaller() || core->isUninstaller())
        return false;

    return true;
}

void PackageManagerGui::clickButton(int wb, int delay)
{
    // We need to to swap here, cause scripts expect to call this function with FinishButton on the
    // finish page.
    if (swapFinishButton(m_core, currentId(), wb))
        wb = QWizard::CancelButton;

    if (QAbstractButton *b = button(static_cast<QWizard::WizardButton>(wb)))
        QTimer::singleShot(delay, b, SLOT(click()));
    else
        qWarning() << "Button with type: " << d->buttonType(wb) << "not found!";
}

bool PackageManagerGui::isButtonEnabled(int wb)
{
    // We need to to swap here, cause scripts expect to call this function with FinishButton on the
    // finish page.
    if (swapFinishButton(m_core, currentId(), wb))
            wb = QWizard::CancelButton;

    if (QAbstractButton *b = button(static_cast<QWizard::WizardButton>(wb)))
        return b->isEnabled();

    qWarning() << "Button with type: " << d->buttonType(wb) << "not found!";
    return false;
}

void PackageManagerGui::setValidatorForCustomPageRequested(Component *component,
    const QString &name, const QString &callbackName)
{
    component->setValidatorCallbackName(callbackName);

    const QString componentName = QLatin1String("Dynamic") + name;
    const QList<int> ids = pageIds();
    foreach (const int i, ids) {
        PackageManagerPage *const p = qobject_cast<PackageManagerPage*> (page(i));
        if (p && p->objectName() == componentName) {
            p->setValidatePageComponent(component);
            return;
        }
    }
}

/*!
    Loads a script to perform the installation non-interactively.
    @throws QInstaller::Error if the script is not readable/cannot be parsed
*/
void PackageManagerGui::loadControlScript(const QString &scriptPath)
{
    d->m_controlScriptContext = m_core->controlScriptEngine()->loadInConext(
        QLatin1String("Controller"), scriptPath);
    qDebug() << "Loaded control script" << scriptPath;
}

void PackageManagerGui::callControlScriptMethod(const QString &methodName)
{
    if (!d->m_controlScriptContext.isValid())
        return;
    try {
        QScriptValue returnValue = m_core->controlScriptEngine()->callScriptMethod(
            d->m_controlScriptContext, methodName);

        if (!returnValue.isValid()) {
            qDebug() << "Control script callback" << methodName << "does not exist.";
            return;
        }
    } catch (const QInstaller::Error &e) {
        qCritical() << qPrintable(e.message());
    }
}

void PackageManagerGui::executeControlScript(int pageId)
{
    if (PackageManagerPage *const p = qobject_cast<PackageManagerPage*> (page(pageId)))
        callControlScriptMethod(p->objectName() + QLatin1String("Callback"));
}

void PackageManagerGui::onLanguageChanged()
{
    d->m_defaultButtonText.clear();
    for (int i = QWizard::BackButton; i < QWizard::CustomButton1; ++i)
        d->m_defaultButtonText.insert(i, buttonText(QWizard::WizardButton(i)));
}

bool PackageManagerGui::event(QEvent *event)
{
    switch(event->type()) {
    case QEvent::LanguageChange:
        emit languageChanged();
        break;
    default:
        break;
    }
    return QWizard::event(event);
}

void PackageManagerGui::showEvent(QShowEvent *event)
{
    if (!event->spontaneous()) {
        foreach (int id, pageIds()) {
            const QString subTitle = page(id)->subTitle();
            if (subTitle.isEmpty()) {
                const QWizard::WizardStyle style = wizardStyle();
                if ((style == QWizard::ClassicStyle) || (style == QWizard::ModernStyle)) {
                    // otherwise the colors might screw up
                    page(id)->setSubTitle(QLatin1String(" "));
                }
            }
        }
    }
    QWizard::showEvent(event);
    QMetaObject::invokeMethod(this, "dependsOnLocalInstallerBinary", Qt::QueuedConnection);
}

void PackageManagerGui::wizardPageInsertionRequested(QWidget *widget,
    QInstaller::PackageManagerCore::WizardPage page)
{
    // just in case it was already in there...
    wizardPageRemovalRequested(widget);

    int pageId = static_cast<int>(page) - 1;
    while (QWizard::page(pageId) != 0)
        --pageId;

    // add it
    setPage(pageId, new DynamicInstallerPage(widget, m_core));
}

void PackageManagerGui::wizardPageRemovalRequested(QWidget *widget)
{
    foreach (int pageId, pageIds()) {
        DynamicInstallerPage *const dynamicPage = dynamic_cast<DynamicInstallerPage*>(page(pageId));
        if (dynamicPage == 0)
            continue;
        if (dynamicPage->widget() != widget)
            continue;
        removePage(pageId);
        d->m_defaultPages.remove(pageId);
    }
}

void PackageManagerGui::wizardWidgetInsertionRequested(QWidget *widget,
    QInstaller::PackageManagerCore::WizardPage page)
{
    Q_ASSERT(widget);
    if (QWizardPage *const p = QWizard::page(page))
        p->layout()->addWidget(widget);
}

void PackageManagerGui::wizardWidgetRemovalRequested(QWidget *widget)
{
    Q_ASSERT(widget);
    widget->setParent(0);
}

void PackageManagerGui::wizardPageVisibilityChangeRequested(bool visible, int p)
{
    if (visible && page(p) == 0) {
        setPage(p, d->m_defaultPages[p]);
    } else if (!visible && page(p) != 0) {
        d->m_defaultPages[p] = page(p);
        removePage(p);
    }
}

PackageManagerPage *PackageManagerGui::page(int pageId) const
{
    return qobject_cast<PackageManagerPage*> (QWizard::page(pageId));
}

QWidget *PackageManagerGui::pageWidgetByObjectName(const QString &name) const
{
    const QList<int> ids = pageIds();
    foreach (const int i, ids) {
        PackageManagerPage *const p = qobject_cast<PackageManagerPage*> (page(i));
        if (p && p->objectName() == name) {
            // For dynamic pages, return the contained widget (as read from the UI file), not the
            // wrapper page
            if (DynamicInstallerPage *dp = dynamic_cast<DynamicInstallerPage*>(p))
                return dp->widget();
            return p;
        }
    }
    qDebug() << "No page found for object name" << name;
    return 0;
}

QWidget *PackageManagerGui::currentPageWidget() const
{
    return currentPage();
}

void PackageManagerGui::cancelButtonClicked()
{
    const int id = currentId();
    if (id == PackageManagerCore::Introduction || id == PackageManagerCore::InstallationFinished) {
        m_core->setNeedsHardRestart(false);
        QDialog::reject(); return;
    }

    QString question;
    bool interrupt = false;
    PackageManagerPage *const page = qobject_cast<PackageManagerPage*> (currentPage());
    if (page && page->isInterruptible()
        && m_core->status() != PackageManagerCore::Canceled
        && m_core->status() != PackageManagerCore::Failure) {
            interrupt = true;
            question = tr("Do you want to cancel the installation process?");
            if (m_core->isUninstaller())
                question = tr("Do you want to cancel the uninstallation process?");
    } else {
        question = tr("Do you want to quit the installer application?");
        if (m_core->isUninstaller())
            question = tr("Do you want to quit the uninstaller application?");
        if (m_core->isUpdater() || m_core->isPackageManager())
            question = tr("Do you want to quit the maintenance application?");
    }

    const QMessageBox::StandardButton button =
        MessageBoxHandler::question(MessageBoxHandler::currentBestSuitParent(),
        QLatin1String("cancelInstallation"), tr("Question"), question,
        QMessageBox::Yes | QMessageBox::No);

    if (button == QMessageBox::Yes) {
        if (interrupt)
            emit interrupted();
        else
            QDialog::reject();
    }
}

void PackageManagerGui::rejectWithoutPrompt()
{
    QDialog::reject();
}

void PackageManagerGui::reject()
{
    cancelButtonClicked();
}

void PackageManagerGui::setModified(bool value)
{
    d->m_modified = value;
}

void PackageManagerGui::showFinishedPage()
{
    qDebug() << "SHOW FINISHED PAGE";
    if (d->m_autoSwitchPage)
        next();
    else
        qobject_cast<QPushButton*>(button(QWizard::CancelButton))->setEnabled(false);
}

void PackageManagerGui::showSettingsButton(bool show)
{
    if (d->m_showSettingsButton == show)
        return;

    d->m_showSettingsButton = show;
    setOption(QWizard::HaveCustomButton1, show);
    setButtonText(QWizard::CustomButton1, tr("Settings"));

    updateButtonLayout();
}

/*!
    Force an update of our own button layout, needs to be called whenever a button option has been
    set.
*/
void PackageManagerGui::updateButtonLayout()
{
    QVector<QWizard::WizardButton> buttons(12, QWizard::NoButton);
    if (options() & QWizard::HaveHelpButton)
        buttons[(options() & QWizard::HelpButtonOnRight) ? 11 : 0] = QWizard::HelpButton;

    buttons[1] = QWizard::Stretch;
    if (options() & QWizard::HaveCustomButton1) {
        buttons[1] = QWizard::CustomButton1;
        buttons[2] = QWizard::Stretch;
    }

    if (options() & QWizard::HaveCustomButton2)
        buttons[3] = QWizard::CustomButton2;

    if (options() & QWizard::HaveCustomButton3)
        buttons[4] = QWizard::CustomButton3;

    if (!(options() & QWizard::NoCancelButton))
        buttons[(options() & QWizard::CancelButtonOnLeft) ? 5 : 10] = QWizard::CancelButton;

    buttons[6] = QWizard::BackButton;
    buttons[7] = QWizard::NextButton;
    buttons[8] = QWizard::CommitButton;
    buttons[9] = QWizard::FinishButton;

    setOption(QWizard::NoBackButtonOnLastPage, true);
    setOption(QWizard::NoBackButtonOnStartPage, true);

    setButtonLayout(buttons.toList());
}

void PackageManagerGui::setSettingsButtonEnabled(bool enabled)
{
    if (QAbstractButton *btn = button(QWizard::CustomButton1))
        btn->setEnabled(enabled);
}

void PackageManagerGui::customButtonClicked(int which)
{
    if (QWizard::WizardButton(which) == QWizard::CustomButton1 && d->m_showSettingsButton)
        emit settingsButtonClicked();
}

void PackageManagerGui::dependsOnLocalInstallerBinary()
{
    if (m_core->settings().dependsOnLocalInstallerBinary() && !m_core->localInstallerBinaryUsed()) {
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("Installer_Needs_To_Be_Local_Error"), tr("Error"),
            tr("It is not possible to install from network location.\n"
               "Please copy the installer to a local drive"), QMessageBox::Ok);
        rejectWithoutPrompt();
    }
}


// -- PackageManagerPage

PackageManagerPage::PackageManagerPage(PackageManagerCore *core)
    : m_fresh(true)
    , m_complete(true)
    , m_needsSettingsButton(false)
    , m_core(core)
    , validatorComponent(0)
{
    if (!m_core->settings().titleColor().isEmpty())
        m_titleColor = m_core->settings().titleColor();
    else {
        QColor defaultColor = style()->standardPalette().text().color();
        m_titleColor = defaultColor.name();
    }
    setPixmap(QWizard::WatermarkPixmap, watermarkPixmap());
    setPixmap(QWizard::BannerPixmap, bannerPixmap());
    setPixmap(QWizard::LogoPixmap, logoPixmap());
}

PackageManagerCore *PackageManagerPage::packageManagerCore() const
{
    return m_core;
}

QPixmap PackageManagerPage::watermarkPixmap() const
{
    return QPixmap(m_core->value(QLatin1String("WatermarkPixmap")));
}

QPixmap PackageManagerPage::bannerPixmap() const
{
    return QPixmap(m_core->value(QLatin1String("BannerPixmap")));
}

QPixmap PackageManagerPage::logoPixmap() const
{
    return QPixmap(m_core->value(QLatin1String("LogoPixmap")));
}

QString PackageManagerPage::productName() const
{
    return m_core->value(QLatin1String("ProductName"));
}

void PackageManagerPage::setColoredTitle(const QString &title)
{
    setTitle(QString::fromLatin1("<font color=\"%1\">%2</font>").arg(m_titleColor, title));
}

void PackageManagerPage::setColoredSubTitle(const QString &subTitle)
{
    setSubTitle(QString::fromLatin1("<font color=\"%1\">%2</font>").arg(m_titleColor, subTitle));
}

bool PackageManagerPage::isComplete() const
{
    return m_complete;
}

void PackageManagerPage::setComplete(bool complete)
{
    m_complete = complete;
    if (QWizard *w = wizard()) {
        if (QAbstractButton *cancel = w->button(QWizard::CancelButton)) {
            if (cancel->hasFocus()) {
                if (QAbstractButton *next = w->button(QWizard::NextButton))
                    next->setFocus();
            }
        }
    }
    emit completeChanged();
}

void PackageManagerPage::setValidatePageComponent(Component *component)
{
    validatorComponent = component;
}

bool PackageManagerPage::validatePage()
{
    if (validatorComponent)
        return validatorComponent->validatePage();
    return true;
}

void PackageManagerPage::insertWidget(QWidget *widget, const QString &siblingName, int offset)
{
    QWidget *sibling = findChild<QWidget *>(siblingName);
    QWidget *parent = sibling ? sibling->parentWidget() : 0;
    QLayout *layout = parent ? parent->layout() : 0;
    QBoxLayout *blayout = qobject_cast<QBoxLayout *>(layout);

    if (blayout) {
        const int index = blayout->indexOf(sibling) + offset;
        blayout->insertWidget(index, widget);
    }
}

QWidget *PackageManagerPage::findWidget(const QString &objectName) const
{
    return findChild<QWidget*> (objectName);
}

/*!
    \reimp
    \Overwritten to support some kind of initializePage() in the case the wizard has been set
    to QWizard::IndependentPages. If that option has been set, initializePage() would be only
    called once. So we provide entering() and leaving() based on this overwritten function.
*/
void PackageManagerPage::setVisible(bool visible)
{
    QWizardPage::setVisible(visible);
    if (m_fresh && !visible) {
        // this is only hit once when the page gets added to the wizard
        m_fresh = false;
        return;
    }

    if (visible) {
        entering();
        emit entered();
    } else {
        leaving();
        emit left();
    }
}

int PackageManagerPage::nextId() const
{
    const int next = QWizardPage::nextId(); // the page to show next
    if (next == PackageManagerCore::LicenseCheck) {
        // calculate the page after the license page
        const int nextNextId = gui()->pageIds().value(gui()->pageIds().indexOf(next) + 1, -1);
        const PackageManagerCore *const core = packageManagerCore();
        if (core->isUninstaller())
            return nextNextId;  // forcibly hide the license page if we run as uninstaller

        core->calculateComponentsToInstall();
        foreach (Component* component, core->orderedComponentsToInstall()) {
            if ((core->isPackageManager() || core->isUpdater()) && component->isInstalled())
                continue; // package manager or updater, hide as long as the component is installed

            // The component is about to be installed and provides a license, so the page needs to
            // be shown.
            if (!component->licenses().isEmpty())
                return next;
        }
        return nextNextId;  // no component with a license or all components with license installed
    }
    return next;    // default, show the next page
}

// -- IntroductionPage

IntroductionPage::IntroductionPage(PackageManagerCore *core)
    : PackageManagerPage(core)
    , m_widget(0)
{
    setObjectName(QLatin1String("IntroductionPage"));
    setColoredTitle(tr("Setup - %1").arg(productName()));

    QVBoxLayout *layout = new QVBoxLayout(this);

    m_msgLabel = new QLabel(this);
    m_msgLabel->setWordWrap(true);
    m_msgLabel->setObjectName(QLatin1String("MessageLabel"));
    m_msgLabel->setText(tr("Welcome to the %1 Setup Wizard.").arg(productName()));
    layout->addWidget(m_msgLabel);

    layout->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

void IntroductionPage::setWidget(QWidget *widget)
{
    if (m_widget) {
        layout()->removeWidget(m_widget);
        delete m_widget;
    }
    m_widget = widget;
    if (m_widget)
        static_cast<QVBoxLayout*>(layout())->addWidget(m_widget, 1);
}

void IntroductionPage::setText(const QString &text)
{
    m_msgLabel->setText(text);
}


// -- LicenseAgreementPage::ClickForwarder

class LicenseAgreementPage::ClickForwarder : public QObject
{
    Q_OBJECT

public:
    explicit ClickForwarder(QAbstractButton *button)
        : QObject(button)
        , m_abstractButton(button) {}

protected:
    bool eventFilter(QObject *object, QEvent *event)
    {
        if (event->type() == QEvent::MouseButtonRelease) {
            m_abstractButton->click();
            return true;
        }
        // standard event processing
        return QObject::eventFilter(object, event);
    }
private:
    QAbstractButton *m_abstractButton;
};


// -- LicenseAgreementPage

LicenseAgreementPage::LicenseAgreementPage(PackageManagerCore *core)
    : PackageManagerPage(core)
{
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("LicenseAgreementPage"));
    setColoredTitle(tr("License Agreement"));

    QVBoxLayout *layout = new QVBoxLayout(this);

    QHBoxLayout *licenseBoxLayout = new QHBoxLayout();
    layout->addLayout(licenseBoxLayout);

    m_licenseListWidget = new QListWidget(this);
    m_licenseListWidget->setObjectName(QLatin1String("LicenseListWidget"));
    m_licenseListWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);
    connect(m_licenseListWidget, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
        this, SLOT(currentItemChanged(QListWidgetItem*)));
    licenseBoxLayout->addWidget(m_licenseListWidget);

    m_textBrowser = new QTextBrowser(this);
    m_textBrowser->setOpenLinks(false);
    m_textBrowser->setOpenExternalLinks(true);
    m_textBrowser->setObjectName(QLatin1String("LicenseTextBrowser"));
    m_textBrowser->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    connect(m_textBrowser, SIGNAL(anchorClicked(QUrl)), this, SLOT(openLicenseUrl(QUrl)));

    licenseBoxLayout->addWidget(m_textBrowser);

    QGridLayout *gridLayout = new QGridLayout;
    layout->addLayout(gridLayout);

    m_acceptRadioButton = new QRadioButton(this);
    m_acceptRadioButton->setShortcut(QKeySequence(tr("Alt+A", "agree license")));
    m_acceptRadioButton->setObjectName(QLatin1String("AcceptLicenseRadioButton"));
    ClickForwarder *acceptClickForwarder = new ClickForwarder(m_acceptRadioButton);
    gridLayout->addWidget(m_acceptRadioButton, 0, 0);

    m_acceptLabel = new QLabel;
    m_acceptLabel->setWordWrap(true);
    m_acceptLabel->installEventFilter(acceptClickForwarder);
    m_acceptLabel->setObjectName(QLatin1String("AcceptLicenseLabel"));
    m_acceptLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    gridLayout->addWidget(m_acceptLabel, 0, 1);

    m_rejectRadioButton = new QRadioButton(this);
    ClickForwarder *rejectClickForwarder = new ClickForwarder(m_rejectRadioButton);
    m_rejectRadioButton->setObjectName(QString::fromUtf8("RejectLicenseRadioButton"));
    m_rejectRadioButton->setShortcut(QKeySequence(tr("Alt+D", "do not agree license")));
    gridLayout->addWidget(m_rejectRadioButton, 1, 0);

    m_rejectLabel = new QLabel;
    m_rejectLabel->setWordWrap(true);
    m_rejectLabel->installEventFilter(rejectClickForwarder);
    m_rejectLabel->setObjectName(QLatin1String("RejectLicenseLabel"));
    m_rejectLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    gridLayout->addWidget(m_rejectLabel, 1, 1);

    gridLayout->setColumnStretch(1, 1);

    connect(m_acceptRadioButton, SIGNAL(toggled(bool)), this, SIGNAL(completeChanged()));
    connect(m_rejectRadioButton, SIGNAL(toggled(bool)), this, SIGNAL(completeChanged()));

    m_rejectRadioButton->setChecked(true);
}

void LicenseAgreementPage::entering()
{
    m_licenseListWidget->clear();
    m_textBrowser->setText(QString());
    m_licenseListWidget->setVisible(false);

    packageManagerCore()->calculateComponentsToInstall();
    foreach (QInstaller::Component *component, packageManagerCore()->orderedComponentsToInstall())
        addLicenseItem(component->licenses());

    const int licenseCount = m_licenseListWidget->count();
    if (licenseCount > 0) {
        m_licenseListWidget->setVisible(licenseCount > 1);
        m_licenseListWidget->setCurrentItem(m_licenseListWidget->item(0));
    }

    updateUi();
}

bool LicenseAgreementPage::isComplete() const
{
    return m_acceptRadioButton->isChecked();
}

void LicenseAgreementPage::openLicenseUrl(const QUrl &url)
{
    QDesktopServices::openUrl(url);
}

void LicenseAgreementPage::currentItemChanged(QListWidgetItem *current)
{
    if (current)
        m_textBrowser->setHtml(current->data(Qt::UserRole).toString());
}

void LicenseAgreementPage::addLicenseItem(const QHash<QString, QPair<QString, QString> > &hash)
{
    for (QHash<QString, QPair<QString, QString> >::const_iterator it = hash.begin();
        it != hash.end(); ++it) {
            QListWidgetItem *item = new QListWidgetItem(it.key(), m_licenseListWidget);
            item->setData(Qt::UserRole, it.value().second);
    }
}

void LicenseAgreementPage::updateUi()
{
    QString subTitleText;
    QString acceptButtonText;
    QString rejectButtonText;
    if (m_licenseListWidget->count() == 1) {
        subTitleText = tr("Please read the following license agreement. You must accept the terms "
                          "contained in this agreement before continuing with the installation.");
        acceptButtonText = tr("I accept the license.");
        rejectButtonText = tr("I do not accept the license.");
    } else {
        subTitleText = tr("Please read the following license agreements. You must accept the terms "
                          "contained in these agreements before continuing with the installation.");
        acceptButtonText = tr("I accept the licenses.");
        rejectButtonText = tr("I do not accept the licenses.");
    }

    setColoredSubTitle(subTitleText);

    m_acceptLabel->setText(acceptButtonText);
    m_rejectLabel->setText(rejectButtonText);

}


// -- ComponentSelectionPage::Private

class ComponentSelectionPage::Private : public QObject
{
    Q_OBJECT

public:
    Private(ComponentSelectionPage *qq, PackageManagerCore *core)
        : q(qq)
        , m_core(core)
        , m_treeView(new QTreeView(q))
        , m_allModel(m_core->defaultComponentModel())
        , m_updaterModel(m_core->updaterComponentModel())
        , m_currentModel(m_allModel)
    {
        m_treeView->setObjectName(QLatin1String("ComponentsTreeView"));

        connect(m_allModel, SIGNAL(checkStateChanged(QInstaller::ComponentModel::ModelState)), this,
            SLOT(onModelStateChanged(QInstaller::ComponentModel::ModelState)));
        connect(m_updaterModel, SIGNAL(checkStateChanged(QInstaller::ComponentModel::ModelState)),
            this, SLOT(onModelStateChanged(QInstaller::ComponentModel::ModelState)));

        QVBoxLayout *layout = new QVBoxLayout(q);

        QHBoxLayout *hlayout = new QHBoxLayout;
        layout->addLayout(hlayout, 1);

        hlayout->addWidget(m_treeView, 3);

        m_descriptionLabel = new QLabel(q);
        m_descriptionLabel->setWordWrap(true);
        m_descriptionLabel->setObjectName(QLatin1String("ComponentDescriptionLabel"));

        QVBoxLayout *vlayout = new QVBoxLayout;
        hlayout->addLayout(vlayout, 2);

        vlayout->addWidget(m_descriptionLabel);

        m_sizeLabel = new QLabel(q);
        m_sizeLabel->setWordWrap(true);
        vlayout->addWidget(m_sizeLabel);
        m_sizeLabel->setObjectName(QLatin1String("ComponentSizeLabel"));

        vlayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::MinimumExpanding,
            QSizePolicy::MinimumExpanding));


        m_checkDefault = new QPushButton;
        connect(m_checkDefault, SIGNAL(clicked()), this, SLOT(selectDefault()));
        if (m_core->isInstaller()) {
            m_checkDefault->setObjectName(QLatin1String("SelectDefaultComponentsButton"));
            m_checkDefault->setShortcut(QKeySequence(ComponentSelectionPage::tr("Alt+A",
                "select default components")));
            m_checkDefault->setText(ComponentSelectionPage::tr("Def&ault"));
        } else {
            m_checkDefault->setEnabled(false);
            m_checkDefault->setObjectName(QLatin1String("ResetComponentsButton"));
            m_checkDefault->setShortcut(QKeySequence(ComponentSelectionPage::tr("Alt+R",
                "reset to already installed components")));
            m_checkDefault->setText(ComponentSelectionPage::tr("&Reset"));
        }
        hlayout = new QHBoxLayout;
        layout->addLayout(hlayout);

        hlayout->addWidget(m_checkDefault);

        m_checkAll = new QPushButton;
        hlayout->addWidget(m_checkAll);
        connect(m_checkAll, SIGNAL(clicked()), this, SLOT(selectAll()));
        m_checkAll->setObjectName(QLatin1String("SelectAllComponentsButton"));
        m_checkAll->setShortcut(QKeySequence(ComponentSelectionPage::tr("Alt+S",
            "select all components")));
        m_checkAll->setText(ComponentSelectionPage::tr("&Select All"));

        m_uncheckAll = new QPushButton;
        hlayout->addWidget(m_uncheckAll);
        connect(m_uncheckAll, SIGNAL(clicked()), this, SLOT(deselectAll()));
        m_uncheckAll->setObjectName(QLatin1String("DeselectAllComponentsButton"));
        m_uncheckAll->setShortcut(QKeySequence(ComponentSelectionPage::tr("Alt+D",
            "deselect all components")));
        m_uncheckAll->setText(ComponentSelectionPage::tr("&Deselect All"));

        hlayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::MinimumExpanding,
            QSizePolicy::MinimumExpanding));
    }

    void updateTreeView()
    {
        m_checkDefault->setVisible(m_core->isInstaller() || m_core->isPackageManager());
        if (m_treeView->selectionModel()) {
            disconnect(m_treeView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
                this, SLOT(currentSelectedChanged(QModelIndex)));
        }

        m_currentModel = m_core->isUpdater() ? m_updaterModel : m_allModel;
        m_treeView->setModel(m_currentModel);
        m_treeView->setExpanded(m_currentModel->index(0, 0), true);

        if (m_core->isInstaller()) {
            m_treeView->setHeaderHidden(true);
            for (int i = 1; i < m_currentModel->columnCount(); ++i)
                m_treeView->hideColumn(i);
        } else {
            m_treeView->header()->setStretchLastSection(true);
            for (int i = 0; i < m_currentModel->columnCount(); ++i)
                m_treeView->resizeColumnToContents(i);
        }

        bool hasChildren = false;
        const int rowCount = m_currentModel->rowCount();
        for (int row = 0; row < rowCount && !hasChildren; ++row)
            hasChildren = m_currentModel->hasChildren(m_currentModel->index(row, 0));
        m_treeView->setRootIsDecorated(hasChildren);

        connect(m_treeView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            this, SLOT(currentSelectedChanged(QModelIndex)));

        m_treeView->setCurrentIndex(m_currentModel->index(0, 0));
    }

public slots:
    void currentSelectedChanged(const QModelIndex &current)
    {
        if (!current.isValid())
            return;

        m_sizeLabel->setText(QString());
        m_descriptionLabel->setText(m_currentModel->data(m_currentModel->index(current.row(),
            ComponentModelHelper::NameColumn, current.parent()), Qt::ToolTipRole).toString());

        Component *component = m_currentModel->componentFromIndex(current);
        if ((m_core->isUninstaller()) || (!component))
            return;

        if (component->isSelected() && (component->value(scUncompressedSizeSum).toLongLong() > 0)) {
            m_sizeLabel->setText(ComponentSelectionPage::tr("This component "
                "will occupy approximately %1 on your hard disk drive.")
                .arg(humanReadableSize(component->value(scUncompressedSizeSum).toLongLong())));
        }
    }

    void selectAll()
    {
        m_currentModel->setCheckedState(ComponentModel::AllChecked);
    }

    void deselectAll()
    {
        m_currentModel->setCheckedState(ComponentModel::AllUnchecked);
    }

    void selectDefault()
    {
        m_currentModel->setCheckedState(ComponentModel::DefaultChecked);
    }

    void onModelStateChanged(QInstaller::ComponentModel::ModelState state)
    {
        q->setModified(state.testFlag(ComponentModel::DefaultChecked) == false);
        // If all components in the checked list are only checkable when run without forced
        // installation, set ComponentModel::AllUnchecked as well, as we cannot uncheck anything.
        // Helps to keep the UI correct.
        if ((!m_core->noForceInstallation())
            && (m_currentModel->checked() == m_currentModel->uncheckable())) {
                state |= ComponentModel::AllUnchecked;
        }
        // enable the button if the corresponding flag is not set
        m_checkAll->setEnabled(state.testFlag(ComponentModel::AllChecked) == false);
        m_uncheckAll->setEnabled(state.testFlag(ComponentModel::AllUnchecked) == false);
        m_checkDefault->setEnabled(state.testFlag(ComponentModel::DefaultChecked) == false);

        // update the current selected node (important to reflect possible sub-node changes)
        if (m_treeView->selectionModel())
            currentSelectedChanged(m_treeView->selectionModel()->currentIndex());
    }

public:
    ComponentSelectionPage *q;
    PackageManagerCore *m_core;
    QTreeView *m_treeView;
    ComponentModel *m_allModel;
    ComponentModel *m_updaterModel;
    ComponentModel *m_currentModel;
    QLabel *m_sizeLabel;
    QLabel *m_descriptionLabel;
    QPushButton *m_checkAll;
    QPushButton *m_uncheckAll;
    QPushButton *m_checkDefault;
};


// -- ComponentSelectionPage

/*!
    \class QInstaller::ComponentSelectionPage
    On this page the user can select and deselect what he wants to be installed.
*/
ComponentSelectionPage::ComponentSelectionPage(PackageManagerCore *core)
    : PackageManagerPage(core)
    , d(new Private(this, core))
{
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("ComponentSelectionPage"));
    setColoredTitle(tr("Select Components"));
}

ComponentSelectionPage::~ComponentSelectionPage()
{
    delete d;
}

void ComponentSelectionPage::entering()
{
    static const char *strings[] = {
        QT_TR_NOOP("Please select the components you want to update."),
        QT_TR_NOOP("Please select the components you want to install."),
        QT_TR_NOOP("Please select the components you want to uninstall."),
        QT_TR_NOOP("Select the components to install. Deselect installed components to uninstall them.")
     };

    int index = 0;
    PackageManagerCore *core = packageManagerCore();
    if (core->isInstaller()) index = 1;
    if (core->isUninstaller()) index = 2;
    if (core->isPackageManager()) index = 3;
    setColoredSubTitle(tr(strings[index]));

    d->updateTreeView();
    setModified(isComplete());
}

void ComponentSelectionPage::showEvent(QShowEvent *event)
{
    // remove once we deprecate isSelected, setSelected etc...
    if (!event->spontaneous())
        packageManagerCore()->resetComponentsToUserCheckedState();
    QWizardPage::showEvent(event);
}

void ComponentSelectionPage::selectAll()
{
    d->selectAll();
}

void ComponentSelectionPage::deselectAll()
{
    d->deselectAll();
}

void ComponentSelectionPage::selectDefault()
{
    if (packageManagerCore()->isInstaller())
        d->selectDefault();
}

/*!
    Selects the component with /a id in the component tree.
*/
void ComponentSelectionPage::selectComponent(const QString &id)
{
    const QModelIndex &idx = d->m_currentModel->indexFromComponentName(id);
    if (idx.isValid())
        d->m_currentModel->setData(idx, Qt::Checked, Qt::CheckStateRole);
}

/*!
    Deselects the component with /a id in the component tree.
*/
void ComponentSelectionPage::deselectComponent(const QString &id)
{
    const QModelIndex &idx = d->m_currentModel->indexFromComponentName(id);
    if (idx.isValid())
        d->m_currentModel->setData(idx, Qt::Unchecked, Qt::CheckStateRole);
}

void ComponentSelectionPage::setModified(bool modified)
{
    setComplete(modified);
}

bool ComponentSelectionPage::isComplete() const
{
    if (packageManagerCore()->isInstaller() || packageManagerCore()->isUpdater())
        return d->m_currentModel->checked().count();
    return d->m_currentModel->checkedState().testFlag(ComponentModel::DefaultChecked) == false;
}


// -- TargetDirectoryPage

TargetDirectoryPage::TargetDirectoryPage(PackageManagerCore *core)
    : PackageManagerPage(core)
{
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("TargetDirectoryPage"));
    setColoredTitle(tr("Installation Folder"));

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *msgLabel = new QLabel(this);
    msgLabel->setObjectName(QLatin1String("MessageLabel"));
    msgLabel->setWordWrap(true);
    msgLabel->setText(tr("Please specify the folder where %1 will be installed.").arg(productName()));
    layout->addWidget(msgLabel);

    QHBoxLayout *hlayout = new QHBoxLayout;
    layout->addLayout(hlayout);

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setObjectName(QLatin1String("TargetDirectoryLineEdit"));
    m_lineEdit->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    connect(m_lineEdit, SIGNAL(textChanged(QString)), this, SIGNAL(completeChanged()));
    hlayout->addWidget(m_lineEdit);

    QPushButton *browseButton = new QPushButton(this);
    browseButton->setObjectName(QLatin1String("BrowseDirectoryButton"));
    connect(browseButton, SIGNAL(clicked()), this, SLOT(dirRequested()));
    browseButton->setShortcut(QKeySequence(tr("Alt+R", "browse file system to choose a file")));
    browseButton->setText(tr("B&rowse..."));
    hlayout->addWidget(browseButton);
}

QString TargetDirectoryPage::targetDir() const
{
    return m_lineEdit->text();
}

void TargetDirectoryPage::setTargetDir(const QString &dirName)
{
    m_lineEdit->setText(dirName);
}

void TargetDirectoryPage::initializePage()
{
    QString targetDir = packageManagerCore()->value(scTargetDir);
    if (targetDir.isEmpty()) {
#ifdef Q_OS_WIN
        targetDir = packageManagerCore()->value(QLatin1String("ApplicationsDir"));
        if(!targetDir.isEmpty()) {
            targetDir += QDir::separator();
            targetDir += packageManagerCore()->value(scPublisher);
            targetDir += QDir::separator();
            targetDir += productName();
        }
        else
#endif
        {
        targetDir = QDir::homePath() + QDir::separator();
        // prevent spaces in the default target directory
        if (targetDir.contains(QLatin1Char(' ')))
            targetDir = QDir::rootPath();
        targetDir += productName().remove(QLatin1Char(' '));
        }
    }
    m_lineEdit->setText(QDir::toNativeSeparators(QDir(targetDir).absolutePath()));

    PackageManagerPage::initializePage();
}

bool TargetDirectoryPage::validatePage()
{
    const QString targetdir = targetDir();
    if (targetdir.isEmpty()) {
        MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("EmptyTargetDirectoryMessage"), tr("Error"), tr("The install directory "
            "cannot be empty, please specify a valid folder."), QMessageBox::Ok);
        return false;
    }

    const QDir dir(targetdir);
    // it exists, but is empty (might be created by the Browse button (getExistingDirectory)
    if (dir.exists() && dir.entryList(QDir::NoDotAndDotDot).isEmpty())
        return true;

    if (dir.exists() && dir.isReadable()) {
        // it exists, but is not empty
        if (dir == QDir::root() || dir == QDir::homePath() || dir == QDir::tempPath()) {
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
                QLatin1String("ForbiddenTargetDirectoryMessage"), tr("Error"), tr("As the install "
                "directory is completely deleted on uninstall, installing in %1 is forbidden.")
                .arg(targetdir),
                QMessageBox::Ok);
            return false;
        }

        if (!QVariant(packageManagerCore()->value(scRemoveTargetDir)).toBool())
            return true;

        return MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("OverwriteTargetDirectoryMessage"), tr("Warning"), tr("You have "
            "selected an existing, non-empty folder for installation. Note that it will be "
            "completely wiped on uninstallation of this application. It is not advisable to "
            "install into this folder as installation might fail. Do you want to continue?"),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
    }
    return true;
}

void TargetDirectoryPage::entering()
{
}

void TargetDirectoryPage::leaving()
{
    packageManagerCore()->setValue(scTargetDir, targetDir());
}

void TargetDirectoryPage::targetDirSelected()
{
}

void TargetDirectoryPage::dirRequested()
{
    const QString newDirName = QFileDialog::getExistingDirectory(this,
        tr("Select Installation Folder"), targetDir());
    if (newDirName.isEmpty() || newDirName == targetDir())
        return;
    m_lineEdit->setText(QDir::toNativeSeparators(newDirName));
}


// -- StartMenuDirectoryPage

StartMenuDirectoryPage::StartMenuDirectoryPage(PackageManagerCore *core)
    : PackageManagerPage(core)
{
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("StartMenuDirectoryPage"));
    setColoredTitle(tr("Start Menu shortcuts"));
    setColoredSubTitle(tr("Select the Start Menu in which you would like to create the program's "
        "shortcuts. You can also enter a name to create a new folder."));

    QVBoxLayout *layout = new QVBoxLayout(this);

    QString startMenuDir = core->value(scStartMenuDir);
    if (startMenuDir.isEmpty())
        startMenuDir = productName();

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setObjectName(QLatin1String("LineEdit"));
    m_lineEdit->setText(startMenuDir);
    layout->addWidget(m_lineEdit);

    // grab existing start menu folders
    QSettings user(QLatin1String("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\"
        "Explorer\\User Shell Folders"), QSettings::NativeFormat);
    // User Shell Folders uses %USERPROFILE%
    startMenuPath = replaceWindowsEnvironmentVariables(user.value(QLatin1String("Programs"),
        QString()).toString());
    core->setValue(QLatin1String("DesktopDir"), replaceWindowsEnvironmentVariables(user
        .value(QLatin1String("Desktop")).toString()));

    QDir dir(startMenuPath); // user only dirs
    QStringList dirs = dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);

    if (core->value(QLatin1String("AllUsers")) == QLatin1String("true")) {
        qDebug() << "AllUsers set. Using HKEY_LOCAL_MACHINE";
        QSettings system(QLatin1String("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\"
            "CurrentVersion\\Explorer\\Shell Folders"), QSettings::NativeFormat);
        startMenuPath = system.value(QLatin1String("Common Programs"), QString()).toString();
        core->setValue(QLatin1String("DesktopDir"),system.value(QLatin1String("Desktop"))
            .toString());

        dir.setPath(startMenuPath); // system only dirs
        dirs += dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    }

    qDebug() << "StartMenuPath: \t" << startMenuPath;
    qDebug() << "DesktopDir: \t" << core->value(QLatin1String("DesktopDir"));

    m_listWidget = new QListWidget(this);
    layout->addWidget(m_listWidget);

    if (!dirs.isEmpty()) {
        dirs.removeDuplicates();
        foreach (const QString &dir, dirs)
            new QListWidgetItem(dir, m_listWidget);
    }

    connect(m_listWidget, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)), this,
        SLOT(currentItemChanged(QListWidgetItem*)));
}

QString StartMenuDirectoryPage::startMenuDir() const
{
    return m_lineEdit->text().trimmed();
}

void StartMenuDirectoryPage::setStartMenuDir(const QString &startMenuDir)
{
    m_lineEdit->setText(startMenuDir.trimmed());
}

void StartMenuDirectoryPage::leaving()
{
    packageManagerCore()->setValue(scStartMenuDir, startMenuPath + QDir::separator()
        + startMenuDir());
}

void StartMenuDirectoryPage::currentItemChanged(QListWidgetItem *current)
{
    if (current)
        setStartMenuDir(current->data(Qt::DisplayRole).toString());
}


// -- ReadyForInstallationPage

ReadyForInstallationPage::ReadyForInstallationPage(PackageManagerCore *core)
    : PackageManagerPage(core)
    , m_msgLabel(new QLabel)
{
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("ReadyForInstallationPage"));

    QVBoxLayout *baseLayout = new QVBoxLayout(this);
    baseLayout->setObjectName(QLatin1String("BaseLayout"));

    QVBoxLayout *topLayout = new QVBoxLayout();
    topLayout->setObjectName(QLatin1String("TopLayout"));
    baseLayout->addLayout(topLayout);

    m_msgLabel->setWordWrap(true);
    m_msgLabel->setObjectName(QLatin1String("MessageLabel"));
    m_msgLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    topLayout->addWidget(m_msgLabel);

    m_taskDetailsButton = new QPushButton(tr("&Show Details"), this);
    m_taskDetailsButton->setObjectName(QLatin1String("TaskDetailsButton"));
    m_taskDetailsButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(m_taskDetailsButton, SIGNAL(clicked()), this, SLOT(toggleDetails()));
    topLayout->addWidget(m_taskDetailsButton);

    QVBoxLayout *bottomLayout = new QVBoxLayout();
    bottomLayout->setObjectName(QLatin1String("BottomLayout"));
    baseLayout->addLayout(bottomLayout);

    bottomLayout->addStretch();

    m_taskDetailsBrowser = new QTextBrowser(this);
    m_taskDetailsBrowser->setReadOnly(true);
    m_taskDetailsBrowser->setObjectName(QLatin1String("TaskDetailsBrowser"));
    m_taskDetailsBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_taskDetailsBrowser->setVisible(false);
    bottomLayout->addWidget(m_taskDetailsBrowser);
    bottomLayout->setStretch(1, 10);
}


/*!
    \reimp
*/
void ReadyForInstallationPage::entering()
{
    setCommitPage(false);

    if (packageManagerCore()->isUninstaller()) {
        m_taskDetailsButton->setVisible(false);
        m_taskDetailsBrowser->setVisible(false);
        setButtonText(QWizard::CommitButton, tr("U&ninstall"));
        setColoredTitle(tr("Ready to Uninstall"));
        m_msgLabel->setText(tr("Setup is now ready to begin removing %1 from your computer.<br>"
            "<font color=\"red\">The program directory %2 will be deleted completely</font>, "
            "including all content in that directory!")
            .arg(productName(),
                QDir::toNativeSeparators(QDir(packageManagerCore()->value(scTargetDir))
            .absolutePath())));
        setCommitPage(true);
        return;
    } else if (packageManagerCore()->isPackageManager() || packageManagerCore()->isUpdater()) {
        setButtonText(QWizard::CommitButton, tr("U&pdate"));
        setColoredTitle(tr("Ready to Update Packages"));
        m_msgLabel->setText(tr("Setup is now ready to begin updating your installation."));
    } else {
        Q_ASSERT(packageManagerCore()->isInstaller());
        setButtonText(QWizard::CommitButton, tr("&Install"));
        setColoredTitle(tr("Ready to Install"));
        m_msgLabel->setText(tr("Setup is now ready to begin installing %1 on your computer.")
            .arg(productName()));
    }

    refreshTaskDetailsBrowser();

    const VolumeInfo tempVolume = VolumeInfo::fromPath(QDir::tempPath());
    const VolumeInfo targetVolume = VolumeInfo::fromPath(packageManagerCore()->value(scTargetDir));

    const quint64 tempVolumeAvailableSize = tempVolume.availableSize();
    const quint64 installVolumeAvailableSize = targetVolume.availableSize();

    // at the moment there is no better way to check this
    if (targetVolume.size() == 0 && installVolumeAvailableSize == 0) {
        qDebug() << QString::fromLatin1("Could not determine available space on device. Volume "
            "descriptor: %1, Mount path: %2. Continue silently.").arg(targetVolume
            .volumeDescriptor(), targetVolume.mountPath());
        setCommitPage(true);
        return;     // TODO: Shouldn't this also disable the "Next" button?
    }

    const bool tempOnSameVolume = (targetVolume == tempVolume);
    if (tempOnSameVolume) {
        qDebug() << "Tmp and install folder are on the same volume. Volume mount point:"
            << targetVolume.mountPath() << "Free space available:"
            << humanReadableSize(installVolumeAvailableSize);
    } else {
        qDebug() << "Tmp is on a different volume than the install folder. Tmp volume mount point:"
            << tempVolume.mountPath() << "Free space available:"
            << humanReadableSize(tempVolumeAvailableSize) << "Install volume mount point:"
            << targetVolume.mountPath() << "Free space available:"
            << humanReadableSize(installVolumeAvailableSize);
    }

    const quint64 extraSpace = 256 * 1024 * 1024LL;
    quint64 required(packageManagerCore()->requiredDiskSpace());
    quint64 tempRequired(packageManagerCore()->requiredTemporaryDiskSpace());
    if (required < extraSpace) {
        required += 0.1 * required;
        tempRequired += 0.1 * tempRequired;
    } else {
        required += extraSpace;
        tempRequired += extraSpace;
    }

    quint64 repositorySize = 0;
    const bool createLocalRepository = packageManagerCore()->createLocalRepositoryFromBinary();
    if (createLocalRepository) {
        repositorySize = QFile(QCoreApplication::applicationFilePath()).size();
        // if we create a local repository, take that space into account as well
        required += repositorySize;
    }

    qDebug() << "Installation space required:" << humanReadableSize(required) << "Temporary space "
        "required:" << humanReadableSize(tempRequired) << "Local repository size:"
        << humanReadableSize(repositorySize);

    if (tempOnSameVolume && (installVolumeAvailableSize <= (required + tempRequired))) {
        m_msgLabel->setText(tr("Not enough disk space to store temporary files and the "
            "installation! Available space: %1, at least required %2.")
            .arg(humanReadableSize(installVolumeAvailableSize),
            humanReadableSize(required + tempRequired)));
        return;
    }

    if (installVolumeAvailableSize < required) {
        m_msgLabel->setText(tr("Not enough disk space to store all selected components! Available "
            "space: %1, at least required: %2.").arg(humanReadableSize(installVolumeAvailableSize),
            humanReadableSize(required)));
        return;
    }

    if (tempVolumeAvailableSize < tempRequired) {
        m_msgLabel->setText(tr("Not enough disk space to store temporary files! Available space: "
            "%1, at least required: %2.").arg(humanReadableSize(tempVolumeAvailableSize),
            humanReadableSize(tempRequired)));
        return;
    }

    if (installVolumeAvailableSize - required < 0.01 * targetVolume.size()) {
        // warn for less than 1% of the volume's space being free
        m_msgLabel->setText(tr("The volume you selected for installation seems to have sufficient "
            "space for installation, but there will be less than 1% of the volume's space "
            "available afterwards. %1").arg(m_msgLabel->text()));
    } else if (installVolumeAvailableSize - required < 100 * 1024 * 1024LL) {
        // warn for less than 100MB being free
        m_msgLabel->setText(tr("The volume you selected for installation seems to have sufficient "
            "space for installation, but there will be less than 100 MB available afterwards. %1")
            .arg(m_msgLabel->text()));
    }
    setCommitPage(true);
}

void ReadyForInstallationPage::refreshTaskDetailsBrowser()
{
    QString htmlOutput;
    QString lastInstallReason;
    if (!packageManagerCore()->calculateComponentsToUninstall() ||
        !packageManagerCore()->calculateComponentsToInstall()) {
            htmlOutput.append(QString::fromLatin1("<h2><font color=\"red\">%1</font></h2><ul>")
                .arg(tr("Can not resolve all dependencies!")));
            //if we have a missing dependency or a recursion we can display it
            if (!packageManagerCore()->componentsToInstallError().isEmpty()) {
                htmlOutput.append(QString::fromLatin1("<li> %1 </li>").arg(
                    packageManagerCore()->componentsToInstallError()));
            }
            htmlOutput.append(QLatin1String("</ul>"));
            m_taskDetailsBrowser->setHtml(htmlOutput);
            if (!m_taskDetailsBrowser->isVisible())
                toggleDetails();
            setCommitPage(false);
            return;
    }

    // In case of updater mode we don't uninstall components.
    if (!packageManagerCore()->isUpdater()) {
        QList<Component*> componentsToRemove = packageManagerCore()->componentsToUninstall();
        if (!componentsToRemove.isEmpty()) {
            htmlOutput.append(QString::fromLatin1("<h3>%1</h3><ul>").arg(tr("Components about to "
                "be removed.")));
            foreach (Component *component, componentsToRemove)
                htmlOutput.append(QString::fromLatin1("<li> %1 </li>").arg(component->name()));
            htmlOutput.append(QLatin1String("</ul>"));
        }
    }

    foreach (Component *component, packageManagerCore()->orderedComponentsToInstall()) {
        const QString installReason = packageManagerCore()->installReason(component);
        if (lastInstallReason != installReason) {
            if (!lastInstallReason.isEmpty()) // means we had to close the previous list
                htmlOutput.append(QLatin1String("</ul>"));
            htmlOutput.append(QString::fromLatin1("<h3>%1</h3><ul>").arg(installReason));
            lastInstallReason = installReason;
        }
        htmlOutput.append(QString::fromLatin1("<li> %1 </li>").arg(component->name()));
    }
    m_taskDetailsBrowser->setHtml(htmlOutput);
}

void ReadyForInstallationPage::toggleDetails()
{
    const bool visible = !m_taskDetailsBrowser->isVisible();
    m_taskDetailsBrowser->setVisible(visible);
    m_taskDetailsButton->setText(visible ? tr("&Hide Details") : tr("&Show Details"));
}

void ReadyForInstallationPage::leaving()
{
    setButtonText(QWizard::CommitButton, gui()->defaultButtonText(QWizard::CommitButton));
}

/*!
    \reimp
*/
bool ReadyForInstallationPage::isComplete() const
{
    return isCommitPage();
}


// -- PerformInstallationPage

/*!
    \class QInstaller::PerformInstallationPage
    On this page the user can see on a progress bar how far the current installation is.
*/
PerformInstallationPage::PerformInstallationPage(PackageManagerCore *core)
    : PackageManagerPage(core)
    , m_performInstallationForm(new PerformInstallationForm(this))
{
    setPixmap(QWizard::WatermarkPixmap, QPixmap());
    setObjectName(QLatin1String("PerformInstallationPage"));

    m_performInstallationForm->setupUi(this);

    connect(ProgressCoordinator::instance(), SIGNAL(detailTextChanged(QString)),
        m_performInstallationForm, SLOT(appendProgressDetails(QString)));
    connect(ProgressCoordinator::instance(), SIGNAL(detailTextResetNeeded()),
        m_performInstallationForm, SLOT(clearDetailsBrowser()));
    connect(m_performInstallationForm, SIGNAL(showDetailsChanged()), this,
        SLOT(toggleDetailsWereChanged()));

    connect(core, SIGNAL(installationStarted()), this, SLOT(installationStarted()));
    connect(core, SIGNAL(installationFinished()), this, SLOT(installationFinished()));

    connect(core, SIGNAL(uninstallationStarted()), this, SLOT(uninstallationStarted()));
    connect(core, SIGNAL(uninstallationFinished()), this, SLOT(uninstallationFinished()));

    connect(core, SIGNAL(titleMessageChanged(QString)), this, SLOT(setTitleMessage(QString)));
    connect(this, SIGNAL(setAutomatedPageSwitchEnabled(bool)), core,
        SIGNAL(setAutomatedPageSwitchEnabled(bool)));

    m_performInstallationForm->setDetailsWidgetVisible(true);
}

PerformInstallationPage::~PerformInstallationPage()
{
    delete m_performInstallationForm;
}

bool PerformInstallationPage::isAutoSwitching() const
{
    return !m_performInstallationForm->isShowingDetails();
}

// -- protected

void PerformInstallationPage::entering()
{
    setComplete(false);
    setCommitPage(true);

    if (packageManagerCore()->isUninstaller()) {
        setButtonText(QWizard::CommitButton, tr("U&ninstall"));
        setColoredTitle(tr("Uninstalling %1").arg(productName()));

        QTimer::singleShot(30, packageManagerCore(), SLOT(runUninstaller()));
    } else if (packageManagerCore()->isPackageManager() || packageManagerCore()->isUpdater()) {
        setButtonText(QWizard::CommitButton, tr("&Update"));
        setColoredTitle(tr("Updating components of %1").arg(productName()));

        QTimer::singleShot(30, packageManagerCore(), SLOT(runPackageUpdater()));
    } else {
        setButtonText(QWizard::CommitButton, tr("&Install"));
        setColoredTitle(tr("Installing %1").arg(productName()));

        QTimer::singleShot(30, packageManagerCore(), SLOT(runInstaller()));
    }

    m_performInstallationForm->enableDetails();
    emit setAutomatedPageSwitchEnabled(true);
}

void PerformInstallationPage::leaving()
{
    setButtonText(QWizard::CommitButton, gui()->defaultButtonText(QWizard::CommitButton));
}

// -- public slots

void PerformInstallationPage::setTitleMessage(const QString &title)
{
    setColoredTitle(title);
}

// -- private slots

void PerformInstallationPage::installationStarted()
{
    m_performInstallationForm->startUpdateProgress();
}

void PerformInstallationPage::installationFinished()
{
    m_performInstallationForm->stopUpdateProgress();
    if (!isAutoSwitching()) {
        m_performInstallationForm->scrollDetailsToTheEnd();
        m_performInstallationForm->setDetailsButtonEnabled(false);

        setComplete(true);
        setButtonText(QWizard::CommitButton, gui()->defaultButtonText(QWizard::NextButton));
    }
}

void PerformInstallationPage::uninstallationStarted()
{
    m_performInstallationForm->startUpdateProgress();
    if (QAbstractButton *cancel = gui()->button(QWizard::CancelButton))
        cancel->setEnabled(false);
}

void PerformInstallationPage::uninstallationFinished()
{
    installationFinished();
    if (QAbstractButton *cancel = gui()->button(QWizard::CancelButton))
        cancel->setEnabled(false);
}

void PerformInstallationPage::toggleDetailsWereChanged()
{
    emit setAutomatedPageSwitchEnabled(isAutoSwitching());
}


// -- FinishedPage

FinishedPage::FinishedPage(PackageManagerCore *core)
    : PackageManagerPage(core)
    , m_commitButton(0)
{
    setObjectName(QLatin1String("FinishedPage"));
    setColoredTitle(tr("Completing the %1 Wizard").arg(productName()));

    m_msgLabel = new QLabel(this);
    m_msgLabel->setWordWrap(true);
    m_msgLabel->setObjectName(QLatin1String("MessageLabel"));

#ifdef Q_OS_MAC
    m_msgLabel->setText(tr("Click Done to exit the %1 Wizard.").arg(productName()));
#else
    m_msgLabel->setText(tr("Click Finish to exit the %1 Wizard.").arg(productName()));
#endif

    m_runItCheckBox = new QCheckBox(this);
    m_runItCheckBox->setObjectName(QLatin1String("RunItCheckBox"));
    m_runItCheckBox->setChecked(true);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_msgLabel);
    layout->addWidget(m_runItCheckBox);
    setLayout(layout);
}

void FinishedPage::entering()
{
    if (m_commitButton) {
        disconnect(m_commitButton, SIGNAL(clicked()), this, SLOT(handleFinishClicked()));
        m_commitButton = 0;
    }

    setCommitPage(true);
    if (packageManagerCore()->isUpdater() || packageManagerCore()->isPackageManager()) {
#ifdef Q_OS_MAC
        gui()->setOption(QWizard::NoCancelButton, false);
#endif
        if (QAbstractButton *cancel = gui()->button(QWizard::CancelButton)) {
            m_commitButton = cancel;
            cancel->setEnabled(true);
            cancel->setVisible(true);
            // we don't use the usual FinishButton so we need to connect the misused CancelButton
            connect(cancel, SIGNAL(clicked()), gui(), SIGNAL(finishButtonClicked()));
            connect(cancel, SIGNAL(clicked()), packageManagerCore(), SIGNAL(finishButtonClicked()));
            // for the moment we don't want the rejected signal connected
            disconnect(gui(), SIGNAL(rejected()), packageManagerCore(), SLOT(setCanceled()));

            connect(gui()->button(QWizard::CommitButton), SIGNAL(clicked()), this,
                SLOT(cleanupChangedConnects()));
        }
        setButtonText(QWizard::CommitButton, tr("Restart"));
        setButtonText(QWizard::CancelButton, gui()->defaultButtonText(QWizard::FinishButton));
    } else {
        if (packageManagerCore()->isInstaller())
            m_commitButton = wizard()->button(QWizard::FinishButton);

        gui()->setOption(QWizard::NoCancelButton, true);
        if (QAbstractButton *cancel = gui()->button(QWizard::CancelButton))
            cancel->setVisible(false);
    }

    gui()->updateButtonLayout();

    if (m_commitButton) {
        disconnect(m_commitButton, SIGNAL(clicked()), this, SLOT(handleFinishClicked()));
        connect(m_commitButton, SIGNAL(clicked()), this, SLOT(handleFinishClicked()));
    }

    if (packageManagerCore()->status() == PackageManagerCore::Success) {
        const QString finishedText = packageManagerCore()->value(QLatin1String("FinishedText"));
        if (!finishedText.isEmpty())
            m_msgLabel->setText(finishedText);

        if (!packageManagerCore()->isUninstaller() && !packageManagerCore()->value(scRunProgram)
            .isEmpty()) {
                m_runItCheckBox->show();
                m_runItCheckBox->setText(packageManagerCore()->value(scRunProgramDescription,
                    tr("Run %1 now.")).arg(productName()));
            return; // job done
        }
    } else {
        // TODO: how to handle this using the config.xml
        setColoredTitle(tr("The %1 Wizard failed.").arg(productName()));
    }

    m_runItCheckBox->hide();
    m_runItCheckBox->setChecked(false);
}

void FinishedPage::leaving()
{
#ifdef Q_OS_MAC
    gui()->setOption(QWizard::NoCancelButton, true);
#endif

    if (QAbstractButton *cancel = gui()->button(QWizard::CancelButton))
        cancel->setVisible(false);
    gui()->updateButtonLayout();

    setButtonText(QWizard::CommitButton, gui()->defaultButtonText(QWizard::CommitButton));
    setButtonText(QWizard::CancelButton, gui()->defaultButtonText(QWizard::CancelButton));
}

void FinishedPage::handleFinishClicked()
{
    const QString program =
        packageManagerCore()->replaceVariables(packageManagerCore()->value(scRunProgram));
    const QStringList args = packageManagerCore()->replaceVariables(
        packageManagerCore()->value(scRunProgramArguments)).split(QLatin1Char(' '),
        QString::SkipEmptyParts);
    if (!m_runItCheckBox->isChecked() || program.isEmpty())
        return;

    qDebug() << "starting" << program << args;
    QProcess::startDetached(program, args);
}

void FinishedPage::cleanupChangedConnects()
{
    if (QAbstractButton *cancel = gui()->button(QWizard::CancelButton)) {
        // remove the workaround connect from entering page
        disconnect(cancel, SIGNAL(clicked()), gui(), SIGNAL(finishButtonClicked()));
        disconnect(cancel, SIGNAL(clicked()), packageManagerCore(), SIGNAL(finishButtonClicked()));
        connect(gui(), SIGNAL(rejected()), packageManagerCore(), SLOT(setCanceled()));

        disconnect(gui()->button(QWizard::CommitButton), SIGNAL(clicked()), this,
            SLOT(cleanupChangedConnects()));
    }
}

// -- RestartPage

RestartPage::RestartPage(PackageManagerCore *core)
    : PackageManagerPage(core)
{
    setObjectName(QLatin1String("RestartPage"));
    setColoredTitle(tr("Completing the %1 Setup Wizard").arg(productName()));

    setFinalPage(false);
    setCommitPage(false);
}

int RestartPage::nextId() const
{
    return PackageManagerCore::Introduction;
}

void RestartPage::entering()
{
    if (!packageManagerCore()->needsHardRestart()) {
        if (QAbstractButton *finish = wizard()->button(QWizard::FinishButton))
            finish->setVisible(false);
        QMetaObject::invokeMethod(this, "restart", Qt::QueuedConnection);
    } else {
        gui()->accept();
    }
}

void RestartPage::leaving()
{
}

#include "packagemanagergui.moc"
#include "moc_packagemanagergui.cpp"
