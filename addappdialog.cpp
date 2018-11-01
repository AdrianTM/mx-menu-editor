/**********************************************************************
 *  addappdialog.cpp
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

#include <QDir>

#include "addappdialog.h"
#include "ui_addappdialog.h"

//#include <QDebug>

AddAppDialog::AddAppDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddAppDialog)
{
    ui->setupUi(this);
    this->setWindowTitle(tr("Add Custom Application"));
}

AddAppDialog::~AddAppDialog()
{
    delete ui;
}

// Save button clicked
void AddAppDialog::on_buttonSave_clicked()
{
    QDir dir;
    QString output;
    QString file_name = ui->lineEditName->text().replace(" ", "-") + ".desktop";
    QString out_name = dir.homePath() + "/.local/share/applications/" + file_name;
    QFile out(out_name);
    if (!out.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::critical(0, tr("Error"), tr("Could not save the file"));
    }
    output = "[Desktop Entry]\n";
    output.append("Name=" + ui->lineEditName->text() + "\n");
    output.append("Exec=" + ui->lineEditCommand->text() + "\n");
    if (ui->lineEditComment->text() != 0) {
        output.append("Comment=" + ui->lineEditComment->text() + "\n");
    }
    if (icon_path != "") {
        output.append("Icon=" + icon_path + "\n");
    }
    if (ui->checkStartup->checkState()) {
        output.append("StartupNotify=true\n");
    }
    if (ui->checkTerminal->checkState()) {
        output.append("Terminal=true\n");
    } else {
        output.append("Terminal=false\n");
    }
    output.append("Type=Application\n");
    QString categories = "Categories=";
    for(int i = 0; i < ui->listWidgetCategories->count(); ++i)
    {
        QListWidgetItem* item = ui->listWidgetCategories->item(i);
        categories += item->text() + ";";
    }
    output.append(categories + "\n");
    out.write(output.toUtf8());
    out.flush();
    out.close();
    system("xfce4-panel --restart");
    resetInterface();
    this->close();
}

// Cancel button clicked
void AddAppDialog::on_buttonCancel_clicked()
{
    if (ui->buttonSave->isEnabled()) {
        saveOrNot();
    }
    resetInterface();
    this->close();
}

// ask whether to save edits or not
void AddAppDialog::saveOrNot()
{
    if (ui->buttonSave->isEnabled()) {
        int ans = QMessageBox::question(0, tr("Save changes?"), tr("Do you want to save your edits?"), tr("Save"), tr("Cancel"));
        if (ans == 0) {
            on_buttonSave_clicked();
        } else {
            ui->buttonSave->setDisabled(true);
        }
    }
}

// clear all the GUI items
void AddAppDialog::resetInterface()
{
    icon_path = "";
    ui->lineEditName->setText("");
    ui->lineEditCommand->setText("");
    ui->lineEditComment->setText("");
    ui->checkStartup->setChecked(false);
    ui->checkTerminal->setChecked(false);
    ui->pushChangeIcon->setIcon(QIcon());
    ui->pushChangeIcon->setText(tr("Set icon"));
    ui->buttonSave->setDisabled(true);
    ui->listWidgetCategories->clear();
}
