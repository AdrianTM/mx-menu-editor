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
 * MX Menu Editor is distributed in the hope that it will be useful,
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
#include <QFile>
#include <QStandardPaths>

#include "mainwindow.h"

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

// Newlines and control characters would corrupt the single-line Key=Value
// format of a .desktop entry; shared by Add and Edit field validation.
bool AddAppDialog::containsInvalidDesktopChars(const QString &text)
{
    for (const QChar &ch : text) {
        if (ch == QLatin1Char('\n') || ch == QLatin1Char('\r')) {
            return true;
        }
        if (ch.unicode() < 32 && ch != QLatin1Char('\t')) {
            return true;
        }
    }
    return false;
}

bool AddAppDialog::validateApplicationName(const QString &name, QString &errorMessage)
{
    errorMessage.clear();
    if (name.isEmpty()) {
        errorMessage = tr("Application name cannot be empty");
        return false;
    }

    if (name.length() > 255) {
        errorMessage = tr("Application name is too long (maximum 255 characters)");
        return false;
    }

    if (containsInvalidDesktopChars(name)) {
        errorMessage = tr("Application name cannot contain newlines or control characters");
        return false;
    }

    return true;
}

// Checks whether the command's executable exists, prompting the user to continue
// anyway if not. Shared by the Add dialog's Save validation and the main editor's
// Save action, so both flows ask the same question with the same wording.
bool AddAppDialog::confirmExecutableExists(QWidget *parent, const QString &command)
{
    const QString executable = parseCommandExecutable(command);
    if (executable.isEmpty() || checkExecutableExists(executable)) {
        return true;
    }

    // Remove surrounding quotes for display (e.g., "/path/to app" -> /path/to app)
    QString cleanExecutable = executable;
    if ((cleanExecutable.startsWith(QLatin1Char('"')) && cleanExecutable.endsWith(QLatin1Char('"'))) ||
        (cleanExecutable.startsWith(QLatin1Char('\'')) && cleanExecutable.endsWith(QLatin1Char('\'')))) {
        cleanExecutable = cleanExecutable.mid(1, cleanExecutable.length() - 2);
    }
    const auto answer = QMessageBox::question(
        parent, tr("Warning"),
        tr("The executable '%1' does not exist or is not in PATH.\nDo you want to continue anyway?")
            .arg(cleanExecutable),
        QMessageBox::Yes | QMessageBox::No);
    return answer == QMessageBox::Yes;
}

bool AddAppDialog::validateCommand(const QString &command, QString &errorMessage)
{
    errorMessage.clear();
    if (command.isEmpty()) {
        errorMessage = tr("Command cannot be empty");
        return false;
    }

    if (command.length() > 1024) {
        errorMessage = tr("Command is too long (maximum 1024 characters)");
        return false;
    }

    // Check for newlines that would corrupt .desktop file format
    if (command.contains(QLatin1Char('\n')) || command.contains(QLatin1Char('\r'))) {
        errorMessage = tr("Command cannot contain newlines");
        return false;
    }

    // Note: .desktop specification allows all shell metacharacters in Exec field
    // Users are responsible for creating valid desktop entries

    return confirmExecutableExists(this, command);
}

bool AddAppDialog::validateComment(const QString &comment, QString &errorMessage)
{
    errorMessage.clear();
    if (comment.length() > 512) {
        errorMessage = tr("Comment is too long (maximum 512 characters)");
        return false;
    }

    if (containsInvalidDesktopChars(comment)) {
        errorMessage = tr("Comment cannot contain newlines or control characters");
        return false;
    }

    return true;
}

bool AddAppDialog::validateIconPath(const QString &iconPath, QString &errorMessage)
{
    errorMessage.clear();
    if (iconPath.isEmpty()) {
        // Empty icon path is allowed
        return true;
    }

    if (iconPath.length() > 512) {
        errorMessage = tr("Icon path is too long (maximum 512 characters)");
        return false;
    }

    if (containsInvalidDesktopChars(iconPath)) {
        errorMessage = tr("Icon path cannot contain newlines or control characters");
        return false;
    }

    return true;
}

