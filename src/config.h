#pragma once

#include <QSettings>
#include <QStandardPaths>
#include <QString>

#include <algorithm>

// Stored in ~/.config/perch/config.ini
struct Config {
    static constexpr int kMinSize = 240;
    static constexpr int kMaxSize = 3000;

    QString url;  // empty on first run: the user picks a service in Settings
    int width = 420;
    int height = 640;
    int margin = 8;  // distance from the screen edge (file only)
    // User-Agent sent to websites (file only). Some sign-in pages (e.g. Google) refuse embedded
    // browsers that identify as QtWebEngine; bump the Firefox version here if that starts happening.
    QString userAgent = QStringLiteral("Mozilla/5.0 (X11; Linux x86_64; rv:140.0) Gecko/20100101 Firefox/140.0");

    [[nodiscard]] static QString normalizeUrl(QString text) {
        text = text.trimmed();
        if (text.isEmpty()) return {};
        if (!text.contains(QStringLiteral("://"))) text.prepend(QStringLiteral("https://"));
        return text;
    }

    [[nodiscard]] static Config load() {
        QSettings s(path(), QSettings::IniFormat);
        Config c;
        c.url = normalizeUrl(s.value(QStringLiteral("window/url")).toString());
        c.width = std::clamp(s.value(QStringLiteral("window/width"), c.width).toInt(), kMinSize, kMaxSize);
        c.height = std::clamp(s.value(QStringLiteral("window/height"), c.height).toInt(), kMinSize, kMaxSize);
        c.margin = std::clamp(s.value(QStringLiteral("window/margin"), c.margin).toInt(), 0, 200);
        c.userAgent = s.value(QStringLiteral("network/user_agent"), c.userAgent).toString();
        return c;
    }

    void save() const {
        QSettings s(path(), QSettings::IniFormat);
        s.setValue(QStringLiteral("window/url"), url);
        s.setValue(QStringLiteral("window/width"), width);
        s.setValue(QStringLiteral("window/height"), height);
        s.setValue(QStringLiteral("window/margin"), margin);
        s.setValue(QStringLiteral("network/user_agent"), userAgent);
    }

private:
    [[nodiscard]] static QString path() {
        return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
               QStringLiteral("/perch/config.ini");
    }
};
