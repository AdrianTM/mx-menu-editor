/**********************************************************************
 *  mainwindow.cpp
 **********************************************************************
 * Copyright (C) 2015-2022 MX Authors
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
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QFileDialog>
#include <QFormLayout>
#include <QScreen>
#include <utility>

#include "ui_addappdialog.h"

MainWindow::MainWindow(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MainWindow)
    , add(new AddAppDialog)
{
    qDebug().noquote() << QCoreApplication::applicationName() << "version:" << VERSION;
    connect(QApplication::instance(), &QApplication::aboutToQuit, this, &MainWindow::saveSettings);
    ui->setupUi(this);
    setWindowFlags(Qt::Window); // for the close, min and max buttons
    setConnections();

    if (ui->pushSave->icon().isNull())
        ui->pushSave->setIcon(QIcon(":/icons/dialog-ok.svg"));
    if (add->ui->pushSave->icon().isNull())
        add->ui->pushSave->setIcon(QIcon(":/icons/dialog-ok.svg"));

    comboBox = new QComboBox;

    all_local_desktop_files = listDesktopFiles(QLatin1String(""), QDir::homePath() + "/.local/share/applications");
    all_usr_desktop_files = listDesktopFiles(QLatin1String(""), QStringLiteral("/usr/share/applications"));

    resetInterface();
    loadMenuFiles();

    connect(ui->treeWidget, &QTreeWidget::itemSelectionChanged, this,
            static_cast<void (MainWindow::*)()>(&MainWindow::loadApps));
    connect(ui->treeWidget, &QTreeWidget::itemExpanded, this,
            static_cast<void (MainWindow::*)(QTreeWidgetItem *)>(&MainWindow::loadApps));
    connect(ui->toolButtonCommand, &QToolButton::clicked, this, &MainWindow::selectCommand);
    connect(ui->lineEditName, &QLineEdit::editingFinished, this, &MainWindow::changeName);
    connect(ui->lineEditCommand, &QLineEdit::editingFinished, this, &MainWindow::changeCommand);
    connect(ui->lineEditComment, &QLineEdit::editingFinished, this, &MainWindow::changeComment);
    connect(ui->listWidgetEditCategories, &QListWidget::itemSelectionChanged, this, &MainWindow::enableDelete);
    connect(ui->pushDelete, &QPushButton::clicked, this, &MainWindow::delCategory);
    connect(ui->pushAdd, &QPushButton::clicked, this, &MainWindow::addCategoryMsgBox);
    connect(ui->pushChangeIcon, &QPushButton::clicked, this, &MainWindow::changeIcon);
    connect(ui->checkNotify, &QCheckBox::clicked, this, &MainWindow::changeNotify);
    connect(ui->checkHide, &QCheckBox::clicked, this, &MainWindow::changeHide);
    connect(ui->checkRunInTerminal, &QCheckBox::clicked, this, &MainWindow::changeTerminal);
    connect(ui->advancedEditor, &QTextEdit::undoAvailable, ui->pushSave, &QPushButton::setEnabled);
    connect(ui->pushAddApp, &QPushButton::clicked, this, &MainWindow::addAppMsgBox);
    connect(ui->lineEditName, &QLineEdit::textEdited, this, &MainWindow::setEnabled);
    connect(ui->lineEditCommand, &QLineEdit::textEdited, this, &MainWindow::setEnabled);
    connect(ui->lineEditComment, &QLineEdit::textEdited, this, &MainWindow::setEnabled);

    QSize size = this->size();
    if (settings.contains(QStringLiteral("geometry"))) {
        restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());
        if (this->isMaximized()) { // add option to resize if maximized
            this->resize(size);
            centerWindow();
        }
    }
}

MainWindow::~MainWindow()
{
    delete add;
    delete ui;
}

void MainWindow::loadMenuFiles()
{
    const QString &home_path = QDir::homePath();
    QStringList menu_items;
    const QStringList &menu_files = listMenuFiles();

    // process each menu_file
    for (const QString &file_name : menu_files) {
        QFile file(file_name);
        if (file.open(QIODevice::ReadOnly)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine();
                QString name;
                if (line.contains(QLatin1String("<Name>"))) {
                    if (!in.atEnd()) {
                        line = in.readLine();
                        if (line.contains(QLatin1String("<Directory>"))) {
                            line = line.remove(QStringLiteral("<Directory>"))
                                       .remove(QStringLiteral("</Directory>"))
                                       .trimmed();
                            QString f_name = home_path + "/.local/share/desktop-directories/" + line;
                            if (!QFileInfo::exists(f_name)) // use /usr if the file is not present in ~/.local
                                f_name = "/usr/share/desktop-directories/" + line;
                            name = getCatName(f_name); // get the Name= from .directory file
                            if (!name.isEmpty() && name != QLatin1String("Other") && name != QLatin1String("Wine"))
                                menu_items << name;
                            // Find <Category> and <Filename> and add them in hashCategory and hashInclude
                            while (!(in.atEnd() || line.contains(QLatin1String("</Include>")))) {
                                line = in.readLine();
                                if (line.contains(QLatin1String("<Category>"))) {
                                    line = line.remove(QStringLiteral("<Category>"))
                                               .remove(QStringLiteral("</Category>"))
                                               .trimmed();
                                    if (!hashCategories.values(name).contains(line))
                                        hashCategories.insert(
                                            name, line); // each menu category displays a number of categories
                                }
                                if (line.contains(QLatin1String("<Filename>"))) {
                                    line = line.remove(QStringLiteral("<Filename>"))
                                               .remove(QStringLiteral("</Filename>"))
                                               .trimmed();
                                    if (!hashInclude.values(name).contains(line))
                                        hashInclude.insert(name, line); // each menu category contains a number of files
                                }
                            }
                            // find <Exludes> and add them in hashExclude
                            while (!(in.atEnd() || line.contains(QLatin1String("</Exclude>"))
                                     || line.contains(QLatin1String("<Menu>")))) {
                                line = in.readLine();
                                if (line.contains(QLatin1String("<Exclude>"))) {
                                    while (!(in.atEnd() || line.contains(QLatin1String("</Exclude>")))) {
                                        line = in.readLine();
                                        if (line.contains(QLatin1String("<Filename>"))) {
                                            line = line.remove(QStringLiteral("<Filename>"))
                                                       .remove(QStringLiteral("</Filename>"))
                                                       .trimmed();
                                            if (!hashExclude.values(name).contains(line))
                                                hashExclude.insert(
                                                    name, line); // each menu category contains a number of files
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            file.close();
        }
    }
    displayList(menu_items);
}

void MainWindow::setConnections()
{
    connect(ui->pushAbout, &QPushButton::clicked, this, &MainWindow::pushAbout_clicked);
    connect(ui->pushCancel, &QPushButton::clicked, this, &MainWindow::pushCancel_clicked);
    connect(ui->pushHelp, &QPushButton::clicked, this, &MainWindow::pushHelp_clicked);
    connect(ui->pushRestoreApp, &QPushButton::clicked, this, &MainWindow::pushRestoreApp_clicked);
    connect(ui->pushSave, &QPushButton::clicked, this, &MainWindow::pushSave_clicked);
}

// get Name= from .directory file
QString MainWindow::getCatName(const QString &file_name)
{
    proc.start(QStringLiteral("grep"), QStringList {"Name=", file_name}, QIODevice::ReadOnly);
    proc.waitForFinished();
    if (proc.exitCode() != 0)
        return QString();
    return QString(proc.readAllStandardOutput().trimmed()).remove(QStringLiteral("Name="));
}

// return a list of .menu files
QStringList MainWindow::listMenuFiles()
{
    QString home_path = QDir::homePath();
    QStringList menu_files(QStringLiteral("/etc/xdg/menus/xfce-applications.menu"));
    QDir user_dir;

    // add menu files from user directory
    user_dir.setPath(home_path + "/.config/menus");
    QDirIterator it(user_dir, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString item = it.next();
        if (item.endsWith(QLatin1String(".menu")))
            menu_files << item;
    }
    return menu_files;
}

// Display sorted list of menu items in the treeWidget
void MainWindow::displayList(QStringList menu_items)
{
    QTreeWidgetItem *topLevelItem = nullptr;
    ui->treeWidget->setHeaderLabel(QLatin1String(""));
    ui->treeWidget->setSortingEnabled(true);
    menu_items.removeDuplicates();
    for (const QString &item : menu_items) {
        topLevelItem = new QTreeWidgetItem(ui->treeWidget, QStringList(item));
        // topLevelItem look
        QFont font;
        font.setBold(true);
        topLevelItem->setForeground(0, QBrush(Qt::darkGreen));
        topLevelItem->setFont(0, font);
    }
    ui->treeWidget->sortItems(0, Qt::AscendingOrder);
}

void MainWindow::loadApps(QTreeWidgetItem *item) { ui->treeWidget->setCurrentItem(item); }

// load the applications in the selected category
void MainWindow::loadApps()
{
    // execute if topLevel item is selected
    if (ui->treeWidget->currentItem()->parent() == nullptr) {
        if (ui->pushSave->isEnabled() && !save())
            return;
        QTreeWidgetItem *item = ui->treeWidget->currentItem();
        item->takeChildren();
        resetInterface();

        QStringList categories; // displayed categories in the menu
        QStringList includes;   // included files
        QStringList excludes;   // excluded files
        QStringList includes_usr;
        QStringList includes_local;
        QStringList listApps;

        categories << hashCategories.values(item->text(0));
        includes << hashInclude.values(item->text(0));
        excludes << hashExclude.values(item->text(0));

        includes_usr.reserve(excludes.size());
        includes_local.reserve(excludes.size());
        for (const QString &file : std::as_const(includes)) {
            includes_usr << "/usr/share/applications" + file;
            includes_local << QDir::homePath() + "/.local/share/applications/" + file;
        }

        // determine search string for all categories to be listead under menu category
        QString search_string;
        for (const QString &category : std::as_const(categories)) {
            if (search_string.isEmpty())
                search_string = "Categories=.*" + category;
            else
                search_string += "|Categories=.*" + category;
        }

        // list .desktop files from /usr and .local
        QStringList usr_desktop_files = listDesktopFiles(search_string, QStringLiteral("/usr/share/applications"));
        QStringList local_desktop_files
            = listDesktopFiles(search_string, QDir::homePath() + "/.local/share/applications");

        // add included files
        usr_desktop_files.append(includes_usr);
        local_desktop_files.append(includes_local);

        // exclude files
        for (const QString &base_name : std::as_const(excludes)) {
            usr_desktop_files.removeAll("/usr/share/applications/" + base_name);
            local_desktop_files.removeAll(QDir::homePath() + "/.local/share/applications/" + base_name);
        }

        // list of names without path
        QStringList local_base_names;
        for (const QString &local_name : std::as_const(all_local_desktop_files)) {
            QFileInfo f_local(local_name);
            local_base_names << f_local.fileName();
        }
        QStringList usr_base_names;
        for (const QString &usr_name : std::as_const(all_usr_desktop_files)) {
            QFileInfo f_usr(usr_name);
            usr_base_names << f_usr.fileName();
        }

        // parse local .desktop files
        QTreeWidgetItem *app = nullptr;
        for (const QString &local_name : std::as_const(local_desktop_files)) {
            QFileInfo fi_local(local_name);
            app = addToTree(local_name);
            all_local_desktop_files << local_name;
            if (usr_base_names.contains(fi_local.fileName()))
                if (app != nullptr)
                    app->setData(0, Qt::UserRole, "restore");
        }

        // parse usr .desktop files
        for (const QString &file : std::as_const(usr_desktop_files)) {
            QFileInfo fi(file);
            QString base_name = fi.fileName();
            // add items only for files that are not in the list of local .desktop files
            if (!local_base_names.contains(base_name))
                addToTree(file);
        }
        item->sortChildren(1, Qt::AscendingOrder);
        item->setExpanded(true);
        current_item = ui->treeWidget
                           ->currentItem(); // remember the current_item in case user selects another item before saving
    } else {
        loadItem(ui->treeWidget->currentItem(), 0);
    }
}

// add .desktop item to treeWidget
QTreeWidgetItem *MainWindow::addToTree(const QString &file_name)
{
    if (QFileInfo::exists(file_name)) {
        proc.start(QStringLiteral("grep"), QStringList {"-m1", "^Name=", file_name}, QIODevice::ReadOnly);
        proc.waitForFinished();
        if (proc.exitCode() != 0)
            return nullptr;
        QString app_name = proc.readAllStandardOutput().trimmed();
        app_name = app_name.section(QStringLiteral("="), 1, 1);

        // add item as childItem to treeWidget
        auto *childItem = new QTreeWidgetItem(ui->treeWidget->currentItem());
        if (isHidden(file_name))
            childItem->setForeground(0, QBrush(Qt::gray));
        childItem->setText(0, app_name);
        childItem->setText(1, file_name);
        return childItem;
    }
    return nullptr;
}

QStringList MainWindow::listDesktopFiles(const QString &search_string, const QString &location)
{
    if (search_string.isEmpty())
        proc.start(QStringLiteral("find"), QStringList {location, "-name", "*.desktop"}, QIODevice::ReadOnly);
    else
        proc.start(QStringLiteral("grep"), QStringList {"-Elr", search_string, location}, QIODevice::ReadOnly);
    proc.waitForFinished();
    if (proc.exitCode() != 0)
        return QStringList();
    QString out = proc.readAllStandardOutput().trimmed();
    if (out.isEmpty())
        return QStringList();
    return out.split(QStringLiteral("\n"));
}

void MainWindow::loadItem(QTreeWidgetItem *item, int /*unused*/)
{
#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
#define IS_LABEL_ICON_EMPTY ui->labelIcon->pixmap() == nullptr
#else
#define IS_LABEL_ICON_EMPTY ui->labelIcon->pixmap(Qt::ReturnByValue).isNull()
#endif
    // execute if not topLevel item is selected
    if (item->parent() != nullptr) {
        if (ui->pushSave->isEnabled() && !save())
            return;
        QString file_name = ui->treeWidget->currentItem()->text(1).trimmed();
        resetInterface();
        enableEdit();

        const QSize size = ui->labelIcon->size();

        QFile file(file_name);
        if (!file.open(QIODevice::ReadOnly)) {
            qDebug() << "Can't open file: " << file.fileName();
            return;
        }
        QString line;
        QString content;

        // reset lines for Exec and Icon
        ui->labelCommand->clear();
        ui->labelIcon->clear();
        while (!file.atEnd()) {
            line = file.readLine();
            content.append(line);
            if (line.startsWith(QLatin1String("Categories="))) {
                line = line.section(QStringLiteral("="), 1);
                line.remove(QRegularExpression(QStringLiteral(";$")));
                QStringList categories = line.trimmed().split(QStringLiteral(";"));
                ui->listWidgetEditCategories->addItems(categories);
            } else if (line.startsWith(QLatin1String("Name="))) {
                line = line.section(QStringLiteral("="), 1).trimmed();
                ui->lineEditName->setText(line);
            } else if (line.startsWith(QLatin1String("Comment="))) {
                line = line.section(QStringLiteral("="), 1).trimmed();
                ui->lineEditComment->setText(line);
            } else if (line.startsWith(QLatin1String("Exec="))) {
                line = line.section(QStringLiteral("="), 1).trimmed();
                if (ui->lineEditCommand->text().isEmpty()) // some .desktop files have multiple Exec= display first one
                    ui->lineEditCommand->setText(line);
                ui->lineEditCommand->home(false);
            } else if (line.startsWith(QLatin1String("StartupNotify="))) {
                const QString value = line.section(QStringLiteral("="), 1).trimmed();
                ui->checkNotify->setChecked(value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0);
            } else if (line.startsWith(QLatin1String("NoDisplay="))) {
                const QString value = line.section(QStringLiteral("="), 1).trimmed();
                ui->checkHide->setChecked(value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0);
            } else if (line.startsWith(QLatin1String("Terminal="))) {
                const QString value = line.section(QStringLiteral("="), 1).trimmed();
                ui->checkRunInTerminal->setChecked(value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0);
            } else if (line.startsWith(QLatin1String("Icon="))) {
                line = line.section(QStringLiteral("="), 1).trimmed();
                if (!line.isEmpty() && IS_LABEL_ICON_EMPTY) // some .desktop files have multiple Icon= display first
                    ui->labelIcon->setPixmap(findIcon(line, size).scaled(size));
            }
        }
        file.close();
        ui->advancedEditor->setText(content); // adding content line by line it triggers "undo available"
        // enable RestoreApp button if flag is set up for item
        if (ui->treeWidget->currentItem()->data(0, Qt::UserRole) == "restore")
            ui->pushRestoreApp->setEnabled(true);
        else
            ui->pushRestoreApp->setEnabled(false);
        current_item = ui->treeWidget
                           ->currentItem(); // remember the current_item in case user selects another item before saving
    }
}

