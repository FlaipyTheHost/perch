#include "settings_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
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
      height_(new QSpinBox(this)),
      trayIcon_(new QComboBox(this)),
      customIcon_(new QLineEdit(this)),
      browseIcon_(new QPushButton(this)),
      iconPreview_(new QLabel(this)) {
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

    trayIcon_->addItem(tr("Automatic (monochrome, follows the panel)"), QStringLiteral("auto"));
    trayIcon_->addItem(tr("Monochrome, white (dark panels)"), QStringLiteral("light"));
    trayIcon_->addItem(tr("Monochrome, black (light panels)"), QStringLiteral("dark"));
    trayIcon_->addItem(tr("Colorful (application icon)"), QStringLiteral("color"));
    trayIcon_->addItem(tr("Custom…"), QStringLiteral("custom"));
    connect(trayIcon_, &QComboBox::currentIndexChanged, this, &SettingsDialog::updateTrayIconWidgets);

    customIcon_->setClearButtonEnabled(true);
    customIcon_->setPlaceholderText(tr("Image file or icon name from your theme"));
    connect(customIcon_, &QLineEdit::textChanged, this, &SettingsDialog::updateTrayIconWidgets);
    browseIcon_->setText(tr("Browse…"));
    browseIcon_->setIcon(QIcon::fromTheme(QStringLiteral("document-open")));
    connect(browseIcon_, &QPushButton::clicked, this, &SettingsDialog::browseTrayIcon);
    iconPreview_->setFixedSize(24, 24);
    iconPreview_->setAlignment(Qt::AlignCenter);

    customRow_ = new QWidget(this);
    auto* customRow = new QHBoxLayout(customRow_);
    customRow->setContentsMargins(0, 0, 0, 0);
    customRow->addWidget(customIcon_, 1);
    customRow->addWidget(browseIcon_);
    customRow->addWidget(iconPreview_);

    auto* form = form_ = new QFormLayout;
    form->addRow(tr("Service"), presets_);
    form->addRow(tr("URL"), url_);
    form->addRow(tr("Width"), width_);
    form->addRow(tr("Height"), height_);
    form->addRow(tr("Tray icon"), trayIcon_);
    form->addRow(QString(), customRow_);

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
    const int style = trayIcon_->findData(current.trayIcon);
    trayIcon_->setCurrentIndex(style < 0 ? 0 : style);
    customIcon_->setText(current.trayIconCustom);
    updateTrayIconWidgets();
    show();
    raise();
    activateWindow();
}

namespace {

// Icon for a custom tray icon entry: an image file, or a name from the icon theme.
QIcon customIcon(const QString& text) {
    if (text.isEmpty()) return {};
    if (QFileInfo::exists(text)) {
        QIcon icon(text);
        if (!icon.pixmap(22).isNull()) return icon;
        return {};
    }
    return QIcon::fromTheme(text);
}

}  // namespace

void SettingsDialog::updateTrayIconWidgets() {
    const bool custom = trayIcon_->currentData().toString() == QLatin1String("custom");
    form_->setRowVisible(customRow_, custom);
    if (!custom) return;
    const QIcon icon = customIcon(customIcon_->text().trimmed());
    iconPreview_->setPixmap(icon.isNull() ? QPixmap() : icon.pixmap(22));
    iconPreview_->setToolTip(icon.isNull() && !customIcon_->text().trimmed().isEmpty()
                                 ? tr("Icon not found")
                                 : QString());
}

void SettingsDialog::browseTrayIcon() {
    QString start = customIcon_->text().trimmed();
    if (!QFileInfo::exists(start))
        start = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Choose a tray icon"), start,
        tr("Images (*.svg *.svgz *.png *.ico *.xpm *.jpg *.jpeg *.webp);;All files (*)"));
    if (!file.isEmpty()) customIcon_->setText(file);
}

void SettingsDialog::accept() {
    const QString style = trayIcon_->currentData().toString();
    const QString custom = customIcon_->text().trimmed();
    if (style == QLatin1String("custom") && customIcon(custom).isNull()) {
        QMessageBox::warning(this, windowTitle(),
                             custom.isEmpty() ? tr("Choose an image file or an icon name for the tray icon.")
                                              : tr("Could not load the tray icon \"%1\".").arg(custom));
        return;
    }

    Config updated = base_;
    updated.trayIcon = style;
    updated.trayIconCustom = custom;
    updated.url = Config::normalizeUrl(url_->text());
    updated.width = width_->value();
    updated.height = height_->value();
    emit saved(updated);
    QDialog::accept();
}
