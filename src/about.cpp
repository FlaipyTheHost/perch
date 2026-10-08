#include "about.h"
#include "config.h"

#include <KAboutApplicationDialog>
#include <QCoreApplication>
#include <QIcon>

namespace Perch {

    KAboutData createAboutData() {
        KAboutData aboutData(
            QStringLiteral("perch"),
                             QCoreApplication::translate("About", "Perch"),
                             QStringLiteral("0.4.0"),
                             QCoreApplication::translate("About", "A lightweight popup launcher for your favorite web services."),
                             KAboutLicense::GPL_V3,
                             QCoreApplication::translate("About", "© 2026 Carlos Araújo"),
                             QString(),
                             QStringLiteral("https://github.com/FlaipyTheHost/perch")
        );

        aboutData.addAuthor(
            QCoreApplication::translate("About", "Carlos Araújo"),
                            QCoreApplication::translate("About", "Lead Developer & Maintainer"),
                            QString(),
                            QStringLiteral("https://github.com/FlaipyTheHost")
        );

        aboutData.setProgramLogo(QIcon::fromTheme(QStringLiteral("io.FlaipyTheHost.Perch")));

        return aboutData;
    }

    void showAboutDialog(QWidget* parent) {
        const KAboutData aboutData = createAboutData();
        KAboutData::setApplicationData(aboutData);

        auto* dialog = new KAboutApplicationDialog(aboutData, parent);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    }

} // namespace Perch
