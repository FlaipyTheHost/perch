#pragma once

#include <QDialog>

#include "config.h"

class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    void edit(const Config& current);

signals:
    void saved(const Config& config);

private:
    void accept() override;
    void updateTrayIconWidgets();
    void browseTrayIcon();

    QComboBox* presets_;
    QLineEdit* url_;
    QSpinBox* width_;
    QSpinBox* height_;
    QComboBox* trayIcon_;
    QLineEdit* customIcon_;
    QPushButton* browseIcon_;
    QLabel* iconPreview_;
    QFormLayout* form_ = nullptr;
    QWidget* customRow_ = nullptr;
    Config base_;
};