bool MainWindow::isHidden(const QString &file_name)
{
    return QProcess::execute(QStringLiteral("grep"), {"-q", "NoDisplay=true", file_name}) == 0;
}

// select command to be used
void MainWindow::selectCommand()
{
    QString selected = QFileDialog::getOpenFileName(this, tr("Select executable file"), QStringLiteral("/usr/bin"));
    if (!selected.isEmpty()) {
        if (ui->lineEditCommand->isEnabled()) {
            ui->lineEditCommand->setText(selected);
            ui->pushSave->setEnabled(true);
        } else { // if running command from add-custom-app window
            add->ui->lineEditCommand->setText(selected);
        }
        changeCommand();
    }
}

void MainWindow::resetInterface()
{
    ui->listWidgetEditCategories->clear();
    ui->advancedEditor->clear();
    ui->advancedEditor->setDisabled(true);
    ui->advancedEditor->setLineWrapMode(QTextEdit::NoWrap);
    ui->lineEditName->clear();
    ui->lineEditComment->clear();
    ui->lineEditCommand->clear();
    ui->checkHide->setChecked(false);
    ui->checkNotify->setChecked(false);
    ui->checkRunInTerminal->setChecked(false);
    ui->checkHide->setDisabled(true);
    ui->checkNotify->setDisabled(true);
    ui->checkRunInTerminal->setDisabled(true);
    ui->toolButtonCommand->setDisabled(true);
    ui->pushAdd->setDisabled(true);
    ui->pushDelete->setDisabled(true);
    ui->lineEditCommand->setDisabled(true);
    ui->lineEditComment->setDisabled(true);
    ui->lineEditName->setDisabled(true);
    ui->pushChangeIcon->setDisabled(true);
    ui->pushRestoreApp->setDisabled(true);
    ui->pushSave->setDisabled(true);
    ui->labelIcon->setPixmap(QPixmap());
}

