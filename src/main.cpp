/**********************************************************************
 *  main.cpp
 **********************************************************************
 * Copyright (C) 2015 MX Authors
 *
 * Authors: Adrian
 *          MX Linux <http://mxlinux.org>
 *
 * This file is part of MX Menu Editor.
 *
 * MX Menu Editor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MX Menu Editor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MX Menu Editor.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#include <QApplication>
#include <QMessageBox>
#include <QLibraryInfo>

#include "mainwindow.h"
#include <QIcon>
#include <qlocale.h>
#include <qtranslator.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    // Set Qt platform to XCB (X11) if not already set and we're in X11 environment
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        if (!qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
            qputenv("QT_QPA_PLATFORM", "xcb");
        }
    }

    QApplication app(argc, argv);
    QApplication::setWindowIcon(QIcon::fromTheme(QApplication::applicationName()));
    QApplication::setOrganizationName(QStringLiteral("MX-Linux"));

    QTranslator qtTran;
    if (qtTran.load(QLocale::system(), QStringLiteral("qt"), QStringLiteral("_"),
                    QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtTran);

    QTranslator qtBaseTran;
    if (qtBaseTran.load("qtbase_" + QLocale::system().name(), QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtBaseTran);

    QTranslator appTran;
    const QString localePath = QStringLiteral("/usr/share/") + QApplication::applicationName() + QStringLiteral("/locale");
    if (appTran.load(QApplication::applicationName() + "_" + QLocale::system().name(), localePath))
        QApplication::installTranslator(&appTran);

    if (getuid() != 0) {
        MainWindow w;
        w.show();
        return QApplication::exec();
    } else {
        QApplication::beep();
        QMessageBox::critical(nullptr, QString(), QApplication::tr("You must run this program as normal user."));
        return EXIT_FAILURE;
    }
}
