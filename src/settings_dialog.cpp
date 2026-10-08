#include "settings_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

struct Preset {
    const char* name;
    const char* url;
};

// Just shortcuts; any web address works.
constexpr Preset kPresets[] = {
    {"Gemini", "https://gemini.google.com/app"},
    {"ChatGPT", "https://chatgpt.com"},
    {"Claude", "https://claude.ai"},
    {"Copilot", "https://copilot.microsoft.com"},
    {"Perplexity", "https://www.perplexity.ai"},
    {"Mistral Le Chat", "https://chat.mistral.ai"},
    {"DeepSeek", "https://chat.deepseek.com"},
};

}  // namespace

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent),
      presets_(new QComboBox(this)),
      url_(new QLineEdit(this)),
      width_(new QSpinBox(this)),
      height_(new QSpinBox(this)) {
    setWindowTitle(tr("Perch Settings"));

    presets_->addItem(tr("Choose a service…"), QString{});
    for (const auto& preset : kPresets) {
        presets_->addItem(QString::fromUtf8(preset.name), QString::fromUtf8(preset.url));
    }
    connect(presets_, &QComboBox::activated, this, [this](int index) {
        const QString url = presets_->itemData(index).toString();
        if (!url.isEmpty()) url_->setText(url);
    });

    url_->setMinimumWidth(360);
    url_->setClearButtonEnabled(true);
    url_->setPlaceholderText(tr("Any web address, e.g. https://example.com/chat"));

    for (auto* spin : {width_, height_}) {
        spin->setRange(Config::kMinSize, Config::kMaxSize);
        spin->setSingleStep(10);
        spin->setSuffix(QStringLiteral(" px"));
    }

    auto* form = new QFormLayout;
    form->addRow(tr("Service"), presets_);
    form->addRow(tr("URL"), url_);
    form->addRow(tr("Width"), width_);
    form->addRow(tr("Height"), height_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText(tr("Save"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

void SettingsDialog::edit(const Config& current) {
    base_ = current;
    presets_->setCurrentIndex(0);
    url_->setText(current.url);
    width_->setValue(current.width);
    height_->setValue(current.height);
    show();
    raise();
    activateWindow();
}

void SettingsDialog::accept() {
    Config updated = base_;
    updated.url = Config::normalizeUrl(url_->text());
    updated.width = width_->value();
    updated.height = height_->value();
    emit saved(updated);
    QDialog::accept();
}