void MainWindow::saveSettings() { settings.setValue(QStringLiteral("geometry"), saveGeometry()); }

void MainWindow::enableEdit()
{
    ui->checkNotify->setEnabled(true);
    ui->checkHide->setEnabled(true);
    ui->checkRunInTerminal->setEnabled(true);
    ui->toolButtonCommand->setEnabled(true);
    ui->pushAdd->setEnabled(true);
    ui->pushChangeIcon->setEnabled(true);
    ui->advancedEditor->setEnabled(true);
    ui->lineEditCommand->setEnabled(true);
    ui->lineEditComment->setEnabled(true);
    ui->lineEditName->setEnabled(true);
}

void MainWindow::changeIcon()
{
    QFileDialog dialog;
    QString selected;
    dialog.setFilter(QDir::Hidden);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilter(tr("Image Files (*.png *.jpg *.bmp *.xpm *.svg)"));
    dialog.setDirectory(QStringLiteral("/usr/share/icons"));
    if (dialog.exec() == QDialog::Accepted) {
        QStringList selected_list = dialog.selectedFiles();
        selected = selected_list.at(0);
    }
    if (!selected.isEmpty()) {
        QString text = ui->advancedEditor->toPlainText();
        if (ui->lineEditCommand->isEnabled()) { // started from editor
            ui->pushSave->setEnabled(true);
            if (text.contains(QRegularExpression(QStringLiteral("(^|\n)Icon="))))
                text.replace(QRegularExpression(QStringLiteral("(^|\n)Icon=[^\n]*(\n|$)")),
                             "\nIcon=" + selected + "\n");
            else
                text.append("\nIcon=" + selected + "\n");
            ui->advancedEditor->setText(text);
            ui->labelIcon->setPixmap(QPixmap(selected));
        } else { // if running command from add-custom-app window
            add->icon_path = selected;
            add->ui->pushChangeIcon->setIcon(QIcon(selected));
            add->ui->pushChangeIcon->setText(tr("Change icon"));
        }
    }
}

