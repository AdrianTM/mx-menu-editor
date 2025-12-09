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
#include "addappdialog.h"
#include "ui_addappdialog.h"

#include <QDir>
#include <QProcess>

AddAppDialog::AddAppDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddAppDialog)
{
    ui->setupUi(this);
    this->setWindowTitle(tr("Add Custom Application"));
    setConnections();
}

AddAppDialog::~AddAppDialog() { delete ui; }

void AddAppDialog::pushSave_clicked()
{
    QString output;
    QString file_name = ui->lineEditName->text().replace(QLatin1String(" "), QLatin1String("-")) + ".desktop";
    QString out_name = QDir::homePath() + "/.local/share/applications/" + file_name;
    const QString app_dir = QDir::homePath() + "/.local/share/applications/";
    if (!QDir().exists(app_dir) && !QDir().mkpath(app_dir)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not create application directory"));
        return;
    }
    QFile out(out_name);
    if (!out.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not save the file"));
        return;
    }
    output = QStringLiteral("[Desktop Entry]\n");
    output.append("Name=" + ui->lineEditName->text() + "\n");
    output.append("Exec=" + ui->lineEditCommand->text() + "\n");
    if (!ui->lineEditComment->text().isEmpty())
        output.append("Comment=" + ui->lineEditComment->text() + "\n");
    if (!icon_path.isEmpty())
        output.append("Icon=" + icon_path + "\n");
    if (ui->checkStartup->checkState() == Qt::Checked)
        output.append("StartupNotify=true\n");
    if (ui->checkTerminal->checkState() == Qt::Checked)
        output.append("Terminal=true\n");
    else
        output.append("Terminal=false\n");
    output.append("Type=Application\n");
    QString categories = QStringLiteral("Categories=");
    for (int i = 0; i < ui->listWidgetCategories->count(); ++i) {
        auto *item = ui->listWidgetCategories->item(i);
        categories += item->text() + ";";
    }
    output.append(categories + "\n");
    out.write(output.toUtf8());
    out.flush();
    out.close();
    QProcess::execute(QStringLiteral("xfce4-panel"), {QStringLiteral("--restart")});
    resetInterface();
    this->close();
}

void AddAppDialog::pushCancel_clicked()
{
    if (ui->pushSave->isEnabled() && !saveOrNot())
        return;
    resetInterface();
    this->close();
}

// ask whether to save edits or not
bool AddAppDialog::saveOrNot()
{
    if (!ui->pushSave->isEnabled())
        return true;

    const auto ans = QMessageBox::question(this, tr("Save changes?"), tr("Do you want to save your edits?"),
                                           QMessageBox::Save | QMessageBox::Discard);
    if (ans == QMessageBox::Save)
        pushSave_clicked();
    else
        ui->pushSave->setDisabled(true);
    return true; // treat discard as continue
}

// clear all the GUI items
void AddAppDialog::resetInterface()
{
    icon_path.clear();
    ui->lineEditName->clear();
    ui->lineEditCommand->clear();
    ui->lineEditComment->clear();
    ui->checkStartup->setChecked(false);
    ui->checkTerminal->setChecked(false);
    ui->pushChangeIcon->setIcon(QIcon());
    ui->pushChangeIcon->setText(tr("Set icon"));
    ui->pushSave->setDisabled(true);
    ui->listWidgetCategories->clear();
}

void AddAppDialog::setConnections() const
{
    connect(ui->pushSave, &QPushButton::clicked, this, &AddAppDialog::pushSave_clicked);
    connect(ui->pushCancel, &QPushButton::clicked, this, &AddAppDialog::pushCancel_clicked);
}
