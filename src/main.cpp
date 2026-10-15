#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <QDesktopServices>
#include <QFileInfo>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLocale>
#include <QMenu>
#include <QStyleHints>
#include <QSystemTrayIcon>
#include <QUrl>
#include <QTranslator>
#include <QMessageBox>

#include "config.h"
#include "popup_window.h"
#include "about.h"
#include "settings_dialog.h"

namespace {

constexpr auto kAppId = "io.FlaipyTheHost.Perch";

// Loads Perch's translations (English is the source language) and Qt's own
// (web view context menus etc.) according to the system language.
// Perch's .qm files are embedded in the executable (resource prefix :/i18n).
void installTranslations(QApplication& app) {
    for (const char* name : {"qtbase", "qtwebengine"}) {
        auto* translator = new QTranslator(&app);
        if (translator->load(QLocale(), QString::fromLatin1(name), QStringLiteral("_"),
            QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
            app.installTranslator(translator);
            } else {
                delete translator;
            }
    }

    auto* translator = new QTranslator(&app);

    bool loaded = translator->load(QLocale(), QStringLiteral("perch"), QStringLiteral("_"),
                                   QStringLiteral(":/i18n"));

    if (!loaded) {
        const QString baseLang = QLocale().name().section(QLatin1Char('_'), 0, 0);
        loaded = translator->load(QStringLiteral("perch_") + baseLang, QStringLiteral(":/i18n"));
    }

    if (loaded) {
        app.installTranslator(translator);
    } else {
        delete translator;
    }
}

// Tray icon for the style chosen in Settings (see Config::trayIcon).
QIcon trayIconFor(const Config& config, const QIcon& appIcon) {
    if (config.trayIcon == QLatin1String("color")) return appIcon;

    if (config.trayIcon == QLatin1String("custom") && !config.trayIconCustom.isEmpty()) {
        const QString& custom = config.trayIconCustom;
        if (QFileInfo::exists(custom)) {
            const QIcon icon(custom);
            if (!icon.availableSizes().isEmpty() || !icon.pixmap(22).isNull()) return icon;
        }
        if (QIcon::hasThemeIcon(custom)) return QIcon::fromTheme(custom);
        qWarning() << "Perch: custom tray icon not found:" << custom;
    }

    if (config.trayIcon == QLatin1String("light")) return QIcon(QStringLiteral(":/tray-light.svg"));
    if (config.trayIcon == QLatin1String("dark")) return QIcon(QStringLiteral(":/tray-dark.svg"));

    // Automatic: the installed symbolic icon is passed to Plasma by name, which recolors it
    // to match the panel. Without it (e.g. running from the build dir), follow the color scheme.
    const QString symbolic = QString::fromLatin1(kAppId) + QStringLiteral("-symbolic");
    if (QIcon::hasThemeIcon(symbolic)) return QIcon::fromTheme(symbolic);
    const bool dark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    return QIcon(dark ? QStringLiteral(":/tray-light.svg") : QStringLiteral(":/tray-dark.svg"));
}

// Shows a downloaded file in the file manager (selected, when it supports FileManager1),
// otherwise just opens its folder.
void revealFile(const QString& path) {
    const QUrl url = QUrl::fromLocalFile(path);
    QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("/org/freedesktop/FileManager1"),
        QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("ShowItems"));
    call << QStringList{url.toString()} << QString();
    const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 2000);
    if (reply.type() != QDBusMessage::ReplyMessage)
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

}  // namespace