void MainWindow::changeName()
{
    if (ui->lineEditCommand->isEnabled()) { // started from editor
        ui->pushSave->setEnabled(true);
        const QString &new_name = ui->lineEditName->text();
        if (!new_name.isEmpty()) {
            QString text = ui->advancedEditor->toPlainText();
            QRegularExpression regex(QStringLiteral("(^|\n)Name=[^\n]*(\n|$)"));
            QRegularExpressionMatch regex_match = regex.match(text);
            int index = regex_match.capturedStart();
            int length = regex_match.capturedLength();

            if (index != -1) {
                text.replace(index, length, "\nName=" + new_name + "\n"); // replace only first match
                ui->advancedEditor->setText(text);
            }
        }
    } else { // if running command from add-custom-app window
        if (!add->ui->lineEditName->text().isEmpty() && !add->ui->lineEditCommand->text().isEmpty()
            && add->ui->listWidgetCategories->count() != 0)
            add->ui->pushSave->setEnabled(true);
        else
            add->ui->pushSave->setEnabled(false);
    }
}

void MainWindow::changeCommand()
{
    if (ui->lineEditCommand->isEnabled()) { // started from editor
        ui->pushSave->setEnabled(true);
        const QString &new_command = ui->lineEditCommand->text();
        if (!new_command.isEmpty()) {
            QString text = ui->advancedEditor->toPlainText();
            text.replace(QRegularExpression(QStringLiteral("(^|\n)Exec=[^\n]*(\n|$)")), "\nExec=" + new_command + "\n");
            ui->advancedEditor->setText(text);
        }
    } else { // if running command from add-custom-app window
        const QString &new_command = add->ui->lineEditCommand->text();
        if (!new_command.isEmpty() && !add->ui->lineEditName->text().isEmpty()
            && add->ui->listWidgetCategories->count() != 0)
            add->ui->pushSave->setEnabled(true);
        else
            add->ui->pushSave->setEnabled(false);
    }
}

