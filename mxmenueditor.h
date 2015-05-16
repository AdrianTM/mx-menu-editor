/**********************************************************************
 *  mxmenueditor.h
 **********************************************************************
 * Copyright (C) 2015 MX Authors
 *
 * Authors: Adrian
 *          MX & MEPIS Community <http://forum.mepiscommunity.org>
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

#ifndef MXMENUEDITOR_H
#define MXMENUEDITOR_H

#include <QMessageBox>
#include <QFile>
#include <QTreeWidget>
#include <QComboBox>

#include "addappdialog.h"

namespace Ui {
class mxmenueditor;
}

// struct for outputing both the exit code and the strings when running a command
struct Output {
    int exit_code;
    QString str;
};


class mxmenueditor : public QDialog
{
    Q_OBJECT

protected:
    QComboBox *comboBox;

public:
    explicit mxmenueditor(QWidget *parent = 0);
    ~mxmenueditor();

    QFile config_file;
    QString getVersion(QString name);
    QString version;
    QStringList all_usr_desktop_files;
    QStringList all_local_desktop_files;
    QHash<QString, QString> hashCategories;
    QHash<QString, QString> hashInclude;
    QHash<QString, QString> hashExclude;

    void loadMenuFiles();
    void displayList(QStringList menu_items);
    void addToTree(QString file_name);
    void findReloadItem(QString base_name);
    bool isHidden(QString file_name);
    bool save();
    QString getCatName(QFile *file);
    QString findIcon(QString icon_name);
    QString findBiggest(QStringList files);
    QStringList listCategories();
    QStringList listDesktopFiles(QString search_category, QString location);
    QStringList listMenuFiles();

public slots:
    void loadApps();
    void loadApps(QTreeWidgetItem *item);
    void loadItem(QTreeWidgetItem *item, int);
    void selectCommand();
    void changeName();
    void changeCommand();
    void changeComment();
    void resetInterface();
    void enableEdit();
    void enableDelete();
    void delCategory();
    void addCategory();
    void addCategoryMsgBox();
    void addAppMsgBox();
    void changeIcon();
    void changeNotify(bool);
    void changeHide(bool);
    void changeTerminal(bool);
    void setEnabled(QString);

private slots:
    void on_buttonSave_clicked();
    void on_buttonAbout_clicked();
    void on_buttonHelp_clicked();
    void on_buttonCancel_clicked();
    void on_pushRestoreApp_clicked();

private:
    Ui::mxmenueditor *ui;
    AddAppDialog *add;
    QTreeWidgetItem *current_item;
};

Output runCmd(QString cmd);

#endif // MXSNAPSHOT_H

