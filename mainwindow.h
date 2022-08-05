/**********************************************************************
 *  mainwindow.h
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QComboBox>
#include <QFile>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QTreeWidget>

#include "addappdialog.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QDialog
{
    Q_OBJECT

protected:
    QComboBox *comboBox;

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    QFile config_file;
    QMultiHash<QString, QString> hashCategories;
    QMultiHash<QString, QString> hashExclude;
    QMultiHash<QString, QString> hashInclude;
    QPixmap findIcon(QString icon_name, QSize size);
    QString getCatName(const QString &file_name);
    QStringList all_local_desktop_files;
    QStringList all_usr_desktop_files;
    QStringList listCategories() const;
    QStringList listDesktopFiles(const QString &search_string, const QString &location);
    static QStringList listMenuFiles();
    QTreeWidgetItem* addToTree(const QString &file_name);
    static bool isHidden(const QString &file_name);
    bool save();
    void displayList(QStringList menu_items);
    void findReloadItem(const QString &base_name);
    void loadMenuFiles();
    void setConnections();

public slots:
    void addAppMsgBox();
    void addCategory();
    void addCategoryMsgBox();
    void centerWindow();
    void changeCommand();
    void changeComment();
    void changeHide(bool);
    void changeIcon();
    void changeName();
    void changeNotify(bool);
    void changeTerminal(bool);
    void delCategory();
    void enableDelete();
    void enableEdit();
    void loadApps();
    void loadApps(QTreeWidgetItem *item);
    void loadItem(QTreeWidgetItem *item, int);
    void resetInterface();
    void saveSettings();
    void selectCommand();
    void setEnabled(const QString&);

private slots:
    void pushAbout_clicked();
    void pushCancel_clicked();
    static void pushHelp_clicked();
    void pushSave_clicked();
    void pushRestoreApp_clicked();

private:
    Ui::MainWindow *ui;
    AddAppDialog *add;
    QTreeWidgetItem *current_item{};
    QProcess proc;
    QSettings settings;
};

#endif // MAINWINDOW_H

