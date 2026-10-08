#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLocale>
#include <QMenu>
#include <QSystemTrayIcon>
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

    QObject::connect(&settings, &SettingsDialog::saved, [&](const Config& updated) {
        config = updated;
        config.save();
        popup.applyConfig(config);
    });
    QObject::connect(&server, &QLocalServer::newConnection, [&] {
        if (auto* socket = server.nextPendingConnection()) {
            socket->deleteLater();
            popup.toggle();
        }
    });

    QMenu menu;
    menu.addAction(QIcon::fromTheme(QStringLiteral("configure")),
                   QCoreApplication::translate("Tray", "Settings"), openSettings);

    menu.addAction(QIcon::fromTheme(QStringLiteral("help-about")),
                   QCoreApplication::translate("Tray", "About"), openAbout);

    menu.addSeparator();
    menu.addAction(QIcon::fromTheme(QStringLiteral("application-exit")),
                   QCoreApplication::translate("Tray", "Quit"), &app, &QApplication::quit);

    QSystemTrayIcon tray(icon);
    tray.setToolTip(QStringLiteral("Perch"));
    tray.setContextMenu(&menu);
    QObject::connect(&tray, &QSystemTrayIcon::activated, [&](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) popup.toggle();
    });
    tray.show();

    // First run: no service chosen yet, so open Settings right away.
    if (config.url.isEmpty()) openSettings();

    return app.exec();
}
