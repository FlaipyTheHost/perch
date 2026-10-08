#ifndef ABOUT_H
#define ABOUT_H

#include <KAboutData>
#include <QWidget>

namespace Perch {

/**
 * Creates and returns the application metadata using KF6's KAboutData.
 */
KAboutData createAboutData();

/**
 * Shows the native KDE Plasma "About" dialog containing Perch information,
 * credits, license, and the standard "About KDE" tab.
 */
void showAboutDialog(QWidget* parent = nullptr);

} // namespace Perch

#endif // ABOUT_H
