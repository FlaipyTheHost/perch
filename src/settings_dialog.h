#pragma once

#include <QDialog>

#include "config.h"

class QComboBox;
class QLineEdit;
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

    QComboBox* presets_;
    QLineEdit* url_;
    QSpinBox* width_;
    QSpinBox* height_;
    Config base_;
};
