/**********************************************************************
 *  mainwindow.cpp
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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ui_addappdialog.h"
#include "version.h"

#include <QProcess>
#include <QFileDialog>
#include <QTextStream>
#include <QFormLayout>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QHashIterator>
#include <QTextCodec>

#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MainWindow),
    add(new AddAppDialog)
{
    qDebug().noquote() << QCoreApplication::applicationName() << "version:" << VERSION;
    ui->setupUi(this);
    setWindowFlags(Qt::Window); // for the close, min and max buttons

    if (ui->buttonSave->icon().isNull()) {
        ui->buttonSave->setIcon(QIcon(":/icons/dialog-ok.svg"));
    }
    if (add->ui->buttonSave->icon().isNull()) {
        add->ui->buttonSave->setIcon(QIcon(":/icons/dialog-ok.svg"));
    }

    comboBox = new QComboBox;

    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    all_local_desktop_files = listDesktopFiles("\"\"", QDir::homePath() + "/.local/share/applications/*");
    all_usr_desktop_files = listDesktopFiles("\"\"", "/usr/share/applications/*");

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

}

MainWindow::~MainWindow()
{
    delete ui;
}

// util function for getting bash command output and error code
Output MainWindow::getCmdOut(const QString &cmd)
{
    QProcess *proc = new QProcess();
    proc->setReadChannelMode(QProcess::MergedChannels);
    proc->start("/bin/bash", QStringList() << "-c" << cmd);
    proc->waitForFinished();
    Output out = {proc->exitCode(), proc->readAll().trimmed()};
    delete proc;
    return out;
}

// load menu files
void MainWindow::loadMenuFiles()
{
    QString home_path = QDir::homePath();
    QStringList menu_items;
    const QStringList menu_files = listMenuFiles();

    // process each menu_file
    for (QString file_name : menu_files) {
        QFile file(file_name);
        if (file.open(QIODevice::ReadOnly)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine();
                QString name;
                // find <Name> of the item
                if (line.contains("<Name>") ) {
                    // find <Directory> for the <Name>
                    if (!in.atEnd()) {
                        line = in.readLine();
                        if (line.contains("<Directory>")) {
                            line = line.remove("<Directory>").remove("</Directory>").trimmed();
                            QString file_name = home_path + "/.local/share/desktop-directories/" + line;
                            QFile *file = new QFile(file_name);
                            if (!file->exists()) { // use /usr if the file is not present in ~/.local
                                QString file_name = "/usr/share/desktop-directories/" + line;
                                file->setFileName(file_name);
                            }
                            name = getCatName(file); // get the Name= from .directory file
                            if (!name.isEmpty() && name != "Other" && name != "Wine") {
                                menu_items << name;
                            }
                            delete file;
                            // Find <Category> and <Filename> and add them in hashCategory and hashInclude
                            while (!(in.atEnd() || line.contains("</Include>"))) {
                                line = in.readLine();
                                if (line.contains("<Category>")) {
                                    line = line.remove("<Category>").remove("</Category>").trimmed();
                                    if (!hashCategories.values(name).contains(line)) {
                                        hashCategories.insertMulti(name, line); //each menu category displays a number of categories
                                    }
                                }
                                if (line.contains("<Filename>")) {
                                    line = line.remove("<Filename>").remove("</Filename>").trimmed();
                                    if (!hashInclude.values(name).contains(line)) {
                                        hashInclude.insertMulti(name, line); //each menu category contains a number of files
                                    }
                                }
                            }
                            // find <Exludes> and add them in hashExclude
                            while (!(in.atEnd() || line.contains("</Exclude>") || line.contains("<Menu>"))) {
                                line = in.readLine();
                                if (line.contains("<Exclude>")) {
                                    while (!(in.atEnd() || line.contains("</Exclude>"))) {
                                        line = in.readLine();
                                        if(line.contains("<Filename>")) {
                                            line = line.remove("<Filename>").remove("</Filename>").trimmed();
                                            if (!hashExclude.values(name).contains(line)) {
                                                hashExclude.insertMulti(name, line); //each menu category contains a number of files
                                            }
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
QString MainWindow::getCatName(const QFile *file)
{
    QString cmd = QString("grep Name= %1").arg(file->fileName());
    Output out = getCmdOut(cmd.toUtf8());
    if (out.exit_code == 0) {
        return out.str.remove("Name=");
    } else {
        return "";
    }
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
        if (item.endsWith(".menu")) {
            menu_files << item;
        }
    }
    return menu_files;
}

// display sorted list of menu items in the treeWidget
void MainWindow::displayList(QStringList menu_items) {
    QTreeWidgetItem *topLevelItem = nullptr;
    ui->treeWidget->setHeaderLabel("");
    ui->treeWidget->setSortingEnabled(true);
    menu_items.removeDuplicates();
    foreach (const QString &item, menu_items) {
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
        if (ui->buttonSave->isEnabled()) {
            if (save()) {
                return;
            }
        }
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

        foreach (const QString &file, includes) {
            includes_usr << "/usr/share/applications" + file;
            includes_local << QDir::homePath() + "/.local/share/applications/" + file;
        }

        // determine search string for all categories to be listead under menu category
        QString search_string;
        foreach (const QString &category, categories) {
            if (search_string.isEmpty()) {
                search_string = "Categories=.*\"" + category + "\"";
            } else {
                search_string += "\\|Categories=.*\"" + category + "\"";
            }
        }

        // list .desktop files from /usr and .local
        QStringList usr_desktop_files = listDesktopFiles(search_string, "/usr/share/applications/*");
        QStringList local_desktop_files = listDesktopFiles(search_string, QDir::homePath() + "/.local/share/applications/*");

        // add included files
        usr_desktop_files.append(includes_usr);
        local_desktop_files.append(includes_local);

        // exclude files
        foreach (const QString &base_name, excludes) {
            usr_desktop_files.removeAll("/usr/share/applications/" + base_name);
        }
        foreach (const QString &base_name, excludes) {
            local_desktop_files.removeAll(QDir::homePath() + "/.local/share/applications/" + base_name);
        }

        // list of names without path
        QStringList local_base_names;
        foreach (const QString &local_name, all_local_desktop_files) {
            QFileInfo f_local(local_name);
            local_base_names << f_local.fileName();
        }
        QStringList usr_base_names;
        foreach (const QString &usr_name, all_usr_desktop_files) {
            QFileInfo f_usr(usr_name);
            usr_base_names << f_usr.fileName();
        }

        // parse local .desktop files
        foreach (const QString &local_name, local_desktop_files) {
            QFileInfo fi_local(local_name);
            addToTree(local_name);
            all_local_desktop_files << local_name;
            if (usr_base_names.contains(fi_local.fileName())) {
                // find item and set restore flag
                QTreeWidgetItemIterator it(item);
                while (*it) {
                    if ((*it)->text(1).contains(local_name)) {
                        (*it)->setText(2, "restore");
                    }
                    ++it;
                }
            }
        }

        // parse usr .desktop files
        foreach (const QString &file, usr_desktop_files) {
            QFileInfo fi(file);
            QString base_name = fi.fileName();
            // add items only for files that are not in the list of local .desktop files
            if (!local_base_names.contains(base_name)) {
                addToTree(file);
            } else {
                // find item and set restore flag
                QTreeWidgetItemIterator it(item);
                while (*it) {
                    if ((*it)->text(1).contains(base_name)) {
                        (*it)->setText(2, "restore");
                    }
                    ++it;
                }
            }
        }
        item->sortChildren(true, Qt::AscendingOrder);
        item->setExpanded(true);
        current_item = ui->treeWidget->currentItem(); // remember the current_item in case user selects another item before saving
    } else {
        loadItem(ui->treeWidget->currentItem(), 0);
    }
}

// add .desktop item to treeWidget
void MainWindow::addToTree(QString file_name)
{
    QFile file(file_name);
    if (file.exists()) {
        QString cmd = "grep -m1 ^Name= \"" + file_name.toUtf8() + "\"| cut  -d'=' -f2";
        QString app_name = getCmdOut(cmd).str;
        // add item as childItem to treeWidget
        QTreeWidgetItem *childItem = new QTreeWidgetItem(ui->treeWidget->currentItem());
        if (isHidden(file_name)) {
            childItem->setForeground(0, QBrush(Qt::gray));
        }
        file_name.insert(0, "\"");
        file_name.append("\"");
        childItem->setText(0, app_name);
        childItem->setText(1, file_name);
    }
}

// list .desktop files
QStringList MainWindow::listDesktopFiles(const QString &search_string, const QString &location)
{
    QStringList listDesktop;
    if (!search_string.isEmpty()) {
        QString cmd = QString("grep -Elr %1 %2").arg(search_string).arg(location);
        Output out = getCmdOut(cmd);
        if (!out.str.isEmpty()) {
            listDesktop = out.str.split("\n");
        }
    }
    return listDesktop;
}

// load selected item to be edited
void MainWindow::loadItem(QTreeWidgetItem *item, int)
{
    // execute if not topLevel item is selected
    if (item->parent()) {
        if (ui->buttonSave->isEnabled()) {
            if (save()) {
                return;
            }
        }
        QString file_name = ui->treeWidget->currentItem()->text(1);
        resetInterface();
        enableEdit();

        QString cmd;
        Output out;

        out = getCmdOut("cat " + file_name.toUtf8());
        ui->advancedEditor->setText(out.str);
        // load categories
        out = getCmdOut("grep ^Categories= " + file_name.toUtf8() + " | cut -f2- -d=");
        if (out.str.endsWith(";")) {
            out.str.remove(out.str.length() - 1, 1);
        }
        QStringList categories = out.str.split(";");
        ui->listWidgetEditCategories->addItems(categories);
        // load name, command, comment
        out = getCmdOut("grep -m1 ^Name= " + file_name.toUtf8() + " | cut -f2- -d=");
        ui->lineEditName->setText(out.str);
        out = getCmdOut("grep -m1 ^Comment= " + file_name.toUtf8() + " | cut -f2- -d=");
        ui->lineEditComment->setText(out.str);
        ui->lineEditComment->home(false);
        out = getCmdOut("grep -m1 ^Exec= " + file_name.toUtf8() + " | cut -f2- -d=");
        ui->lineEditCommand->setText(out.str);
        ui->lineEditCommand->home(false);
        // load options
        out = getCmdOut("grep -m1 ^StartupNotify= " + file_name.toUtf8() + " | cut -f2- -d=");
        if (out.str == "true") {
            ui->checkNotify->setChecked(true);
        }
        out = getCmdOut("grep -m1 ^NoDisplay= " + file_name.toUtf8() + " | cut -f2- -d=");
        if (out.str == "true") {
            ui->checkHide->setChecked(true);
        }
        out = getCmdOut("grep -m1 ^Terminal= " + file_name.toUtf8() + " | cut -f2- -d=");
        if (out.str == "true") {
            ui->checkRunInTerminal->setChecked(true);
        }
        out = getCmdOut("grep -m1 ^Icon= " + file_name.toUtf8() + " | cut -f2- -d=");
        if (!out.str.isEmpty()) {
            QSize size = ui->labelIcon->size();
            QString icon = out.str;
            if (QFile(icon).exists()) {
                ui->labelIcon->setPixmap(QPixmap(icon).scaled(size));
            } else {
                ui->labelIcon->setPixmap(QPixmap(findIcon(icon)).scaled(size));
            }
        }

        // enable RestoreApp button if flag is set up for item
        if (ui->treeWidget->currentItem()->text(2) == "restore") {
            ui->pushRestoreApp->setEnabled(true);
        } else {
            ui->pushRestoreApp->setEnabled(false);
        }
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
    ui->pushChangeIcon->setEnabled(true);
}

// change the icon of the application
void MainWindow::changeIcon()
{
    QFileDialog dialog;
    QString selected;
    dialog.setFilter(QDir::Hidden);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilter(tr("Image Files (*.png *.jpg *.bmp *.xpm)"));
    dialog.setDirectory("/usr/share/icons");
    if (dialog.exec()) {
        QStringList selected_list = dialog.selectedFiles();
        selected = selected_list.at(0);
    }
    if (!selected.isEmpty()) {
        QString text = ui->advancedEditor->toPlainText();
        if (ui->lineEditCommand->isEnabled()) { // started from editor
            ui->buttonSave->setEnabled(true);
            text.replace(QRegExp("(^|\n)Icon=[^\n]*(\n|$)"), "\nIcon=" + selected + "\n");
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
        QString new_name = ui->lineEditName->text();
        if (!new_name.isEmpty()) {
            QString text = ui->advancedEditor->toPlainText();
            QRegExp regex("(^|\n)Name=[^\n]*(\n|$)");
            int index = text.indexOf(regex, 0);
            if (index != -1) {
                text.replace(index, regex.matchedLength(), "\nName=" + new_name + "\n");
                ui->advancedEditor->setText(text);
            }
        }
    } else { // if running command from add-custom-app window
        if (!add->ui->lineEditName->text().isEmpty() && !add->ui->lineEditCommand->text().isEmpty() && add->ui->listWidgetCategories->count() != 0) {
            add->ui->buttonSave->setEnabled(true);
        } else {
            add->ui->buttonSave->setEnabled(false);
        }
    }
}

// change the command string
void MainWindow::changeCommand()
{
    if (ui->lineEditCommand->isEnabled()) { // started from editor
        ui->buttonSave->setEnabled(true);
        QString new_command = ui->lineEditCommand->text();
        if (!new_command.isEmpty()) {
            QString text = ui->advancedEditor->toPlainText();
            text.replace(QRegExp("(^|\n)Exec=[^\n]*(\n|$)"), "\nExec=" + new_command + "\n");
            ui->advancedEditor->setText(text);
        }
    } else { // if running command from add-custom-app window
        QString new_command = add->ui->lineEditCommand->text();
        if (!new_command.isEmpty() && !add->ui->lineEditName->text().isEmpty() && add->ui->listWidgetCategories->count() != 0) {
            add->ui->buttonSave->setEnabled(true);
        } else {
            add->ui->buttonSave->setEnabled(false);
        }
    }
}

// change the comment string
void MainWindow::changeComment()
{
    if (ui->lineEditCommand->isEnabled()) { // started from editor
        ui->buttonSave->setEnabled(true);
        QString text = ui->advancedEditor->toPlainText();
        QString new_comment = ui->lineEditComment->text();
        if (!new_comment.isEmpty()) {
            if (text.contains("Comment=")) {
                text.replace(QRegExp("(^|\n)Comment=[^\n]*(\n|$)"), "\nComment=" + new_comment + "\n");
            } else {
                text = text.trimmed();
                text.append("\nComment=" + new_comment + "\n");
            }
        } else {
            text.remove(QRegExp("(^|\n)Comment=[^\n]*(\n|$)"));
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
        int indexCategory = text.indexOf(QRegExp("(^|\n)Categories=[^\n]*(\n|$)"));
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
    QString text = ui->advancedEditor->toPlainText();
    QString str = QString(checked ? "true" : "false");
    if (text.contains("StartupNotify=")) {
        text.replace(QRegExp("(^|\n)StartupNotify=[^\n]*(\n|$)"), "\nStartupNotify=" + str + "\n");
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
    QString text = ui->advancedEditor->toPlainText().trimmed();
    QString str = QString(checked ? "true" : "false");
    if (text.contains("NoDisplay=")) {
        text.replace(QRegExp("(^|\n)NoDisplay=[^\n]*(\n|$)"), "\nNoDisplay=" + str + "\n");
    } else {
        QString new_text;
        foreach (const QString &line, text.split("\n")) {
            new_text.append(line + "\n");
            if (line.startsWith("Exec=")) {
                new_text.append("NoDisplay=" + str + "\n");
            }
        }
        text = new_text;
    }
    ui->advancedEditor->setText(text);
}

// change "run in terminal" setting
void MainWindow::changeTerminal(bool checked)
{
    ui->buttonSave->setEnabled(true);
    QString text = ui->advancedEditor->toPlainText();
    QString str = QString(checked ? "true" : "false");
    if (text.contains("Terminal=")) {
        text.replace(QRegExp("(^|\n)Terminal=[^\n]*(\n|$)"), "\nTerminal=" + str + "\n");
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
    categories = categories.filter(QRegExp("^(?!<Not>).*$"));
    categories.sort();
    return categories;
}

// display add category message box
void MainWindow::addCategoryMsgBox()
{
    QStringList categories = listCategories();

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

// add selected categorory to the .desktop file
void MainWindow::addCategory()
{
    QString str = comboBox->currentText();
    QString text = ui->advancedEditor->toPlainText();
    int index = text.indexOf(QRegExp("(^|\n)Categories=[^\n]*(\n|$)"));
    index = text.indexOf(QRegExp("(\n|$)"), index + 1); // find the end of the string
    if (ui->lineEditCommand->isEnabled()) { // started from editor
        if (ui->listWidgetEditCategories->findItems(str, Qt::MatchFixedString).isEmpty()) {
            ui->buttonSave->setEnabled(true);
            text.insert(index, str + ";");
            ui->listWidgetEditCategories->addItem(str);
            ui->advancedEditor->setText(text);
            ui->pushDelete->setEnabled(true);
            if (ui->listWidgetEditCategories->count() == 0) {
                ui->buttonSave->setDisabled(true);
            }
        }
    } else { // if running command from add-custom-app window
        if (add->ui->listWidgetCategories->findItems(str, Qt::MatchFixedString).isEmpty()) {
            text.insert(index, str + ";");
            add->ui->listWidgetCategories->addItem(str);
            add->ui->pushDelete->setEnabled(true);
            if (!add->ui->lineEditName->text().isEmpty() && !add->ui->lineEditCommand->text().isEmpty() && add->ui->listWidgetCategories->count() != 0) {
                add->ui->buttonSave->setEnabled(true);
            } else {
                add->ui->buttonSave->setEnabled(false);
            }
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
    if (ui->buttonSave->isEnabled()) {
        if (save()) {
            return;
        }
    }
    add->show();
    resetInterface();
    ui->treeWidget->collapseAll();
    disconnect(add->ui->selectCommand, 0, 0, 0);
    disconnect(add->ui->pushChangeIcon, 0, 0, 0);
    disconnect(add->ui->lineEditName, 0, 0, 0);
    disconnect(add->ui->lineEditCommand, 0, 0, 0);
    disconnect(add->ui->lineEditComment, 0, 0, 0);
    disconnect(add->ui->pushAdd, 0, 0, 0);
    disconnect(add->ui->pushDelete, 0, 0, 0);
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
    QString file_name = current_item->text(1).remove("\"");
    QFileInfo fi(file_name);
    QString base_name = fi.fileName();
    QString out_name = dir.homePath() + "/.local/share/applications/" + base_name;
    QFile out(out_name);
    if (!out.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not save the file"));
    }
    all_local_desktop_files << out_name;
    out.write(ui->advancedEditor->toPlainText().toUtf8());
    out.flush();
    out.close();
    if (system("pgrep xfce4-panel") == 0) {
        system("xfce4-panel --restart");
    }
    ui->buttonSave->setDisabled(true);
    findReloadItem(base_name);
}

// About button clicked
void MainWindow::on_buttonAbout_clicked()
{
    this->hide();
    QMessageBox msgBox(QMessageBox::NoIcon,
                       tr("About MX Menu Editor"), "<p align=\"center\"><b><h2>" +
                       tr("MX Menu Editor") + "</h2></b></p><p align=\"center\">" + tr("Version: ") + VERSION + "</p><p align=\"center\"><h3>" +
                       tr("Program for editing Xfce menu") +
                       "</h3></p><p align=\"center\"><a href=\"http://mxlinux.org\">http://mxlinux.org</a><br /></p><p align=\"center\">" +
                       tr("Copyright (c) MX Linux") + "<br /><br /></p>", nullptr, this);
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
        text->setText(getCmdOut("zless /usr/share/doc/" + QFileInfo(QCoreApplication::applicationFilePath()).fileName()  + "/changelog.gz").str);

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

    QString url = "/usr/share/doc/mx-menu-editor/help/mx-menu-editor.html";

    if (lang.startsWith("fr")) {
        url = "https://mxlinux.org/wiki/help-files/help-mx-editeur-de-menu";
    }

    system("xdg-open " + url.toUtf8());
}

// Cancel button clicked
void MainWindow::on_buttonCancel_clicked()
{
    if (ui->buttonSave->isEnabled()) {
        save();
    }
    qApp->quit();
}

// ask whether to save edits or not
bool MainWindow::save()
{
    if (ui->buttonSave->isEnabled()) {
        int ans = QMessageBox::question(this, tr("Save changes?"), tr("Do you want to save your edits?"), QMessageBox::Save, QMessageBox::Cancel);
        if (ans == QMessageBox::Save) {
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
    file_name.remove("\"");
    QFileInfo fi(file_name);
    QString base_name = fi.fileName();
    QFile file(file_name);
    file.remove();
    all_local_desktop_files = listDesktopFiles("\"\"", QDir::homePath() + "/.local/share/applications/*");
    ui->pushRestoreApp->setDisabled(true);
    findReloadItem(base_name);
}

// find and reload item
void MainWindow::findReloadItem(QString base_name)
{
    base_name.remove("\"");
    ui->treeWidget->setCurrentItem(current_item); // change current item back to original selection
    ui->treeWidget->setCurrentItem(current_item->parent()); // change current item to reload category
    QTreeWidgetItemIterator it(current_item->treeWidget());
    while (*it) {
        QFileInfo fi((*it)->text(1).remove("\""));
        if ((fi.fileName() == base_name)) {
            ui->treeWidget->setCurrentItem(*it);
            return;
        }
        ++it;
    }
}

// find icon file location using the icon name form .desktop file
QString MainWindow::findIcon(const QString &icon_name)
{
    Output out;
    const QStringList extList({".png", ".svg", ".xpm"});

    out = getCmdOut("xfconf-query -c xsettings -p /Net/IconThemeName");
    if (!out.str.isEmpty()) {
        QString dir = "/usr/share/icons/" + out.str;
        if (QDir(dir).exists()) {
            foreach (const QString &ext, extList) {
                out = getCmdOut("find " + dir + " -iname " + icon_name + ext);
                if (!out.str.isEmpty()) {
                    QStringList files = out.str.split("\n");
                    return findBiggest(files);
                } else {
                    out = getCmdOut("find " + QDir::homePath() + "/.local/share/icons " + "/usr/share/icons /usr/share/pixmaps -iname " + icon_name + ext);
                    if (!out.str.isEmpty()) {
                        QStringList files = out.str.split("\n");
                        return findBiggest(files);
                    }
                }
            }
        }
    }
    return "";
}

// find largest icon
QString MainWindow::findBiggest(const QStringList &files)
{
    qint64 max = 0;
    QString name_biggest;
    for (const QString &file : files) {
        QFile f(file);
        qint64 size = f.size();
        if (size >= max) {
            name_biggest = file;
            max = size;
        }
    }
    return name_biggest;
}
