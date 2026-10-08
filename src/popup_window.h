#pragma once

#include <QElapsedTimer>
#include <QWidget>

#include "config.h"

class QWebEnginePage;
class QWebEngineProfile;
class QWebEngineView;
class QEvent;

// Regular Qt window hosting the AI chat, placed on the right side of the screen (above the panel).
// Losing focus (clicking outside) hides it; the app keeps running in the tray.
class PopupWindow : public QWidget {
    Q_OBJECT
public:
    explicit PopupWindow(const Config& config, QWidget* parent = nullptr);
    ~PopupWindow() override;

    void toggle();
    void showPopup();
    void hidePopup();
    void applyConfig(const Config& config);

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void buildUi();
    void loadPage();
    void moveToRightSide();
    void hideIfFocusLost();
    [[nodiscard]] QString welcomeHtml() const;

    QWebEngineProfile* profile_ = nullptr;
    QWebEnginePage* page_ = nullptr;
    QWebEngineView* view_ = nullptr;

    Config config_;
    bool initialized_ = false;
    QElapsedTimer lastAutoHide_;
};
