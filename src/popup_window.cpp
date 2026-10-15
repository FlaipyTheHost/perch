#include "popup_window.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QShortcut>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>
#include <QWebEngineDownloadRequest>
#include <QWebEngineNewWindowRequest>
#include <QWebEnginePage>
#include <QWebEnginePermission>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
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

    // Downloads (files, images, generated documents...) go straight to the user's Downloads folder.
    // QtWebEngine cancels every download that isn't explicitly accepted.
    connect(profile_, &QWebEngineProfile::downloadRequested, this, &PopupWindow::handleDownload);

    page_ = new QWebEnginePage(profile_, this);
    // Links that request a new window open in the system's default browser.
    connect(page_, &QWebEnginePage::newWindowRequested, this,
            [](QWebEngineNewWindowRequest& request) { QDesktopServices::openUrl(request.requestedUrl()); });

    // Let the site's "copy" buttons (navigator.clipboard.writeText) reach the system clipboard.
    // QtWebEngine blocks JavaScript clipboard access by default; Ctrl+C works regardless
    // because it goes through Qt's native copy action.
    page_->settings()->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, true);
    connect(page_, &QWebEnginePage::permissionRequested, this, [](QWebEnginePermission permission) {
        if (permission.permissionType() == QWebEnginePermission::PermissionType::ClipboardReadWrite)
            permission.grant();
    });

    view_ = new QWebEngineView(this);
    view_->setPage(page_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(view_);
}

QString PopupWindow::uniqueFileName(const QString& dir, QString name) const {
    name = QFileInfo(name).fileName();  // never let a suggested name escape the folder
    if (name.isEmpty() || name == QLatin1String(".") || name == QLatin1String(".."))
        name = QStringLiteral("download");

    const QDir folder(dir);
    auto taken = [&](const QString& candidate) {
        const QString path = folder.filePath(candidate);
        return QFileInfo::exists(path) || pendingDownloads_.contains(path);
    };
    if (!taken(name)) return name;

    // "report.pdf" -> "report (1).pdf"; keeps compound extensions such as ".tar.gz".
    const QFileInfo info(name);
    QString base = info.completeBaseName();
    QString suffix = info.suffix().isEmpty() ? QString() : QLatin1Char('.') + info.suffix();
    if (base.endsWith(QLatin1String(".tar"))) {
        base.chop(4);
        suffix.prepend(QLatin1String(".tar"));
    }
    if (base.isEmpty()) {  // dotfiles like ".bashrc"
        base = name;
        suffix.clear();
    }
    for (int i = 1;; ++i) {
        const QString candidate = QStringLiteral("%1 (%2)%3").arg(base).arg(i).arg(suffix);
        if (!taken(candidate)) return candidate;
    }
}

void PopupWindow::handleDownload(QWebEngineDownloadRequest* download) {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dir.isEmpty()) dir = QDir::homePath();
    QDir().mkpath(dir);

    download->setDownloadDirectory(dir);
    download->setDownloadFileName(uniqueFileName(dir, download->downloadFileName()));
    const QString path = QDir(dir).filePath(download->downloadFileName());
    pendingDownloads_.insert(path);

    connect(download, &QWebEngineDownloadRequest::isFinishedChanged, this, [this, download, path] {
        if (!download->isFinished()) return;
        pendingDownloads_.remove(path);
        switch (download->state()) {
            case QWebEngineDownloadRequest::DownloadCompleted:
                emit downloadFinished(path);
                break;
            case QWebEngineDownloadRequest::DownloadInterrupted:
                emit downloadFailed(download->downloadFileName(), download->interruptReasonString());
                break;
            default:
                break;
        }
    });
    download->accept();
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