void MainWindow::changeComment()
{
    if (ui->lineEditCommand->isEnabled()) { // started from editor
        ui->pushSave->setEnabled(true);
        const QString &new_comment = ui->lineEditComment->text();
        QString text = ui->advancedEditor->toPlainText();
        if (!new_comment.isEmpty()) {
            if (text.contains(QLatin1String("Comment="))) {
                text.replace(QRegularExpression(QStringLiteral("(^|\n)Comment=[^\n]*(\n|$)")),
                             "\nComment=" + new_comment + "\n");
            } else {
                text = text.trimmed();
                text.append("\nComment=" + new_comment + "\n");
            }
        } else {
            text.remove(QRegularExpression(QStringLiteral("(^|\n)Comment=[^\n]*(\n|$)")));
        }
        ui->advancedEditor->setText(text);
    }
}

void MainWindow::enableDelete() { ui->pushDelete->setEnabled(true); }

void MainWindow::delCategory()
{
    int row = 0;
    if (ui->lineEditCommand->isEnabled()) { // started from editor
        ui->pushSave->setEnabled(true);
        row = ui->listWidgetEditCategories->currentRow();
        QListWidgetItem *item = ui->listWidgetEditCategories->takeItem(row);
        QString text = ui->advancedEditor->toPlainText();
        int indexCategory = text.indexOf(QRegularExpression(QStringLiteral("(^|\n)Categories=[^\n]*(\n|$)")));
        int indexToDelete = text.indexOf(item->text() + ";", indexCategory);
        text.remove(indexToDelete, item->text().length() + 1);
        ui->advancedEditor->setText(text);
        if (ui->listWidgetEditCategories->count() == 0) {
            ui->pushDelete->setDisabled(true);
            ui->pushSave->setDisabled(true);
        }
    } else { // if running command from add-custom-app window
        row = add->ui->listWidgetCategories->currentRow();
        add->ui->listWidgetCategories->takeItem(row);
        if (add->ui->listWidgetCategories->count() == 0) {
            add->ui->pushDelete->setDisabled(true);
            add->ui->pushSave->setDisabled(true);
        }
    }
}

