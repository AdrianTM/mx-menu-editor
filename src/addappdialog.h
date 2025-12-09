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
 *  is distributed in the hope that it will be useful,
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
    void resetInterface();
    void setConnections() const;

public slots:
    bool saveOrNot();

private slots:
    void pushSave_clicked();
    void pushCancel_clicked();

private:
};

#endif // ADDAPPDIALOG_H
