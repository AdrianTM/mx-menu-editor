/**********************************************************************
 *  addappdialog.h
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

#ifndef ADDAPPDIALOG_H
#define ADDAPPDIALOG_H

#include <QDialog>
#include <QMessageBox>
#include <QStringList>

namespace Ui
{
class AddAppDialog;
}

class AddAppDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddAppDialog(QWidget *parent = nullptr);
    ~AddAppDialog();
    void resetInterface();
    void setConnections() const;
    void addCategoryToList(const QString &category);
    [[nodiscard]] QStringList selectedCategories() const;
    [[nodiscard]] static bool validateApplicationName(const QString &name, QString &errorMessage);
    [[nodiscard]] bool validateCommand(const QString &command, QString &errorMessage);
    [[nodiscard]] static bool validateComment(const QString &comment, QString &errorMessage);
    [[nodiscard]] static bool validateIconPath(const QString &iconPath, QString &errorMessage);
    [[nodiscard]] static QString parseCommandExecutable(const QString &command);
    [[nodiscard]] static bool checkExecutableExists(const QString &executable);
    [[nodiscard]] static bool containsInvalidDesktopChars(const QString &text);
    [[nodiscard]] static bool confirmExecutableExists(QWidget *parent, const QString &command);

signals:
    // Emitted after a successful save, carrying the saved file's path and categories
    // (the dialog clears its own fields immediately after, so this is the one chance
    // to hand the result to whoever is listening).
    void appSaved(const QString &path, const QStringList &categories);
    // Emitted when the user wants to add a category; the picker itself needs the
    // full freedesktop.org category list, which only MainWindow knows.
    void addCategoryRequested();

public slots:
    bool saveOrNot();

private slots:
    bool pushSave_clicked();
    void pushCancel_clicked();
    void pushChangeIcon_clicked();
    void selectCommand_clicked();
    void removeCurrentCategory();
    void updateSaveButtonState();

private:
    void setIconPath(const QString &path);
    static QString sanitizeFileName(const QString &name);

    Ui::AddAppDialog *ui;
    QString icon_path;
};

#endif // ADDAPPDIALOG_H