void MainWindow::changeNotify(bool checked)
{
    ui->pushSave->setEnabled(true);
    const QString &str = QString(checked ? QStringLiteral("true") : QStringLiteral("false"));
    QString text = ui->advancedEditor->toPlainText();
    if (text.contains(QLatin1String("StartupNotify="))) {
        text.replace(QRegularExpression(QStringLiteral("(^|\n)StartupNotify=[^\n]*(\n|$)")),
                     "\nStartupNotify=" + str + "\n");
    } else {
        text = text.trimmed();
        text.append("\nStartupNotify=" + str);
    }
    ui->advancedEditor->setText(text);
}

// hide or show the item in the menu
void MainWindow::changeHide(bool checked)
{
    ui->pushSave->setEnabled(true);
    const QString &str = QString(checked ? QStringLiteral("true") : QStringLiteral("false"));
    QString text = ui->advancedEditor->toPlainText().trimmed();
    if (text.contains(QLatin1String("NoDisplay="))) {
        text.replace(QRegularExpression(QStringLiteral("(^|\n)NoDisplay=[^\n]*(\n|$)")), "\nNoDisplay=" + str + "\n");
    } else {
        QString new_text;
        for (const QString &line : text.split(QStringLiteral("\n"))) {
            new_text.append(line + "\n");
            if (line.startsWith(QLatin1String("Exec=")))
                new_text.append("NoDisplay=" + str + "\n");
        }
        text = new_text;
    }
    ui->advancedEditor->setText(text);
}

// change "run in terminal" setting
void MainWindow::changeTerminal(bool checked)
{
    ui->pushSave->setEnabled(true);
    const QString &str = QString(checked ? QStringLiteral("true") : QStringLiteral("false"));
    QString text = ui->advancedEditor->toPlainText();
    if (text.contains(QLatin1String("Terminal="))) {
        text.replace(QRegularExpression(QStringLiteral("(^|\n)Terminal=[^\n]*(\n|$)")), "\nTerminal=" + str + "\n");
    } else {
        text = text.trimmed();
        text.append("\nTerminal=" + str);
    }
    ui->advancedEditor->setText(text);
}

// list categories of the displayed items
QStringList MainWindow::listCategories() const
{
    QStringList categories;
    for (auto it = hashCategories.cbegin(); it != hashCategories.cend(); ++it) {
        categories << it.value();
    }
    categories.removeDuplicates();
    categories = categories.filter(QRegularExpression(QStringLiteral("^(?!<Not>).*$")));
    categories.sort();
    return categories;
}

// display add category message box
void MainWindow::addCategoryMsgBox()
{
    const QStringList &categories = listCategories();

    auto *window = new QWidget(add, Qt::Dialog);
    window->setWindowTitle(tr("Choose category"));
    const int height = 80;
    const int width = 250;
    window->resize(width, height);

    auto *buttonBox = new QDialogButtonBox();

    comboBox->clear();
    // comboBox->setEditable(true);
    comboBox->addItems(categories);
    comboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // because we want to display the buttons in reverse order we use counter-intuitive roles.
    buttonBox->addButton(tr("Cancel"), QDialogButtonBox::AcceptRole);
    buttonBox->addButton(tr("OK"), QDialogButtonBox::RejectRole);
    connect(buttonBox, &QDialogButtonBox::accepted, window, &QWidget::close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &MainWindow::addCategory);
    connect(buttonBox, &QDialogButtonBox::rejected, window, &QWidget::close);

    auto *layout = new QFormLayout;
    layout->addRow(comboBox);
    layout->addRow(buttonBox);

    window->setLayout(layout);
    window->show();
}

void MainWindow::centerWindow()
{
    QRect screenGeometry = QApplication::screens().constFirst()->geometry();
    int x = (screenGeometry.width() - this->width()) / 2;
    int y = (screenGeometry.height() - this->height()) / 2;
    this->move(x, y);
}

// add selected categorory to the .desktop file
void MainWindow::addCategory()
{
    const QString &str = comboBox->currentText();
    QString text = ui->advancedEditor->toPlainText();
    int index = text.indexOf(QRegularExpression(QStringLiteral("(^|\n)Categories=[^\n]*(\n|$)")));
    index = text.indexOf(QRegularExpression(QStringLiteral("(\n|$)")), index + 1); // find the end of the string
    if (ui->lineEditCommand->isEnabled()) {                                        // started from editor
        if (ui->listWidgetEditCategories->findItems(str, Qt::MatchFixedString).isEmpty()) {
            ui->pushSave->setEnabled(true);
            text.insert(index, str + ";");
            ui->listWidgetEditCategories->addItem(str);
            ui->advancedEditor->setText(text);
            ui->pushDelete->setEnabled(true);
            if (ui->listWidgetEditCategories->count() == 0)
                ui->pushSave->setDisabled(true);
        }
    } else { // if running command from add-custom-app window
        if (add->ui->listWidgetCategories->findItems(str, Qt::MatchFixedString).isEmpty()) {
            text.insert(index, str + ";");
            add->ui->listWidgetCategories->addItem(str);
            add->ui->pushDelete->setEnabled(true);
            if (!add->ui->lineEditName->text().isEmpty() && !add->ui->lineEditCommand->text().isEmpty()
                && add->ui->listWidgetCategories->count() != 0)
                add->ui->pushSave->setEnabled(true);
            else
                add->ui->pushSave->setEnabled(false);
        }
    }
}

