// From: skalee / on-interactive-qt-installer

function Controller() {
    installer.installationFinished.connect(proceed)
    installer.setMessageBoxAutomaticAnswer("OverwriteTargetDirectory", QMessageBox.Yes)
    installer.setMessageBoxAutomaticAnswer("stopProcessesForUpdates", QMessageBox.Ignore)
    gui.showMinimized()
}

function logCurrentPage() {
    var pageName = page().objectName
    print("At page: " + pageName)
}

function page() {
    return gui.currentPageWidget()
}

function proceed(button, delay) {
    gui.clickButton(button || buttons.NextButton, delay)
    gui.hide()
}

/// Skip welcome page
Controller.prototype.WelcomePageCallback = function() {
    logCurrentPage()
    // For some reason, delay is needed.  Two seconds seems to be enough.
    proceed(buttons.NextButton, 2000)
}

/// Set target directory
Controller.prototype.TargetDirectoryPageCallback = function() {
    logCurrentPage()
    page().setDefault()
    proceed()
}

Controller.prototype.ComponentSelectionPageCallback = function() {
    logCurrentPage()
    // Deselect whatever was default, and can be deselected.
    page().selectDefault()
    proceed()
}

/// Just click next -- that is sign in to Qt account if credentials are
/// remembered from previous installs, or skip sign in otherwise.
Controller.prototype.CredentialsPageCallback = function() {
    logCurrentPage()
    proceed()
}

/// Skip introduction page
Controller.prototype.IntroductionPageCallback = function() {
    logCurrentPage()
    proceed()
}

/// Agree license
Controller.prototype.LicenseAgreementPageCallback = function() {
    logCurrentPage()
    page().AcceptLicenseRadioButton.checked = true
    gui.clickButton(buttons.NextButton)
    gui.hide()
}

/// Windows-specific, skip it
Controller.prototype.StartMenuDirectoryPageCallback = function() {
    logCurrentPage()
    gui.clickButton(buttons.NextButton)
    gui.hide()
}

/// Skip confirmation page
Controller.prototype.ReadyForInstallationPageCallback = function() {
    logCurrentPage()
    proceed()
}

/// Installation in progress, do nothing
Controller.prototype.PerformInstallationPageCallback = function() {
    logCurrentPage()
    gui.hide()
}

Controller.prototype.FinishedPageCallback = function() {
    logCurrentPage()
    page().RunItCheckBox.checked = false
    proceed(buttons.FinishButton)
}
