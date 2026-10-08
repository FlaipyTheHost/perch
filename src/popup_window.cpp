#include "popup_window.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QDesktopServices>
#include <QEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>
#include <QWebEngineNewWindowRequest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>

PopupWindow::PopupWindow(const Config& config, QWidget* parent)
    : QWidget(parent, Qt::Window), config_(config) {
    setWindowTitle(QStringLiteral("Perch"));
    buildUi();
    applyConfig(config);

    auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(esc, &QShortcut::activated, this, &PopupWindow::hidePopup);
}

PopupWindow::~PopupWindow() {
    // The page must die before its profile.
    delete view_;
    delete page_;
}

void PopupWindow::buildUi() {
    // Persistent session: logins survive restarts.
    profile_ = new QWebEngineProfile(QStringLiteral("perch"), this);
    profile_->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);

    // Some sign-in pages refuse embedded browsers that identify as QtWebEngine/Chromium.
    // The User-Agent is configurable; Client Hints are disabled in main.cpp so both agree.
    profile_->setHttpUserAgent(config_.userAgent);

    page_ = new QWebEnginePage(profile_, this);
    // Links that request a new window open in the system's default browser.
    connect(page_, &QWebEnginePage::newWindowRequested, this,
            [](QWebEngineNewWindowRequest& request) { QDesktopServices::openUrl(request.requestedUrl()); });

    view_ = new QWebEngineView(this);
    view_->setPage(page_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(view_);
}

QString PopupWindow::welcomeHtml() const {
    return QStringLiteral(
               "<!doctype html><meta charset=utf-8><meta name=color-scheme content='light dark'>"
               "<body style='font-family:sans-serif;display:flex;align-items:center;"
               "justify-content:center;height:100vh;margin:0;text-align:center'>"
               "<div style='max-width:20em;padding:1em'><h2>%1</h2><p>%2</p></div></body>")
        .arg(tr("Welcome to Perch").toHtmlEscaped(),
             tr("Right-click the tray icon and choose Settings to pick the AI service you want to use.")
                 .toHtmlEscaped());
}

void PopupWindow::loadPage() {
    if (config_.url.isEmpty()) {
        view_->setHtml(welcomeHtml());
    } else {
        view_->setUrl(QUrl(config_.url));
    }
}

void PopupWindow::applyConfig(const Config& config) {
    const bool urlChanged = !initialized_ || config.url != config_.url;
    initialized_ = true;
    config_ = config;
    resize(config.width, config.height);
    if (urlChanged) loadPage();
}

void PopupWindow::moveToRightSide() {
    // The screen under the cursor (the user just clicked the tray icon).
    QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) screen = QGuiApplication::primaryScreen();
    const QRect area = screen->availableGeometry();  // already excludes the panel

    // frameGeometry includes the title bar (known once the window is mapped).
    const QSize frame = frameGeometry().size().expandedTo(size());
    move(area.right() - frame.width() - config_.margin + 1,
         area.bottom() - frame.height() - config_.margin + 1);
}

void PopupWindow::toggle() {
    if (isVisible()) {
        hidePopup();
        return;
    }
    // Clicking the tray icon takes focus away (the window hides itself) and the activation
    // click arrives right after: don't reopen in that case.
    if (lastAutoHide_.isValid() && lastAutoHide_.elapsed() < 500) return;
    showPopup();
}

void PopupWindow::showPopup() {
    moveToRightSide();
    show();
    QTimer::singleShot(0, this, &PopupWindow::moveToRightSide);  // re-adjust with the title bar size
    raise();
    activateWindow();
    view_->setFocus();
}

void PopupWindow::hidePopup() {
    hide();
}

void PopupWindow::closeEvent(QCloseEvent* event) {
    // Closing only hides: the app stays in the tray.
    event->ignore();
    hidePopup();
}

void PopupWindow::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::ActivationChange && isVisible() && !isActiveWindow()) {
        QTimer::singleShot(150, this, &PopupWindow::hideIfFocusLost);
    }
}

void PopupWindow::hideIfFocusLost() {
    // Don't hide if focus moved to another window of this app (e.g. a file chooser when
    // attaching a file) or to a menu/popup.
    if (!isVisible() || isActiveWindow()) return;
    if (QApplication::activeWindow() || QApplication::activePopupWidget()) return;
    lastAutoHide_.restart();
    hidePopup();
}