// display add application message box
void MainWindow::addAppMsgBox()
{
    QStringList categories;
    QTreeWidgetItemIterator it(ui->treeWidget);
    // list top level items
    while (((*it) != nullptr) && ((*it)->parent() == nullptr)) {
        categories << (*it)->text(0);
        ++it;
    }
    if (ui->pushSave->isEnabled() && !save())
        return;
    add->show();
    resetInterface();
    ui->treeWidget->collapseAll();
    add->ui->selectCommand->disconnect();
    add->ui->pushChangeIcon->disconnect();
    add->ui->lineEditName->disconnect();
    add->ui->lineEditCommand->disconnect();
    add->ui->lineEditComment->disconnect();
    add->ui->pushAdd->disconnect();
    add->ui->pushDelete->disconnect();
    connect(add->ui->selectCommand, &QToolButton::clicked, this, &MainWindow::selectCommand);
    connect(add->ui->pushChangeIcon, &QPushButton::clicked, this, &MainWindow::changeIcon);
    connect(add->ui->lineEditName, &QLineEdit::editingFinished, this, &MainWindow::changeName);
    connect(add->ui->lineEditCommand, &QLineEdit::editingFinished, this, &MainWindow::changeCommand);
    connect(add->ui->lineEditComment, &QLineEdit::editingFinished, this, &MainWindow::changeComment);
    connect(add->ui->pushAdd, &QPushButton::clicked, this, &MainWindow::addCategoryMsgBox);
    connect(add->ui->pushDelete, &QPushButton::clicked, this, &MainWindow::delCategory);
}

void MainWindow::pushSave_clicked()
{
    QString file_name = current_item->text(1);
    QFileInfo fi(file_name);
    QString base_name = fi.fileName();
    if (!QFileInfo::exists(QDir::homePath() + "/.local/share/applications/"))
        QDir().mkpath(QDir::homePath() + "/.local/share/applications/");
    QString out_name = QDir::homePath() + "/.local/share/applications/" + base_name;
    QFile out(out_name);
    if (!out.open(QFile::WriteOnly | QFile::Text))
        QMessageBox::critical(this, tr("Error"), tr("Could not save the file"));
    all_local_desktop_files << out_name;
    out.write(ui->advancedEditor->toPlainText().toUtf8());
    out.flush();
    out.close();
    if (QProcess::execute(QStringLiteral("pgrep"), {QStringLiteral("xfce4-panel")}) == 0)
        QProcess::execute(QStringLiteral("xfce4-panel"), {QStringLiteral("--restart")});
    ui->pushSave->setDisabled(true);
    findReloadItem(base_name);
}

void MainWindow::pushAbout_clicked()
{
    this->hide();
    QMessageBox msgBox(
        QMessageBox::NoIcon, tr("About MX Menu Editor"),
        "<p align=\"center\"><b><h2>" + tr("MX Menu Editor") + "</h2></b></p><p align=\"center\">" + tr("Version: ")
            + VERSION + "</p><p align=\"center\"><h3>" + tr("Program for editing Xfce menu")
            + R"(</h3></p><p align="center"><a href="http://mxlinux.org">http://mxlinux.org</a><br /></p><p align="center">)"
            + tr("Copyright (c) MX Linux") + "<br /><br /></p>");
    auto *btnLicense = msgBox.addButton(tr("License"), QMessageBox::HelpRole);
    auto *btnChangelog = msgBox.addButton(tr("Changelog"), QMessageBox::HelpRole);
    auto *btnCancel = msgBox.addButton(tr("Cancel"), QMessageBox::NoRole);
    btnCancel->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));

    msgBox.exec();

    if (msgBox.clickedButton() == btnLicense) {
        QProcess::execute(QStringLiteral("xdg-open"), {"file:///usr/share/doc/mx-menu-editor/license.html"});
    } else if (msgBox.clickedButton() == btnChangelog) {
        auto *changelog = new QDialog(this);
        const int width = 500;
        const int height = 600;
        changelog->resize(width, height);

        auto *text = new QTextEdit;
        text->setReadOnly(true);
        proc.start(QStringLiteral("zless"),
                   QStringList {"/usr/share/doc/" + QFileInfo(QCoreApplication::applicationFilePath()).fileName()
                                + "/changelog.gz"});
        proc.waitForFinished();
        if (proc.exitCode() != 0)
            return;
        text->setText(proc.readAllStandardOutput());

        auto *btnClose = new QPushButton(tr("&Close"));
        btnClose->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));
        connect(btnClose, &QPushButton::clicked, changelog, &QDialog::close);

        auto *layout = new QVBoxLayout;
        layout->addWidget(text);
        layout->addWidget(btnClose);
        changelog->setLayout(layout);
        changelog->exec();
    }
    this->show();
}

