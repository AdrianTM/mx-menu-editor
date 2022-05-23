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

#include <QDebug>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QFileDialog>
#include <QFormLayout>
#include <QScreen>
#include <QTextCodec>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ui_addappdialog.h"
#include "version.h"

MainWindow::MainWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MainWindow),
    add(new AddAppDialog)
{
    qDebug().noquote() << QCoreApplication::applicationName() << "version:" << VERSION;
    connect(qApp, &QApplication::aboutToQuit, this, &MainWindow::saveSettings);
    ui->setupUi(this);
    setWindowFlags(Qt::Window); // for the close, min and max buttons

    if (ui->buttonSave->icon().isNull()) ui->buttonSave->setIcon(QIcon(":/icons/dialog-ok.svg"));
    if (add->ui->buttonSave->icon().isNull()) add->ui->buttonSave->setIcon(QIcon(":/icons/dialog-ok.svg"));

    comboBox = new QComboBox;

    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    all_local_desktop_files = listDesktopFiles("", QDir::homePath() + "/.local/share/applications");
    all_usr_desktop_files = listDesktopFiles("", "/usr/share/applications");

    resetInterface();
    loadMenuFiles();

    connect(ui->treeWidget, &QTreeWidget::itemSelectionChanged, this, static_cast<void (MainWindow::*)()>(&MainWindow::loadApps));
    connect(ui->treeWidget, &QTreeWidget::itemExpanded, this, static_cast<void (MainWindow::*)(QTreeWidgetItem*)>(&MainWindow::loadApps));
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
    connect(ui->advancedEditor, &QTextEdit::undoAvailable, ui->buttonSave, &QPushButton::setEnabled);
    connect(ui->pushAddApp, &QPushButton::clicked, this, &MainWindow::addAppMsgBox);
    connect(ui->lineEditName, &QLineEdit::textEdited, this, &MainWindow::setEnabled);
    connect(ui->lineEditCommand, &QLineEdit::textEdited, this, &MainWindow::setEnabled);
    connect(ui->lineEditComment, &QLineEdit::textEdited, this, &MainWindow::setEnabled);

    QSize size = this->size();
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
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

// load menu files
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
                if (line.contains("<Name>") ) {
                    if (!in.atEnd()) {
                        line = in.readLine();
                        if (line.contains("<Directory>")) {
                            line = line.remove("<Directory>").remove("</Directory>").trimmed();
                            QString f_name = home_path + "/.local/share/desktop-directories/" + line;
                            if (!QFileInfo::exists(f_name)) // use /usr if the file is not present in ~/.local
                                f_name = "/usr/share/desktop-directories/" + line;
                            name = getCatName(f_name); // get the Name= from .directory file
                            if (!name.isEmpty() && name != "Other" && name != "Wine")
                                menu_items << name;
                            // Find <Category> and <Filename> and add them in hashCategory and hashInclude
                            while (!(in.atEnd() || line.contains("</Include>"))) {
                                line = in.readLine();
                                if (line.contains("<Category>")) {
                                    line = line.remove("<Category>").remove("</Category>").trimmed();
                                    if (!hashCategories.values(name).contains(line))
                                        hashCategories.insert(name, line); //each menu category displays a number of categories
                                }
                                if (line.contains("<Filename>")) {
                                    line = line.remove("<Filename>").remove("</Filename>").trimmed();
                                    if (!hashInclude.values(name).contains(line))
                                        hashInclude.insert(name, line); //each menu category contains a number of files
                                }
                            }
                            // find <Exludes> and add them in hashExclude
                            while (!(in.atEnd() || line.contains("</Exclude>") || line.contains("<Menu>"))) {
                                line = in.readLine();
                                if (line.contains("<Exclude>")) {
                                    while (!(in.atEnd() || line.contains("</Exclude>"))) {
                                        line = in.readLine();
                                        if (line.contains("<Filename>")) {
                                            line = line.remove("<Filename>").remove("</Filename>").trimmed();
                                            if (!hashExclude.values(name).contains(line))
                                                hashExclude.insert(name, line); //each menu category contains a number of files
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

// get Name= from .directory file
QString MainWindow::getCatName(const QString &file_name)
{
    proc.start("grep", QStringList{"Name=", file_name}, QIODevice::ReadOnly);
    proc.waitForFinished();
    if (proc.exitCode() != 0)
        return QString();
    return QString(proc.readAllStandardOutput().trimmed()).remove("Name=");
}

// return a list of .menu files
QStringList MainWindow::listMenuFiles() {
    QString home_path = QDir::homePath();
    QStringList menu_files("/etc/xdg/menus/xfce-applications.menu");
    QDir user_dir;

    // add menu files from user directory
    user_dir.setPath(home_path + "/.config/menus");
    QDirIterator it(user_dir, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString item = it.next();
        if (item.endsWith(".menu"))
            menu_files << item;
    }
    return menu_files;
}

// Display sorted list of menu items in the treeWidget
void MainWindow::displayList(QStringList menu_items) {
    QTreeWidgetItem *topLevelItem = nullptr;
    ui->treeWidget->setHeaderLabel("");
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


void MainWindow::loadApps(QTreeWidgetItem *item)
{
    ui->treeWidget->setCurrentItem(item);
}

// load the applications in the selected category
void MainWindow::loadApps()
{
    // execute if topLevel item is selected
    if (!ui->treeWidget->currentItem()->parent()) {
        if (ui->buttonSave->isEnabled() && save())
            return;
        QTreeWidgetItem *item = ui->treeWidget->currentItem();
        item->takeChildren();
        resetInterface();

        QStringList categories; // displayed categories in the menu
        QStringList includes; // included files
        QStringList excludes; // excluded files
        QStringList includes_usr;
        QStringList includes_local;
        QStringList listApps;

        categories << hashCategories.values(item->text(0));
        includes << hashInclude.values(item->text(0));
        excludes << hashExclude.values(item->text(0));

        for (const QString &file : qAsConst(includes)) {
            includes_usr << "/usr/share/applications" + file;
            includes_local << QDir::homePath() + "/.local/share/applications/" + file;
        }

        // determine search string for all categories to be listead under menu category
        QString search_string;
        for (const QString &category : qAsConst(categories)) {
            if (search_string.isEmpty())
                search_string = "Categories=.*" + category;
            else
                search_string += "|Categories=.*" + category;
        }

        // list .desktop files from /usr and .local
        QStringList usr_desktop_files = listDesktopFiles(search_string, "/usr/share/applications");
        QStringList local_desktop_files = listDesktopFiles(search_string, QDir::homePath() + "/.local/share/applications");

        // add included files
        usr_desktop_files.append(includes_usr);
        local_desktop_files.append(includes_local);

        // exclude files
        for (const QString &base_name : qAsConst(excludes)) {
            usr_desktop_files.removeAll("/usr/share/applications/" + base_name);
            local_desktop_files.removeAll(QDir::homePath() + "/.local/share/applications/" + base_name);
        }

        // list of names without path
        QStringList local_base_names;
        for (const QString &local_name : qAsConst(all_local_desktop_files)) {
            QFileInfo f_local(local_name);
            local_base_names << f_local.fileName();
        }
        QStringList usr_base_names;
        for (const QString &usr_name : qAsConst(all_usr_desktop_files)) {
            QFileInfo f_usr(usr_name);
            usr_base_names << f_usr.fileName();
        }

        // parse local .desktop files
        QTreeWidgetItem *app;
        for (const QString &local_name : qAsConst(local_desktop_files)) {
            QFileInfo fi_local(local_name);
            app = addToTree(local_name);
            all_local_desktop_files << local_name;
            if (usr_base_names.contains(fi_local.fileName()))
                if (app) app->setData(0, Qt::UserRole, "restore");
        }

        // parse usr .desktop files
        for (const QString &file : qAsConst(usr_desktop_files)) {
            QFileInfo fi(file);
            QString base_name = fi.fileName();
            // add items only for files that are not in the list of local .desktop files
            if (!local_base_names.contains(base_name))
                addToTree(file);
        }
        item->sortChildren(true, Qt::AscendingOrder);
        item->setExpanded(true);
        current_item = ui->treeWidget->currentItem(); // remember the current_item in case user selects another item before saving
    } else {
        loadItem(ui->treeWidget->currentItem(), 0);
    }
}

// add .desktop item to treeWidget
QTreeWidgetItem* MainWindow::addToTree(QString file_name)
{
    if (QFileInfo::exists(file_name)) {
        proc.start("grep", QStringList{"-m1", "^Name=", file_name}, QIODevice::ReadOnly);
        proc.waitForFinished();
        if (proc.exitCode() != 0)
            return nullptr;
        QString app_name = proc.readAllStandardOutput().trimmed();
        app_name = app_name.section("=", 1, 1);

        // add item as childItem to treeWidget
        QTreeWidgetItem *childItem = new QTreeWidgetItem(ui->treeWidget->currentItem());
        if (isHidden(file_name))
            childItem->setForeground(0, QBrush(Qt::gray));
        childItem->setText(0, app_name);
        childItem->setText(1, file_name);
        return childItem;
    }
    return nullptr;
}

// list .desktop files
QStringList MainWindow::listDesktopFiles(const QString &search_string, const QString &location)
{
    if (search_string.isEmpty())
        proc.start("find", QStringList{location, "-name", "*.desktop"}, QIODevice::ReadOnly);
    else
        proc.start("grep", QStringList{"-Elr", search_string, location}, QIODevice::ReadOnly);
    proc.waitForFinished();
    if (proc.exitCode() != 0)
        return QStringList();
    QString out = proc.readAllStandardOutput().trimmed();
    if (out.isEmpty())
        return QStringList();
    return out.split("\n");
}

// load selected item to be edited
void MainWindow::loadItem(QTreeWidgetItem *item, int)
{
    // execute if not topLevel item is selected
    if (item->parent()) {
        if (ui->buttonSave->isEnabled() && save())
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
            if (line.startsWith("Categories=")) {
                line = line.section("=", 1);
                line.remove(QRegularExpression(";$"));
                QStringList categories = line.trimmed().split(";");
                ui->listWidgetEditCategories->addItems(categories);
            } else if (line.startsWith("Name=")) {
                line = line.section("=", 1).trimmed();
                ui->lineEditName->setText(line);
            } else if (line.startsWith("Comment=")) {
                line = line.section("=", 1).trimmed();
                ui->lineEditComment->setText(line);
            } else if (line.startsWith("Exec=")) {
                line = line.section("=", 1).trimmed();
                if (ui->lineEditCommand->text().isEmpty()) // some .desktop files have multiple Exec= display first one
                    ui->lineEditCommand->setText(line);
                ui->lineEditCommand->home(false);
            } else if (line.startsWith("StartupNotify=")) {
                line = line.section("=", 1).trimmed();
                ui->checkNotify->setChecked(true);
            } else if (line.startsWith("NoDisplay=")) {
                line = line.section("=", 1).trimmed();
                ui->checkHide->setChecked(true);
            } else if (line.startsWith("Terminal=")) {
                line = line.section("=", 1).trimmed();
                ui->checkRunInTerminal->setChecked(true);
            } else if (line.startsWith("Icon=")) {
                line = line.section("=", 1).trimmed();
                if (!line.isEmpty() && ui->labelIcon->pixmap() == nullptr) // some .desktop files have multiple Icon= display first
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
        current_item = ui->treeWidget->currentItem(); // remember the current_item in case user selects another item before saving
    }
}

// check if the item is hidden
bool MainWindow::isHidden(const QString &file_name)
{
    QString cmd = "grep -q NoDisplay=true \"" + file_name + "\"";
    return !system(cmd.toUtf8());
}

// select command to be used
void MainWindow::selectCommand()
{
    QFileDialog dialog;
    QString selected = dialog.getOpenFileName(this, tr("Select executable file"), "/usr/bin");
    if (!selected.isEmpty()) {
        if (ui->lineEditCommand->isEnabled()) {
            ui->lineEditCommand->setText(selected);
            ui->buttonSave->setEnabled(true);
        } else { // if running command from add-custom-app window
            add->ui->lineEditCommand->setText(selected);
        }
        changeCommand();
    }
}

// clear selection and reset GUI components
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
    ui->buttonSave->setDisabled(true);
    ui->labelIcon->setPixmap(QPixmap());
}

void MainWindow::saveSettings()
{
    settings.setValue("geometry", saveGeometry());
}

// enable buttons to edit item
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

// change the icon of the application
void MainWindow::changeIcon()
{
    QFileDialog dialog;
    QString selected;
    dialog.setFilter(QDir::Hidden);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilter(tr("Image Files (*.png *.jpg *.bmp *.xpm *.svg)"));
    dialog.setDirectory("/usr/share/icons");
    if (dialog.exec()) {
        QStringList selected_list = dialog.selectedFiles();
        selected = selected_list.at(0);
    }
    if (!selected.isEmpty()) {
        QString text = ui->advancedEditor->toPlainText();
        if (ui->lineEditCommand->isEnabled()) { // started from editor
            ui->buttonSave->setEnabled(true);
            if (text.contains(QRegularExpression("(^|\n)Icon=")))
                text.replace(QRegularExpression("(^|\n)Icon=[^\n]*(\n|$)"), "\nIcon=" + selected + "\n");
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

// change the name of the entry
void MainWindow::changeName()
{
    if (ui->lineEditCommand->isEnabled()) { // started from editor
        ui->buttonSave->setEnabled(true);
        const QString &new_name = ui->lineEditName->text();
        if (!new_name.isEmpty()) {
            QString text = ui->advancedEditor->toPlainText();
            QRegularExpression regex("(^|\n)Name=[^\n]*(\n|$)");
            QRegularExpressionMatch regex_match = regex.match(text);
            int index = regex_match.capturedStart();
            int length = regex_match.capturedLength();

            if (index != -1) {
                text.replace(index, length, "\nName=" + new_name + "\n"); // replace only first match
                ui->advancedEditor->setText(text);
            }
        }
    } else { // if running command from add-custom-app window
        if (!add->ui->lineEditName->text().isEmpty()
                && !add->ui->lineEditCommand->text().isEmpty()
                && add->ui->listWidgetCategories->count() != 0)
            add->ui->buttonSave->setEnabled(true);
        else
            add->ui->buttonSave->setEnabled(false);
    }
}

// change the command string
void MainWindow::changeCommand()
{
    if (ui->lineEditCommand->isEnabled()) { // started from editor
        ui->buttonSave->setEnabled(true);
        const QString &new_command = ui->lineEditCommand->text();
        if (!new_command.isEmpty()) {
            QString text = ui->advancedEditor->toPlainText();
            text.replace(QRegularExpression("(^|\n)Exec=[^\n]*(\n|$)"), "\nExec=" + new_command + "\n");
            ui->advancedEditor->setText(text);
        }
    } else { // if running command from add-custom-app window
        const QString &new_command = add->ui->lineEditCommand->text();
        if (!new_command.isEmpty()
                && !add->ui->lineEditName->text().isEmpty()
                && add->ui->listWidgetCategories->count() != 0)
            add->ui->buttonSave->setEnabled(true);
        else
            add->ui->buttonSave->setEnabled(false);
    }
}

// change the comment string
void MainWindow::changeComment()
{
    if (ui->lineEditCommand->isEnabled()) { // started from editor
        ui->buttonSave->setEnabled(true);
        const QString &new_comment = ui->lineEditComment->text();
        QString text = ui->advancedEditor->toPlainText();
        if (!new_comment.isEmpty()) {
            if (text.contains("Comment=")) {
                text.replace(QRegularExpression("(^|\n)Comment=[^\n]*(\n|$)"), "\nComment=" + new_comment + "\n");
            } else {
                text = text.trimmed();
                text.append("\nComment=" + new_comment + "\n");
            }
        } else {
            text.remove(QRegularExpression("(^|\n)Comment=[^\n]*(\n|$)"));
        }
        ui->advancedEditor->setText(text);
    }
}

// enable delete button for category
void MainWindow::enableDelete()
{
    ui->pushDelete->setEnabled(true);
}

// delete selected category
void MainWindow::delCategory()
{
    int row;
    if (ui->lineEditCommand->isEnabled()) { // started from editor
        ui->buttonSave->setEnabled(true);
        row = ui->listWidgetEditCategories->currentRow();
        QListWidgetItem *item = ui->listWidgetEditCategories->takeItem(row);
        QString text = ui->advancedEditor->toPlainText();
        int indexCategory = text.indexOf(QRegularExpression("(^|\n)Categories=[^\n]*(\n|$)"));
        int indexToDelete = text.indexOf(item->text() + ";", indexCategory);
        text.remove(indexToDelete, item->text().length() + 1);
        ui->advancedEditor->setText(text);
        if (ui->listWidgetEditCategories->count() == 0) {
            ui->pushDelete->setDisabled(true);
            ui->buttonSave->setDisabled(true);
        }
    } else { // if running command from add-custom-app window
        row = add->ui->listWidgetCategories->currentRow();
        add->ui->listWidgetCategories->takeItem(row);
        if (add->ui->listWidgetCategories->count() == 0) {
            add->ui->pushDelete->setDisabled(true);
            add->ui->buttonSave->setDisabled(true);
        }
    }
}

// change startup notify
void MainWindow::changeNotify(bool checked)
{
    ui->buttonSave->setEnabled(true);
    const QString &str = QString(checked ? "true" : "false");
    QString text = ui->advancedEditor->toPlainText();
    if (text.contains("StartupNotify=")) {
        text.replace(QRegularExpression("(^|\n)StartupNotify=[^\n]*(\n|$)"), "\nStartupNotify=" + str + "\n");
    } else {
        text = text.trimmed();
        text.append("\nStartupNotify=" + str);
    }
    ui->advancedEditor->setText(text);
}

// hide or show the item in the menu
void MainWindow::changeHide(bool checked)
{
    ui->buttonSave->setEnabled(true);
    const QString &str = QString(checked ? "true" : "false");
    QString text = ui->advancedEditor->toPlainText().trimmed();
    if (text.contains("NoDisplay=")) {
        text.replace(QRegularExpression("(^|\n)NoDisplay=[^\n]*(\n|$)"), "\nNoDisplay=" + str + "\n");
    } else {
        QString new_text;
        for (const QString &line : text.split("\n")) {
            new_text.append(line + "\n");
            if (line.startsWith("Exec="))
                new_text.append("NoDisplay=" + str + "\n");
        }
        text = new_text;
    }
    ui->advancedEditor->setText(text);
}

// change "run in terminal" setting
void MainWindow::changeTerminal(bool checked)
{
    ui->buttonSave->setEnabled(true);
    const QString &str = QString(checked ? "true" : "false");
    QString text = ui->advancedEditor->toPlainText();
    if (text.contains("Terminal=")) {
        text.replace(QRegularExpression("(^|\n)Terminal=[^\n]*(\n|$)"), "\nTerminal=" + str + "\n");
    } else {
        text = text.trimmed();
        text.append("\nTerminal=" + str);
    }
    ui->advancedEditor->setText(text);
}


// list categories of the displayed items
QStringList MainWindow::listCategories()
{
    QStringList categories;
    QHashIterator<QString, QString> i(hashCategories);
    while (i.hasNext()) {
        i.next();
        categories << i.value();
    }
    categories.removeDuplicates();
    categories = categories.filter(QRegularExpression("^(?!<Not>).*$"));
    categories.sort();
    return categories;
}

// display add category message box
void MainWindow::addCategoryMsgBox()
{
    const QStringList &categories = listCategories();

    QWidget *window = new QWidget(add, Qt::Dialog);
    window->setWindowTitle(tr("Choose category"));
    window->resize(250, 80);

    QDialogButtonBox *buttonBox = new QDialogButtonBox();

    comboBox->clear();
    //comboBox->setEditable(true);
    comboBox->addItems(categories);
    comboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // because we want to display the buttons in reverse order we use counter-intuitive roles.
    buttonBox->addButton(tr("Cancel"), QDialogButtonBox::AcceptRole);
    buttonBox->addButton(tr("OK"), QDialogButtonBox::RejectRole);
    connect(buttonBox, &QDialogButtonBox::accepted, window, &QWidget::close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &MainWindow::addCategory);
    connect(buttonBox, &QDialogButtonBox::rejected, window, &QWidget::close);

    QFormLayout *layout = new QFormLayout;
    layout->addRow(comboBox);
    layout->addRow(buttonBox);

    window->setLayout(layout);
    window->show();
}

void MainWindow::centerWindow()
{
    QRect screenGeometry = qApp->screens().first()->geometry();
    int x = (screenGeometry.width()-this->width()) / 2;
    int y = (screenGeometry.height()-this->height()) / 2;
    this->move(x, y);
}

// add selected categorory to the .desktop file
void MainWindow::addCategory()
{
    const QString &str = comboBox->currentText();
    QString text = ui->advancedEditor->toPlainText();
    int index = text.indexOf(QRegularExpression("(^|\n)Categories=[^\n]*(\n|$)"));
    index = text.indexOf(QRegularExpression("(\n|$)"), index + 1); // find the end of the string
    if (ui->lineEditCommand->isEnabled()) { // started from editor
        if (ui->listWidgetEditCategories->findItems(str, Qt::MatchFixedString).isEmpty()) {
            ui->buttonSave->setEnabled(true);
            text.insert(index, str + ";");
            ui->listWidgetEditCategories->addItem(str);
            ui->advancedEditor->setText(text);
            ui->pushDelete->setEnabled(true);
            if (ui->listWidgetEditCategories->count() == 0)
                ui->buttonSave->setDisabled(true);
        }
    } else { // if running command from add-custom-app window
        if (add->ui->listWidgetCategories->findItems(str, Qt::MatchFixedString).isEmpty()) {
            text.insert(index, str + ";");
            add->ui->listWidgetCategories->addItem(str);
            add->ui->pushDelete->setEnabled(true);
            if (!add->ui->lineEditName->text().isEmpty()
                    && !add->ui->lineEditCommand->text().isEmpty()
                    && add->ui->listWidgetCategories->count() != 0)
                add->ui->buttonSave->setEnabled(true);
            else
                add->ui->buttonSave->setEnabled(false);
        }
    }
}

// display add application message box
void MainWindow::addAppMsgBox()
{
    QStringList categories;
    QTreeWidgetItemIterator it(ui->treeWidget);
    // list top level items
    while (*it && !(*it)->parent()) {
        categories << (*it)->text(0);
        ++it;
    }
    if (ui->buttonSave->isEnabled() && save())
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


// Save button clicked
void MainWindow::on_buttonSave_clicked()
{
    QDir dir;
    QString file_name = current_item->text(1);
    QFileInfo fi(file_name);
    QString base_name = fi.fileName();
    if (!QFileInfo::exists(dir.homePath() + "/.local/share/applications/"))
        dir.mkpath(dir.homePath() + "/.local/share/applications/");
    QString out_name = dir.homePath() + "/.local/share/applications/" + base_name;
    QFile out(out_name);
    if (!out.open(QFile::WriteOnly | QFile::Text))
        QMessageBox::critical(this, tr("Error"), tr("Could not save the file"));
    all_local_desktop_files << out_name;
    out.write(ui->advancedEditor->toPlainText().toUtf8());
    out.flush();
    out.close();
    if (system("pgrep xfce4-panel") == 0)
        system("xfce4-panel --restart");
    ui->buttonSave->setDisabled(true);
    findReloadItem(base_name);
}

// About button clicked
void MainWindow::on_buttonAbout_clicked()
{
    this->hide();
    QMessageBox msgBox(QMessageBox::NoIcon,
                       tr("About MX Menu Editor"), "<p align=\"center\"><b><h2>" +
                       tr("MX Menu Editor") + "</h2></b></p><p align=\"center\">" + tr("Version: ") + VERSION +
                       "</p><p align=\"center\"><h3>" + tr("Program for editing Xfce menu") +
                       "</h3></p><p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br /></p><p align=\"center\">" +
                       tr("Copyright (c) MX Linux") + "<br /><br /></p>");
    QPushButton *btnLicense = msgBox.addButton(tr("License"), QMessageBox::HelpRole);
    QPushButton *btnChangelog = msgBox.addButton(tr("Changelog"), QMessageBox::HelpRole);
    QPushButton *btnCancel = msgBox.addButton(tr("Cancel"), QMessageBox::NoRole);
    btnCancel->setIcon(QIcon::fromTheme("window-close"));

    msgBox.exec();

    if (msgBox.clickedButton() == btnLicense) {
        system("xdg-open file:///usr/share/doc/mx-menu-editor/license.html");
    } else if (msgBox.clickedButton() == btnChangelog) {
        QDialog *changelog = new QDialog(this);
        changelog->resize(600, 500);

        QTextEdit *text = new QTextEdit;
        text->setReadOnly(true);
        proc.start("zless", QStringList{"/usr/share/doc/" + QFileInfo(QCoreApplication::applicationFilePath()).fileName()  + "/changelog.gz"});
        proc.waitForFinished();
        if (proc.exitCode() != 0)
            return;
        text->setText(proc.readAllStandardOutput());

        QPushButton *btnClose = new QPushButton(tr("&Close"));
        btnClose->setIcon(QIcon::fromTheme("window-close"));
        connect(btnClose, &QPushButton::clicked, changelog, &QDialog::close);

        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(text);
        layout->addWidget(btnClose);
        changelog->setLayout(layout);
        changelog->exec();
    }
    this->show();
}

void MainWindow::setEnabled(QString)
{
    ui->buttonSave->setEnabled(true);
}

// Help button clicked
void MainWindow::on_buttonHelp_clicked()
{
    QLocale locale;
    QString lang = locale.bcp47Name();

    QString url = "/usr/share/doc/mx-menu-editor/mx-menu-editor.html";

    if (lang.startsWith("fr"))
        url = "https://mxlinux.org/wiki/help-files/help-mx-editeur-de-menu";

    system("xdg-open " + url.toUtf8());
}

// Cancel button clicked
void MainWindow::on_buttonCancel_clicked()
{
    if (ui->buttonSave->isEnabled())
        save();
    qApp->quit();
}

// ask whether to save edits or not
bool MainWindow::save()
{
    if (ui->buttonSave->isEnabled()) {
        if (QMessageBox::Save == QMessageBox::question(this, tr("Save changes?"), tr("Do you want to save your edits?"),
                                                       QMessageBox::Save, QMessageBox::Cancel)) {
            on_buttonSave_clicked();
            return true;
        }
    }
    ui->buttonSave->setDisabled(true);
    return false;
}

// delete .local file and reload files
void MainWindow::on_pushRestoreApp_clicked()
{
    QString file_name = current_item->text(1);
    QFileInfo fi(file_name);
    QString base_name = fi.fileName();
    QFile file(file_name);
    file.remove();
    all_local_desktop_files = listDesktopFiles("", QDir::homePath() + "/.local/share/applications");
    ui->pushRestoreApp->setDisabled(true);
    findReloadItem(base_name);
}

// find and reload item
void MainWindow::findReloadItem(QString base_name)
{
    ui->treeWidget->setCurrentItem(current_item); // change current item back to original selection
    ui->treeWidget->setCurrentItem(current_item->parent()); // change current item to reload category
    QTreeWidgetItemIterator it(current_item->treeWidget());
    while (*it) {
        QFileInfo fi((*it)->text(1));
        if ((fi.fileName() == base_name)) {
            ui->treeWidget->setCurrentItem(*it);
            return;
        }
        ++it;
    }
}

// Find icon file by name
QPixmap MainWindow::findIcon(QString icon_name, const QSize &size)
{
    if (icon_name.isEmpty())
        return QPixmap();
    if (QFileInfo::exists("/" + icon_name))
        return QIcon(icon_name).pixmap(size);

    QString search_term = icon_name;
    if (!icon_name.endsWith(".png") && !icon_name.endsWith(".svg") && !icon_name.endsWith(".xpm"))
        search_term = icon_name + ".*";

    icon_name.remove(QRegularExpression("\\.png$|\\.svg$|\\.xpm$"));

    // return the icon from the theme if it exists
    if (!QIcon::fromTheme(icon_name).isNull())
        return QIcon::fromTheme(icon_name).pixmap(size);

    // Try to find in most obvious places
    QStringList search_paths { QDir::homePath() + "/.local/share/icons/",
                               "/usr/share/pixmaps/",
                               "/usr/local/share/icons/",
                               "/usr/share/icons/hicolor/48x48/apps/" };
    for (const QString &path : search_paths) {
        if (!QFileInfo::exists(path)) {
            search_paths.removeOne(path);
            continue;
        }
        for (const QString &ext : {".png", ".svg", ".xpm"} ) {
            QString file = path + icon_name + ext;
            if (QFileInfo::exists(file))
                return QIcon(file).pixmap(QSize());
        }
    }

    // Search recursive
    search_paths.append("/usr/share/icons/hicolor/48x48/");
    search_paths.append("/usr/share/icons/hicolor/");
    search_paths.append("/usr/share/icons/");
    proc.start("find", QStringList{search_paths << "-iname" << search_term << "-print" << "-quit"});
    proc.waitForFinished();
    QString out = proc.readAllStandardOutput().trimmed();
    if (out.isEmpty())
        return QPixmap();
    return QIcon(out).pixmap(size);
}