bool AddAppDialog::checkExecutableExists(const QString &executable)
{
    if (executable.isEmpty()) {
        return false;
    }

    // Remove surrounding quotes if present (e.g., "/path/to app" -> /path/to app)
    QString cleanExecutable = executable;
    if ((cleanExecutable.startsWith(QLatin1Char('"')) && cleanExecutable.endsWith(QLatin1Char('"'))) ||
        (cleanExecutable.startsWith(QLatin1Char('\'')) && cleanExecutable.endsWith(QLatin1Char('\'')))) {
        cleanExecutable = cleanExecutable.mid(1, cleanExecutable.length() - 2);
    }

    // Check if the executable exists
    if (cleanExecutable.startsWith(QLatin1Char('/'))) {
        // Absolute path - check directly
        return QFile::exists(cleanExecutable);
    }
    // Relative path or command name - check in PATH
    return !QStandardPaths::findExecutable(cleanExecutable).isEmpty();
}

QString AddAppDialog::parseCommandExecutable(const QString &command)
{
    // Parse command line respecting quotes (similar to shell parsing)
    QString result;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool escaped = false;

    for (int i = 0; i < command.length(); ++i) {
        QChar ch = command.at(i);

        if (escaped) {
            result.append(ch);
            escaped = false;
            continue;
        }

        if (ch == QLatin1Char('\\')) {
            escaped = true;
            continue;
        }

        if (ch == QLatin1Char('\'') && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
            continue;
        }

        if (ch == QLatin1Char('"') && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            continue;
        }

        if (ch == QLatin1Char(' ') && !inSingleQuote && !inDoubleQuote) {
            // Found first complete token
            if (!result.isEmpty()) {
                break;
            }
            continue; // Skip leading spaces
        }

        result.append(ch);
    }

    return result.trimmed();
}

QString AddAppDialog::sanitizeFileName(const QString &name)
{
    QString sanitized = name;
    // Remove or replace invalid filename characters
    static const QString invalidChars = QStringLiteral("/\\:*?\"<>|");
    for (const QChar &ch : invalidChars) {
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

bool AddAppDialog::pushSave_clicked()
{
    lastSavedPath.clear();
    lastSavedCategories.clear();

    // Validate application name
    QString appName = ui->lineEditName->text().trimmed();
    QString errorMessage;
    if (!validateApplicationName(appName, errorMessage)) {
        QMessageBox::warning(this, tr("Error"), errorMessage);
        return false;
    }

    // Validate command
    QString execCommand = ui->lineEditCommand->text().trimmed();
    if (!validateCommand(execCommand, errorMessage)) {
        QMessageBox::warning(this, tr("Error"), errorMessage);
        return false;
    }

    // Validate comment
    QString comment = ui->lineEditComment->text().trimmed();
    if (!validateComment(comment, errorMessage)) {
        QMessageBox::warning(this, tr("Error"), errorMessage);
        return false;
    }

    // Validate icon path
    QString iconPath = this->icon_path;
    if (!validateIconPath(iconPath, errorMessage)) {
        QMessageBox::warning(this, tr("Error"), errorMessage);
        return false;
    }

    QString output;
    QString fileName = sanitizeFileName(appName) + ".desktop";
    const QString appDir = MainWindow::localApplicationsPath();
    QString outName = appDir + QLatin1Char('/') + fileName;
    if (!QDir().exists(appDir) && !QDir().mkpath(appDir)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not create application directory"));
        return false;
    }
    QFile out(outName);
    if (!out.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not save the file"));
        return false;
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
    lastSavedPath = outName;
    lastSavedCategories = selectedCategories();
    MainWindow::restartPanel();
    resetInterface();
    accept();
    return true;
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
        return pushSave_clicked(); // Continue only if the save actually succeeded
    ui->pushSave->setDisabled(true);
    return true; // Discarded, continue
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