void MainWindow::setEnabled(const QString & /*unused*/) { ui->pushSave->setEnabled(true); }

void MainWindow::pushHelp_clicked()
{
    QLocale locale;
    const QString lang = locale.bcp47Name();

    QString url = QStringLiteral("/usr/share/doc/mx-menu-editor/mx-menu-editor.html");

    if (lang.startsWith(QLatin1String("fr")))
        url = QStringLiteral("https://mxlinux.org/wiki/help-files/help-mx-editeur-de-menu");

    QProcess::execute(QStringLiteral("xdg-open"), {url});
}

// Cancel button clicked
void MainWindow::pushCancel_clicked()
{
    if (ui->pushSave->isEnabled() && !save())
        return;
    QApplication::quit();
}

// ask whether to save edits or not
bool MainWindow::save()
{
    if (!ui->pushSave->isEnabled())
        return true;

    const auto answer = QMessageBox::question(this, tr("Save changes?"), tr("Do you want to save your edits?"),
                                              QMessageBox::Save | QMessageBox::Discard);
    if (answer == QMessageBox::Save)
        pushSave_clicked();
    else
        ui->pushSave->setDisabled(true);

    return true; // always continue; discard is treated as proceed without saving
}

// delete .local file and reload files
void MainWindow::pushRestoreApp_clicked()
{
    QString file_name = current_item->text(1);
    QFileInfo fi(file_name);
    QString base_name = fi.fileName();
    QFile file(file_name);
    file.remove();
    all_local_desktop_files = listDesktopFiles(QLatin1String(""), QDir::homePath() + "/.local/share/applications");
    ui->pushRestoreApp->setDisabled(true);
    findReloadItem(base_name);
}

// find and reload item
void MainWindow::findReloadItem(const QString &base_name)
{
    ui->treeWidget->setCurrentItem(current_item);           // change current item back to original selection
    ui->treeWidget->setCurrentItem(current_item->parent()); // change current item to reload category
    QTreeWidgetItemIterator it(current_item->treeWidget());
    while ((*it) != nullptr) {
        QFileInfo fi((*it)->text(1));
        if ((fi.fileName() == base_name)) {
            ui->treeWidget->setCurrentItem(*it);
            return;
        }
        ++it;
    }
}

QPixmap MainWindow::findIcon(QString icon_name, QSize size)
{
    static QHash<QString, QIcon> iconCache;
    static const QRegularExpression re(QStringLiteral("\\.(png|svg|xpm)$"));
    static const QStringList extensions {QStringLiteral(".png"), QStringLiteral(".svg"), QStringLiteral(".xpm")};
    static const QStringList searchPaths {QDir::homePath() + QStringLiteral("/.local/share/icons/"),
                                          QStringLiteral("/usr/share/pixmaps/"),
                                          QStringLiteral("/usr/local/share/icons/"),
                                          QStringLiteral("/usr/share/icons/"),
                                          QStringLiteral("/usr/share/icons/hicolor/scalable/apps/"),
                                          QStringLiteral("/usr/share/icons/hicolor/48x48/apps/"),
                                          QStringLiteral("/usr/share/icons/Adwaita/48x48/legacy/")};

    const auto makePixmap = [size](const QIcon &icon) {
        return icon.isNull() ? QPixmap() : icon.pixmap(size.isValid() ? size : QSize());
    };

    if (icon_name.isEmpty())
        return QPixmap();

    if (iconCache.contains(icon_name))
        return makePixmap(iconCache.value(icon_name));

    // Absolute path handling
    if (QFileInfo::exists(icon_name) && QFileInfo(icon_name).isAbsolute()) {
        QIcon icon(icon_name);
        iconCache.insert(icon_name, icon);
        return makePixmap(icon);
    }

    QString nameNoExt = icon_name;
    nameNoExt.remove(re);

    // Themed icon
    QIcon themedIcon = QIcon::fromTheme(nameNoExt);
    if (!themedIcon.isNull()) {
        iconCache.insert(icon_name, themedIcon);
        return makePixmap(themedIcon);
    }

    const auto searchInPaths = [](const QString &name) -> QIcon {
        for (const auto &path : searchPaths) {
            const QString fullPath = QDir(path).filePath(name);
            if (QFile::exists(fullPath)) {
                QIcon icon(fullPath);
                if (!icon.isNull())
                    return icon;
            }
        }
        return QIcon();
    };

    // Try exact name first
    QIcon icon = searchInPaths(icon_name);
    if (!icon.isNull()) {
        iconCache.insert(icon_name, icon);
        return makePixmap(icon);
    }

    // Try without extension and standard extensions
    for (const auto &ext : extensions) {
        icon = searchInPaths(nameNoExt + ext);
        if (!icon.isNull()) {
            iconCache.insert(icon_name, icon);
            return makePixmap(icon);
        }
    }

    return QPixmap();
}
