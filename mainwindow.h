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

#include <QMessageBox>
#include <QFile>
#include <QTreeWidget>
#include <QComboBox>

#include "addappdialog.h"

namespace Ui {
class MainWindow;
}

// struct for outputing both the exit code and the strings when running a command
struct Output {
    int exit_code;
    QString str;
};


class MainWindow : public QDialog
{
    Q_OBJECT

protected:
    QComboBox *comboBox;

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    Output getCmdOut(const QString &cmd);
    QFile config_file;
    QHash<QString, QString> hashCategories;
    QHash<QString, QString> hashExclude;
    QHash<QString, QString> hashInclude;
    QStringList all_local_desktop_files;
    QStringList all_usr_desktop_files;

    QString findIcon(QString icon_name);
    QString findLargest(const QStringList &files);
    QString getCatName(const QString &file_name);
    QString pickIcon(const QStringList &icons, const QSize &size);
    QStringList listCategories();
    QStringList listDesktopFiles(const QString &search_category, const QString &location);
    QStringList listMenuFiles();
    QTreeWidgetItem* addToTree(QString file_name);
    bool isHidden(const QString &file_name);
    bool save();
    void displayList(QStringList menu_items);
    void findReloadItem(QString base_name);
    void loadMenuFiles();

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
    void setEnabled(QString);

private slots:
    void on_buttonAbout_clicked();
    void on_buttonCancel_clicked();
    void on_buttonHelp_clicked();
    void on_buttonSave_clicked();
    void on_pushRestoreApp_clicked();

private:
    Ui::MainWindow *ui;
    AddAppDialog *add;
    QTreeWidgetItem *current_item;
};

#endif // MAINWINDOW_H

