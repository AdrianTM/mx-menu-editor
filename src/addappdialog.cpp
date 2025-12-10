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
#include <QStandardPaths>

AddAppDialog::AddAppDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddAppDialog)
{
    ui->setupUi(this);
    this->setWindowTitle(tr("Add Custom Application"));

    // Remove focus from Cancel button
    ui->pushCancel->setAutoDefault(false);
    ui->pushCancel->setDefault(false);

    setConnections();
}

AddAppDialog::~AddAppDialog() { delete ui; }

QString AddAppDialog::sanitizeFileName(const QString &name)
{
    QString sanitized = name;
    // Remove or replace invalid filename characters
    const QString invalid_chars = QStringLiteral("/\\:*?\"<>|\0");
    for (const QChar &ch : invalid_chars) {
        sanitized.replace(ch, QLatin1String("-"));
    }
    // Replace spaces with dashes
    sanitized.replace(QLatin1Char(' '), QLatin1Char('-'));
    // Remove leading/trailing dots and spaces
    sanitized = sanitized.trimmed();
    while (sanitized.startsWith(QLatin1Char('.'))) {
        sanitized.remove(0, 1);
    }
    // Ensure it's not empty
    if (sanitized.isEmpty()) {
        sanitized = QStringLiteral("application");
    }
    return sanitized;
}

void AddAppDialog::pushSave_clicked()
{
    lastSavedPath.clear();
    lastSavedCategories.clear();

    // Validate and sanitize the application name
    QString app_name = ui->lineEditName->text().trimmed();
    if (app_name.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Application name cannot be empty"));
        return;
    }

    // Check if the Exec command is valid
    QString exec_command = ui->lineEditCommand->text().trimmed();
    if (exec_command.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Command cannot be empty"));
        return;
    }

    // Extract the executable path (first part before any arguments)
    QString executable = exec_command.split(QLatin1Char(' ')).first();
    // Remove quotes if present
    if (executable.startsWith(QLatin1Char('"')) && executable.endsWith(QLatin1Char('"'))) {
        executable = executable.mid(1, executable.length() - 2);
    }
    if (executable.startsWith(QLatin1Char('\'')) && executable.endsWith(QLatin1Char('\''))) {
        executable = executable.mid(1, executable.length() - 2);
    }

    // Check if the executable exists
    bool exec_exists = false;
    if (executable.startsWith(QLatin1Char('/'))) {
        // Absolute path - check directly
        exec_exists = QFile::exists(executable);
    } else {
        // Relative path or command name - check in PATH
        exec_exists = !QStandardPaths::findExecutable(executable).isEmpty();
    }

    if (!exec_exists) {
        auto answer = QMessageBox::question(
            this, tr("Warning"),
            tr("The executable '%1' does not exist or is not in PATH.\nDo you want to continue anyway?")
                .arg(executable),
            QMessageBox::Yes | QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    QString output;
    QString file_name = sanitizeFileName(app_name) + ".desktop";
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
    lastSavedPath = out_name;
    lastSavedCategories = selectedCategories();
    if (QProcess::execute(QStringLiteral("pgrep"), {QStringLiteral("xfce4-panel")}) == 0)
        QProcess::execute(QStringLiteral("xfce4-panel"), {QStringLiteral("--restart")});
    resetInterface();
    accept();
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

QStringList AddAppDialog::selectedCategories() const
{
    QStringList categories;
    categories.reserve(ui->listWidgetCategories->count());
    for (int i = 0; i < ui->listWidgetCategories->count(); ++i) {
        categories << ui->listWidgetCategories->item(i)->text();
    }
    return categories;
}