int main(int argc, char* argv[]) {
    // On native Wayland an app can't position its own window (only the compositor decides).
    // Through XWayland (xcb) Qt positions it normally; if xcb is unavailable it falls back to Wayland.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "xcb;wayland");
    }

    // Without Client Hints (Sec-CH-UA), Chromium doesn't reveal its real identity under the User-Agent.
    // Must be set before the QApplication is created.
    {
        QByteArray flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
        if (!flags.contains("UserAgentClientHint")) {
            if (!flags.isEmpty()) flags += ' ';
            flags += "--disable-features=UserAgentClientHint";
            qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
        }
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("perch"));
    QApplication::setApplicationDisplayName(QStringLiteral("Perch"));
    QApplication::setDesktopFileName(QString::fromLatin1(kAppId));
    QApplication::setQuitOnLastWindowClosed(false);
    installTranslations(app);

    auto openAbout = [&] {
        Perch::showAboutDialog();
    };

    // Single instance: running `perch` again (e.g. from a keyboard shortcut) toggles the first one's window.
    {
        QLocalSocket probe;
        probe.connectToServer(QString::fromLatin1(kAppId));
        if (probe.waitForConnected(200)) {
            probe.write("toggle");
            probe.waitForBytesWritten(200);
            return 0;
        }
    }
    QLocalServer::removeServer(QString::fromLatin1(kAppId));
    QLocalServer server;
    server.listen(QString::fromLatin1(kAppId));

    const QIcon icon = QIcon::fromTheme(QString::fromLatin1(kAppId),
                                        QIcon(QStringLiteral(":/io.FlaipyTheHost.Perch.svg")));
    QApplication::setWindowIcon(icon);

    Config config = Config::load();
    PopupWindow popup(config);
    SettingsDialog settings;

    auto openSettings = [&] {
        popup.hidePopup();
        settings.edit(config);
    };

    QMenu menu;  // declared before the tray icon, which keeps a pointer to it
    QSystemTrayIcon tray(trayIconFor(config, icon));
    auto refreshTrayIcon = [&] { tray.setIcon(trayIconFor(config, icon)); };

    QObject::connect(&settings, &SettingsDialog::saved, [&](const Config& updated) {
        config = updated;
        config.save();
        popup.applyConfig(config);
        refreshTrayIcon();
    });
    // The automatic fallback icon depends on the color scheme.
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, &tray, refreshTrayIcon);
    QObject::connect(&server, &QLocalServer::newConnection, [&] {
        if (auto* socket = server.nextPendingConnection()) {
            socket->deleteLater();
            popup.toggle();
        }
    });

    menu.addAction(QIcon::fromTheme(QStringLiteral("configure")),
                   QCoreApplication::translate("Tray", "Settings"), openSettings);

    menu.addAction(QIcon::fromTheme(QStringLiteral("help-about")),
                   QCoreApplication::translate("Tray", "About"), openAbout);

    menu.addSeparator();
    menu.addAction(QIcon::fromTheme(QStringLiteral("application-exit")),
                   QCoreApplication::translate("Tray", "Quit"), &app, &QApplication::quit);

    tray.setToolTip(QStringLiteral("Perch"));
    tray.setContextMenu(&menu);
    QObject::connect(&tray, &QSystemTrayIcon::activated, [&](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) popup.toggle();
    });
    tray.show();

    // Download notifications; clicking the last one shows the file in the file manager.
    QString lastDownload;
    QObject::connect(&popup, &PopupWindow::downloadFinished, &tray, [&](const QString& path) {
        lastDownload = path;
        tray.showMessage(QCoreApplication::translate("Tray", "Download complete"),
                         QCoreApplication::translate("Tray", "%1 was saved to %2")
                             .arg(QFileInfo(path).fileName(), QFileInfo(path).absolutePath()),
                         icon, 6000);
    });
    QObject::connect(&popup, &PopupWindow::downloadFailed, &tray, [&](const QString& name, const QString& reason) {
        lastDownload.clear();
        tray.showMessage(QCoreApplication::translate("Tray", "Download failed"),
                         reason.isEmpty() ? name : QStringLiteral("%1: %2").arg(name, reason),
                         QSystemTrayIcon::Warning, 6000);
    });
    QObject::connect(&tray, &QSystemTrayIcon::messageClicked, [&] {
        if (!lastDownload.isEmpty()) revealFile(lastDownload);
    });

    // First run: no service chosen yet, so open Settings right away.
    if (config.url.isEmpty()) openSettings();

    return app.exec();
}
