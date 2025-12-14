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
    Ui::AddAppDialog *ui;
    QString icon_path;
    QString lastSavedPath;
    QStringList lastSavedCategories;
    void resetInterface();
    void setConnections() const;
    [[nodiscard]] QStringList selectedCategories() const;
    [[nodiscard]] bool validateApplicationName(const QString &name, QString *errorMessage = nullptr);
    [[nodiscard]] bool validateCommand(const QString &command, QString *errorMessage = nullptr);
    [[nodiscard]] bool validateComment(const QString &comment, QString *errorMessage = nullptr);
    [[nodiscard]] bool validateIconPath(const QString &iconPath, QString *errorMessage = nullptr);
    [[nodiscard]] static QString parseCommandExecutable(const QString &command);

public slots:
    [[nodiscard]] bool saveOrNot();

private slots:
    void pushSave_clicked();
    void pushCancel_clicked();

private:
    static QString sanitizeFileName(const QString &name);
};

#endif // ADDAPPDIALOG_H
