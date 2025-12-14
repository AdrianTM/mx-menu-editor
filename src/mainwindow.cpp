/**********************************************************************
 *  mainwindow.cpp
 **********************************************************************
 * Copyright (C) 2015-2025 MX Authors
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
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Walloc-size-larger-than="
#endif
#include <QHash>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QSet>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QTextDocument>
#include <QTextStream>
#include <utility>
#include <algorithm>

#include "ui_addappdialog.h"

namespace
{
constexpr int ExecRole = Qt::UserRole + 1;

// Cached regex patterns to avoid repeated construction
const QRegularExpression regexTrailingSemicolon(QStringLiteral(";$"));
const QRegularExpression regexIconLine(QStringLiteral("(^|\n)Icon="));
const QRegularExpression regexIconFull(QStringLiteral("(^|\n)Icon=[^\n]*(\n|$)"));
const QRegularExpression regexExecFull(QStringLiteral("(^|\n)Exec=[^\n]*(\n|$)"));
const QRegularExpression regexCommentFull(QStringLiteral("(^|\n)Comment=[^\n]*(\n|$)"));
const QRegularExpression regexCategoriesFull(QStringLiteral("(^|\n)Categories=[^\n]*(\n|$)"));
const QRegularExpression regexNewlineOrEnd(QStringLiteral("(\n|$)"));
const QRegularExpression regexStartupNotifyFull(QStringLiteral("(^|\n)StartupNotify=[^\n]*(\n|$)"));
const QRegularExpression regexNoDisplayFull(QStringLiteral("(^|\n)NoDisplay=[^\n]*(\n|$)"));
const QRegularExpression regexTerminalFull(QStringLiteral("(^|\n)Terminal=[^\n]*(\n|$)"));
const QRegularExpression regexNotFilter(QStringLiteral("^(?!<Not>).*$"));
const QRegularExpression regexNameFull(QStringLiteral("(^|\n)Name=[^\n]*(\n|$)"));
}

MainWindow::MainWindow(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::MainWindow),
      add(new AddAppDialog)
{
    qDebug().noquote() << QCoreApplication::applicationName() << "version:" << VERSION;
    connect(QApplication::instance(), &QApplication::aboutToQuit, this, &MainWindow::saveSettings);
    ui->setupUi(this);
    setWindowFlags(Qt::Window); // for the close, min and max buttons
    setConnections();

    if (ui->pushSave->icon().isNull()) {
        ui->pushSave->setIcon(QIcon(":/icons/dialog-ok.svg"));
    }
    if (add->ui->pushSave->icon().isNull()) {
        add->ui->pushSave->setIcon(QIcon(":/icons/dialog-ok.svg"));
    }

    // Remove focus from Cancel button
    ui->pushCancel->setAutoDefault(false);
    ui->pushCancel->setDefault(false);

    all_local_desktop_files = listDesktopFiles(QLatin1String(""), localApplicationsPath());
    all_usr_desktop_files = listDesktopFiles(QLatin1String(""), systemApplicationsPath());

    resetInterface();
    loadMenuFiles();

    connect(ui->treeWidget, &QTreeWidget::itemSelectionChanged, this, &MainWindow::loadApps);
    connect(ui->lineEditSearch, &QLineEdit::textChanged, this, &MainWindow::filterTree);
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
    connect(ui->advancedEditor, &QTextEdit::textChanged, this, &MainWindow::onEditorTextChanged);
    connect(ui->pushAddApp, &QPushButton::clicked, this, &MainWindow::addAppMsgBox);
    connect(ui->lineEditName, &QLineEdit::textEdited, this, &MainWindow::setEnabled);
    connect(ui->lineEditCommand, &QLineEdit::textEdited, this, &MainWindow::setEnabled);
    connect(ui->lineEditComment, &QLineEdit::textEdited, this, &MainWindow::setEnabled);
    connect(add, &QDialog::accepted, this, &MainWindow::onCustomAppSaved);

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
    hashCategories.clear();
    hashExclude.clear();
    hashInclude.clear();
    ui->treeWidget->clear();

    const QString &homePath = QDir::homePath();
    QStringList menuItems;
    const QStringList &menuFiles = listMenuFiles();

    // process each menu_file
    for (const auto &fileName : menuFiles) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                auto line = in.readLine();
                QString name;
                if (line.contains(QLatin1String("<Name>"))) {
                    if (!in.atEnd()) {
                        line = in.readLine();
                        if (line.contains(QLatin1String("<Directory>"))) {
                            line = line.remove(QStringLiteral("<Directory>"))
                                       .remove(QStringLiteral("</Directory>"))
                                       .trimmed();
                            auto fName = homePath + "/.local/share/desktop-directories/" + line;
                            if (!QFileInfo::exists(fName)) { // use /usr if the file is not present in ~/.local
                                fName = "/usr/share/desktop-directories/" + line;
                            }
                            name = getCatName(fName); // get the Name= from .directory file
                            if (!name.isEmpty() && QLatin1String("Other") != name && QLatin1String("Wine") != name) {
                                menuItems << name;
                            }
                            // Find <Category> and <Filename> and add them in hashCategory and hashInclude
                            while (!(in.atEnd() || line.contains(QLatin1String("</Include>")))) {
                                line = in.readLine();
                                if (line.contains(QLatin1String("<Category>"))) {
                                    line = line.remove(QStringLiteral("<Category>"))
                                               .remove(QStringLiteral("</Category>"))
                                               .trimmed();
                                    if (!hashCategories.values(name).contains(line)) {
                                        hashCategories.insert(
                                            name, line); // each menu category displays a number of categories
                                    }
                                }
                                if (line.contains(QLatin1String("<Filename>"))) {
                                    line = line.remove(QStringLiteral("<Filename>"))
                                               .remove(QStringLiteral("</Filename>"))
                                               .trimmed();
                                    if (!hashInclude.values(name).contains(line)) {
                                        hashInclude.insert(name, line); // each menu category contains a number of files
                                    }
                                }
                            }
                             // find <Excludes> and add them in hashExclude
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
                                            if (!hashExclude.values(name).contains(line)) {
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
            }
            file.close();
        }
    }
    displayList(menuItems);
    populateAllCategories();
    filterTree(ui->lineEditSearch->text());
}

void MainWindow::populateAllCategories()
{
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
        auto *categoryItem = ui->treeWidget->topLevelItem(i);
        populateCategory(categoryItem);
    }
}

void MainWindow::populateCategory(QTreeWidgetItem *categoryItem)
{
    if (categoryItem == nullptr) {
        return;
    }

    categoryItem->takeChildren();

    const auto &categoryName = categoryItem->text(0);
    const QStringList categories = hashCategories.values(categoryName);
    const QStringList includes = hashInclude.values(categoryName);
    const QStringList excludes = hashExclude.values(categoryName);

    QStringList includesUsr;
    QStringList includesLocal;
    includesUsr.reserve(includes.size());
    includesLocal.reserve(includes.size());
    for (const auto &file : std::as_const(includes)) {
        includesUsr << systemApplicationsPath() + "/" + file;
        includesLocal << localApplicationsPath() + "/" + file;
    }

    QStringList searchPatterns;
    searchPatterns.reserve(categories.size());
    for (const auto &category : std::as_const(categories)) {
        const auto escapedCategory = QRegularExpression::escape(category);
        searchPatterns << ("Categories=.*" + escapedCategory);
    }
    const QString searchString = searchPatterns.join(QLatin1Char('|'));

    bool usrListError = false;
    bool localListError = false;
    auto usrDesktopFiles
        = listDesktopFiles(searchString, systemApplicationsPath(), &usrListError);
    auto localDesktopFiles
        = listDesktopFiles(searchString, localApplicationsPath(), &localListError);

    usrDesktopFiles.append(includesUsr);
    localDesktopFiles.append(includesLocal);

    for (const auto &baseName : std::as_const(excludes)) {
        usrDesktopFiles.removeAll(systemApplicationsPath() + "/" + baseName);
        localDesktopFiles.removeAll(localApplicationsPath() + "/" + baseName);
    }

    QSet<QString> localBaseNames;
    for (const auto &localName : std::as_const(all_local_desktop_files)) {
        QFileInfo fLocal(localName);
        localBaseNames.insert(fLocal.fileName());
    }

    for (const auto &localName : std::as_const(localDesktopFiles)) {
        QFileInfo fiLocal(localName);
        auto *app = addToTree(categoryItem, localName);
        if (app != nullptr && all_usr_desktop_files.contains(systemApplicationsPath() + "/" + fiLocal.fileName())) {
            app->setData(0, Qt::UserRole, "restore");
        }
    }

    for (const auto &file : std::as_const(usrDesktopFiles)) {
        QFileInfo fi(file);
        const auto baseName = fi.fileName();
        if (!localBaseNames.contains(baseName)) {
            addToTree(categoryItem, file);
        }
    }

    categoryItem->sortChildren(1, Qt::AscendingOrder);
}

void MainWindow::insertAppIntoCategories(const QString &filePath, const QStringList &categories)
{
    if (filePath.isEmpty() || categories.isEmpty()) {
        return;
    }

    const QFileInfo newFileInfo(filePath);
    const QString baseName = newFileInfo.fileName();

    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
        auto *categoryItem = ui->treeWidget->topLevelItem(i);
        const auto existingCategories = hashCategories.values(categoryItem->text(0));
        const bool belongs = std::any_of(categories.cbegin(), categories.cend(), [&](const QString &cat) {
            return existingCategories.contains(cat);
        });
        if (!belongs) {
            continue;
        }

        for (int j = 0; j < categoryItem->childCount(); ++j) {
            auto *child = categoryItem->child(j);
            QFileInfo childInfo(child->text(1));
            if (childInfo.fileName() == baseName) {
                delete categoryItem->takeChild(j);
                break;
            }
        }

        auto *child = addToTree(categoryItem, filePath);
        if (child != nullptr && QFileInfo::exists(systemApplicationsPath() + "/" + baseName)) {
            child->setData(0, Qt::UserRole, "restore");
        }
        categoryItem->sortChildren(1, Qt::AscendingOrder);
        categoryItem->setExpanded(true);
    }
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
QString MainWindow::getCatName(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    const QStringList uiLanguages = QLocale::system().uiLanguages();
    QStringList localeKeys;
    localeKeys.reserve(uiLanguages.size() * 2);
    for (const auto &lang : uiLanguages) {
        const auto normalized = QString(lang).replace(QLatin1Char('-'), QLatin1Char('_'));
        localeKeys << normalized;
        const int sepIndex = normalized.indexOf(QLatin1Char('_'));
        if (sepIndex > 0) {
            localeKeys << normalized.left(sepIndex);
        }
    }

    QString defaultName;
    QHash<QString, QString> localizedNames;
    bool inDesktopEntry = false;
    QTextStream in(&file);
    while (!in.atEnd()) {
        const auto line = in.readLine().trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inDesktopEntry = line == QLatin1String("[Desktop Entry]");
            continue;
        }
        if (!inDesktopEntry || !line.startsWith(QLatin1String("Name"))) {
            continue;
        }
        if (line.startsWith(QLatin1String("Name["))) {
            const int closeIdx = line.indexOf(QLatin1Char(']'));
            if (closeIdx > 5 && line.size() > closeIdx + 1 && line.at(closeIdx + 1) == QLatin1Char('=')) {
                const auto locale = line.mid(5, closeIdx - 5);
                const auto value = line.mid(closeIdx + 2).trimmed();
                localizedNames.insert(locale, value);
            }
        } else if (line.startsWith(QLatin1String("Name="))) {
            defaultName = line.mid(5).trimmed();
        }
    }

    for (const auto &key : std::as_const(localeKeys)) {
        if (localizedNames.contains(key)) {
            return localizedNames.value(key);
        }
    }

    if (!defaultName.isEmpty()) {
        return defaultName;
    }

    if (!localizedNames.isEmpty()) {
        return localizedNames.constBegin().value();
    }

    return QString();
}

// return a list of .menu files
QStringList MainWindow::listMenuFiles()
{
    const auto homePath = QDir::homePath();
    QStringList menuFiles(QStringLiteral("/etc/xdg/menus/xfce-applications.menu"));

    // add menu files from user directory
    const QString userMenuPath = homePath + "/.config/menus";
    QDirIterator it(userMenuPath, QStringList() << "*.menu", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        menuFiles << it.next();
    }
    return menuFiles;
}

QString MainWindow::localApplicationsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/applications";
}

QString MainWindow::systemApplicationsPath()
{
    return QStringLiteral("/usr/share/applications");
}

// Display sorted list of menu items in the treeWidget
void MainWindow::displayList(QStringList menuItems)
{
    QTreeWidgetItem *topLevelItem = nullptr;
    ui->treeWidget->setHeaderLabel(QLatin1String(""));
    ui->treeWidget->setSortingEnabled(true);
    menuItems.removeDuplicates();
    for (const auto &item : menuItems) {
        topLevelItem = new QTreeWidgetItem(ui->treeWidget, QStringList(item));
        // topLevelItem look
        QFont font;
        font.setBold(true);
        topLevelItem->setForeground(0, QBrush(Qt::darkGreen));
        topLevelItem->setFont(0, font);
    }
    ui->treeWidget->sortItems(0, Qt::AscendingOrder);
}

// load the applications in the selected category
void MainWindow::loadApps()
{
    auto *currentItem = ui->treeWidget->currentItem();
    if (currentItem == nullptr) {
        return;
    }

    if (ui->pushSave->isEnabled() && !save()) {
        // Restore previous selection if save was cancelled
        auto *itemToRestore = current_item;
        if (itemToRestore != nullptr) {
            ui->treeWidget->setCurrentItem(itemToRestore);
        }
        return;
    }

    // Re-fetch current item after potential save operation
    currentItem = ui->treeWidget->currentItem();
    if (currentItem == nullptr) {
        current_item = nullptr;
        return;
    }

    if (currentItem->parent() == nullptr) {
        current_item = nullptr;
        resetInterface();
        currentItem->setExpanded(true);
        return;
    }

    loadItem(currentItem, 0);
}

void MainWindow::filterTree(const QString &query)
{
    const QString trimmed = query.trimmed();
    const bool hasQuery = !trimmed.isEmpty();

    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
        auto *categoryItem = ui->treeWidget->topLevelItem(i);
        const bool categoryMatch = categoryItem->text(0).contains(trimmed, Qt::CaseInsensitive);
        bool anyChildVisible = false;

        for (int j = 0; j < categoryItem->childCount(); ++j) {
            auto *child = categoryItem->child(j);
            const auto execCommand = child->data(0, ExecRole).toString();
            const bool match = categoryMatch || child->text(0).contains(trimmed, Qt::CaseInsensitive)
                               || child->text(1).contains(trimmed, Qt::CaseInsensitive)
                               || execCommand.contains(trimmed, Qt::CaseInsensitive);
            child->setHidden(hasQuery && !match);
            anyChildVisible = anyChildVisible || !child->isHidden();
        }

        const bool hideCategory = hasQuery && !categoryMatch && !anyChildVisible;
        categoryItem->setHidden(hideCategory);
        // Only change expanded state when there's a search query
        // Otherwise preserve the current expanded state
        if (hasQuery) {
            categoryItem->setExpanded(!hideCategory && anyChildVisible);
        }
    }

    auto *current = ui->treeWidget->currentItem();
    if (current != nullptr && current->isHidden()) {
        if (ui->pushSave->isEnabled() && !save()) {
            return;
        }
        ui->treeWidget->clearSelection();
        resetInterface();
    }
}

void MainWindow::updateRestoreButtonState(const QString &fileName)
{
    const QString baseName = QFileInfo(fileName).fileName();
    const QString localPrefix = localApplicationsPath() + "/";
    const bool isLocal = fileName.startsWith(localPrefix);
    const bool hasUsrCopy = QFile::exists(systemApplicationsPath() + "/" + baseName);

    if (isLocal && hasUsrCopy) {
        ui->pushRestoreApp->setText(tr("Restore original item"));
        ui->pushRestoreApp->setEnabled(true);
    } else if (isLocal) {
        ui->pushRestoreApp->setText(tr("Delete"));
        ui->pushRestoreApp->setEnabled(true);
    } else {
        ui->pushRestoreApp->setText(tr("Restore original item"));
        ui->pushRestoreApp->setEnabled(false);
    }
}

// add .desktop item to treeWidget
QTreeWidgetItem *MainWindow::addToTree(QTreeWidgetItem *parent, const QString &fileName)
{
    if (!QFileInfo::exists(fileName)) {
        return nullptr;
    }

    // Read the file once to extract both Name and NoDisplay values
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return nullptr;
    }

    QString appName;
    QString execCommand;
    bool isHidden = false;
    QTextStream in(&file);

    // Read file line by line, looking for Name=, Exec= and NoDisplay=
    // We scan the whole [Desktop Entry] section to ensure we find NoDisplay if it exists
    bool inDesktopEntry = false;
    while (!in.atEnd()) {
        const auto line = in.readLine();
        if (line.startsWith(QLatin1String("[Desktop Entry]"))) {
            inDesktopEntry = true;
        } else if (line.startsWith(QLatin1Char('[')) && inDesktopEntry) {
            // Reached another section, stop reading
            break;
        }
        if (line.startsWith(QLatin1String("Name=")) && appName.isEmpty()) {
            appName = line.section(QStringLiteral("="), 1).trimmed();
        } else if (inDesktopEntry && line.startsWith(QLatin1String("NoDisplay="))) {
            const auto value = line.section(QStringLiteral("="), 1).trimmed();
            isHidden = (value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0);
        } else if (line.startsWith(QLatin1String("Exec=")) && execCommand.isEmpty()) {
            execCommand = line.section(QStringLiteral("="), 1).trimmed();
        }
        // Early exit if we found both required fields (Name and Exec)
        // Continue reading to find NoDisplay if it exists in the Desktop Entry section
        if (inDesktopEntry && !appName.isEmpty() && !execCommand.isEmpty()) {
            // Continue reading until end of [Desktop Entry] section to catch NoDisplay
            bool foundNextSection = false;
            while (!in.atEnd()) {
                const auto nextLine = in.readLine();
                if (nextLine.startsWith(QLatin1Char('['))) {
                    foundNextSection = true;
                    break;
                }
                if (nextLine.startsWith(QLatin1String("NoDisplay="))) {
                    const auto value = nextLine.section(QStringLiteral("="), 1).trimmed();
                    isHidden = (value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0);
                    break;
                }
            }
            if (foundNextSection || in.atEnd()) {
                break;
            }
        }
    }
    file.close();

    if (appName.isEmpty()) {
        return nullptr;
    }

    if (parent == nullptr) {
        return nullptr;
    }

    auto *childItem = new QTreeWidgetItem(parent);
    if (isHidden) {
        childItem->setForeground(0, QBrush(Qt::gray));
    }
    childItem->setText(0, appName);
    childItem->setText(1, fileName);
    childItem->setData(0, ExecRole, execCommand);
    return childItem;
}

QStringList MainWindow::listDesktopFiles(const QString &searchString, const QString &location, bool *hadError)
{
    if (hadError != nullptr) {
        *hadError = false;
    }

    QStringList result;

    const QFileInfo locationInfo(location);
    if (!locationInfo.exists() || !locationInfo.isDir()) {
        if (hadError != nullptr) {
            *hadError = true;
        }
        return result;
    }

    if (searchString.isEmpty()) {
        // No search string, find all .desktop files recursively
        QDirIterator it(location, QStringList() << "*.desktop", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            result.append(it.next());
        }
    } else {
        // Search for files containing the search pattern
        const QRegularExpression regex(searchString);
        if (!regex.isValid()) {
            if (hadError != nullptr) {
                *hadError = true;
            }
            return result;
        }

        QDirIterator it(location, QStringList() << "*.desktop", QDir::Files, QDirIterator::Subdirectories);

        while (it.hasNext()) {
            const QString filePath = it.next();
            QFile file(filePath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                while (!in.atEnd()) {
                    const QString line = in.readLine();
                    if (regex.match(line).hasMatch()) {
                        result.append(filePath);
                        break;
                    }
                }
            }
        }
    }

    return result;
}

void MainWindow::loadItem(QTreeWidgetItem *item, int /*unused*/)
{
    if (item == nullptr) {
        return;
    }

// Qt 6: pixmap() returns by value, use isNull() to check if empty
    // execute if not topLevel item is selected
    if (item->parent() != nullptr) {
        if (ui->pushSave->isEnabled() && !save()) {
            return;
        }
        const auto file_name = item->text(1).trimmed();
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
                line.remove(regexTrailingSemicolon);
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
                if (ui->lineEditCommand->text()
                        .isEmpty()) { // some .desktop files have multiple Exec= display first one
                    ui->lineEditCommand->setText(line);
                }
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
                if (!line.isEmpty() && ui->labelIcon->pixmap(Qt::ReturnByValue).isNull()) { // some .desktop files have multiple Icon= display first
                    ui->labelIcon->setPixmap(findIcon(line, size).scaled(size));
                }
            }
        }
        file.close();
        {
            QSignalBlocker editorBlocker(ui->advancedEditor);
            QSignalBlocker docBlocker(ui->advancedEditor->document());
            ui->advancedEditor->setText(content);
            ui->advancedEditor->document()->setModified(false);
            ui->advancedEditor->document()->clearUndoRedoStacks(QTextDocument::UndoAndRedoStacks);
        }
        advancedEditorBaseline = content;
        QTreeWidgetItem *currentItem = ui->treeWidget->currentItem();
        updateRestoreButtonState(file_name);
        current_item = currentItem; // remember the current_item in case user selects another item before saving
    }
}

// select command to be used
void MainWindow::selectCommand()
{
    const auto selected = QFileDialog::getOpenFileName(this, tr("Select executable file"), QStringLiteral("/usr/bin"));
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
    {
        QSignalBlocker editorBlocker(ui->advancedEditor);
        QSignalBlocker docBlocker(ui->advancedEditor->document());
        ui->advancedEditor->clear();
        ui->advancedEditor->document()->setModified(false);
        ui->advancedEditor->document()->clearUndoRedoStacks(QTextDocument::UndoAndRedoStacks);
    }
    advancedEditorBaseline.clear();
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
    ui->pushRestoreApp->setText(tr("Restore original item"));
    ui->pushSave->setDisabled(true);
    ui->labelIcon->setPixmap(QPixmap());
}

void MainWindow::saveSettings()
{
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
}

void MainWindow::onEditorTextChanged()
{
    const auto currentText = ui->advancedEditor->toPlainText();
    if (currentText == advancedEditorBaseline) {
        ui->pushSave->setEnabled(false);
        QSignalBlocker blocker(ui->advancedEditor->document());
        ui->advancedEditor->document()->setModified(false);
        return;
    }
    ui->pushSave->setEnabled(true);
}

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
        QStringList selectedList = dialog.selectedFiles();
        if (!selectedList.isEmpty()) {
            selected = selectedList.at(0);
        }
    }
    if (!selected.isEmpty()) {
        QString text = ui->advancedEditor->toPlainText();
        if (ui->lineEditCommand->isEnabled()) { // started from editor
            ui->pushSave->setEnabled(true);
            if (text.contains(regexIconLine)) {
                text.replace(regexIconFull, "\nIcon=" + selected + "\n");
            } else {
                text.append("\nIcon=" + selected + "\n");
            }
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
        const auto newName = ui->lineEditName->text();
        ui->pushSave->setEnabled(true);
        if (newName.isEmpty()) {
            return;
        }
        QString text = ui->advancedEditor->toPlainText();
        QRegularExpressionMatch regexMatch = regexNameFull.match(text);
        int index = regexMatch.capturedStart();
        int length = regexMatch.capturedLength();

        if (index != -1) {
            text.replace(index, length, "\nName=" + newName + "\n"); // replace only first match
        } else {
            // Name= line doesn't exist, add it after [Desktop Entry]
            text = text.trimmed();
            text.append("\nName=" + newName + "\n");
        }
        ui->advancedEditor->setText(text);
    } else { // if running command from add-custom-app window
        if (!add->ui->lineEditName->text().isEmpty() && !add->ui->lineEditCommand->text().isEmpty()
            && add->ui->listWidgetCategories->count() != 0) {
            add->ui->pushSave->setEnabled(true);
        } else {
            add->ui->pushSave->setEnabled(false);
        }
    }
}

void MainWindow::changeCommand()
{
    if (ui->lineEditCommand->isEnabled()) { // started from editor
        const auto newCommand = ui->lineEditCommand->text();
        ui->pushSave->setEnabled(true);
        if (newCommand.isEmpty()) {
            return;
        }
        QString text = ui->advancedEditor->toPlainText();
        QRegularExpressionMatch regexMatch = regexExecFull.match(text);
        int index = regexMatch.capturedStart();
        int length = regexMatch.capturedLength();
        if (index != -1) {
            text.replace(index, length, "\nExec=" + newCommand + "\n"); // replace only first match
        } else {
            // Exec= line doesn't exist, add it
            text = text.trimmed();
            text.append("\nExec=" + newCommand + "\n");
        }
        ui->advancedEditor->setText(text);
    } else { // if running command from add-custom-app window
        const auto new_command = add->ui->lineEditCommand->text();
        if (!new_command.isEmpty() && !add->ui->lineEditName->text().isEmpty()
            && add->ui->listWidgetCategories->count() != 0) {
            add->ui->pushSave->setEnabled(true);
        } else {
            add->ui->pushSave->setEnabled(false);
        }
    }
}

void MainWindow::changeComment()
{
    if (ui->lineEditCommand->isEnabled()) { // started from editor
        const auto newComment = ui->lineEditComment->text();
        ui->pushSave->setEnabled(true);
        QString text = ui->advancedEditor->toPlainText();
        if (!newComment.isEmpty()) {
            if (text.contains(QLatin1String("Comment="))) {
                text.replace(regexCommentFull, "\nComment=" + newComment + "\n");
            } else {
                text = text.trimmed();
                text.append("\nComment=" + newComment + "\n");
            }
        } else {
            text.remove(regexCommentFull);
        }
        ui->advancedEditor->setText(text);
    }
}

void MainWindow::enableDelete()
{
    ui->pushDelete->setEnabled(true);
}

void MainWindow::delCategory()
{
    int row = 0;
    if (ui->lineEditCommand->isEnabled()) { // started from editor
        ui->pushSave->setEnabled(true);
        row = ui->listWidgetEditCategories->currentRow();
        QScopedPointer<QListWidgetItem> item(ui->listWidgetEditCategories->takeItem(row));
        if (item.isNull()) {
            return;
        }
        QString text = ui->advancedEditor->toPlainText();
        const auto match = regexCategoriesFull.match(text);
        if (match.hasMatch()) {
            const int categoriesPos = text.indexOf(QLatin1String("Categories="), match.capturedStart());
            if (categoriesPos != -1) {
                const int lineEnd = text.indexOf(QLatin1Char('\n'), categoriesPos);
                const int lineLength = (lineEnd == -1) ? (text.length() - categoriesPos) : (lineEnd - categoriesPos);
                const QString line = text.mid(categoriesPos, lineLength);
                const int equalsPos = line.indexOf(QLatin1Char('='));
                QStringList categories;
                if (equalsPos != -1) {
                    categories = line.mid(equalsPos + 1).split(QLatin1Char(';'), Qt::SkipEmptyParts);
                }
                categories.removeAll(item->text());
                QString newLine = QStringLiteral("Categories=");
                if (categories.isEmpty()) {
                    newLine.append(QLatin1Char(';'));
                } else {
                    newLine.append(categories.join(QLatin1Char(';')));
                    newLine.append(QLatin1Char(';'));
                }
                text.replace(categoriesPos, lineLength, newLine);
                ui->advancedEditor->setText(text);
            }
        }
        if (ui->listWidgetEditCategories->count() == 0) {
            ui->pushDelete->setDisabled(true);
            ui->pushSave->setDisabled(true);
        }
    } else { // if running command from add-custom-app window
        row = add->ui->listWidgetCategories->currentRow();
        QScopedPointer<QListWidgetItem> item(add->ui->listWidgetCategories->takeItem(row));
        if (item.isNull()) {
            return;
        }
        Q_UNUSED(item); // item automatically deleted when going out of scope
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
        text.replace(regexStartupNotifyFull, "\nStartupNotify=" + str + "\n");
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
        text.replace(regexNoDisplayFull, "\nNoDisplay=" + str + "\n");
    } else {
        QString newText;
        for (const auto &line : text.split(QStringLiteral("\n"))) {
            newText.append(line + "\n");
            if (line.startsWith(QLatin1String("Exec="))) {
                newText.append("NoDisplay=" + str + "\n");
            }
        }
        text = newText;
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
        text.replace(regexTerminalFull, "\nTerminal=" + str + "\n");
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
    categories = categories.filter(regexNotFilter);
    categories.sort();
    return categories;
}

// display add category message box
void MainWindow::addCategoryMsgBox()
{
    const QStringList &categories = listCategories();

    auto *window = new QWidget(add, Qt::Dialog);
    window->setAttribute(Qt::WA_DeleteOnClose); // Auto-delete when closed
    window->setWindowTitle(tr("Choose category"));
    const int height = 80;
    const int width = 250;
    window->resize(width, height);

    comboBox = new QComboBox(window);
    auto *buttonBox = new QDialogButtonBox(window);

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

    auto *layout = new QFormLayout(window);
    layout->addRow(comboBox);
    layout->addRow(buttonBox);

    window->setLayout(layout);
    window->show();
}

void MainWindow::centerWindow()
{
    const auto screens = QApplication::screens();
    if (screens.isEmpty()) {
        return;
    }
    QRect screenGeometry = screens.constFirst()->geometry();
    int x = (screenGeometry.width() - this->width()) / 2;
    int y = (screenGeometry.height() - this->height()) / 2;
    this->move(x, y);
}

// add selected categorory to the .desktop file
void MainWindow::addCategory()
{
    const auto str = comboBox->currentText();
    QString text = ui->advancedEditor->toPlainText();
    int index = text.indexOf(regexCategoriesFull);
    if (index != -1) {
        index = text.indexOf(regexNewlineOrEnd, index + 1); // end of Categories line
    } else {
        // No Categories line found; append a fresh one at the end
        text = text.trimmed();
        text.append("\nCategories=;\n");
        index = text.indexOf(regexCategoriesFull);
        index = text.indexOf(regexNewlineOrEnd, index + 1);
    }
    if (ui->lineEditCommand->isEnabled()) { // started from editor
        if (ui->listWidgetEditCategories->findItems(str, Qt::MatchFixedString).isEmpty()) {
            ui->pushSave->setEnabled(true);
            text.insert(index, str + ";");
            ui->listWidgetEditCategories->addItem(str);
            ui->advancedEditor->setText(text);
            ui->pushDelete->setEnabled(true);
            if (ui->listWidgetEditCategories->count() == 0) {
                ui->pushSave->setDisabled(true);
            }
        }
    } else { // if running command from add-custom-app window
        if (add->ui->listWidgetCategories->findItems(str, Qt::MatchFixedString).isEmpty()) {
            text.insert(index, str + ";");
            add->ui->listWidgetCategories->addItem(str);
            add->ui->pushDelete->setEnabled(true);
            if (!add->ui->lineEditName->text().isEmpty() && !add->ui->lineEditCommand->text().isEmpty()
                && add->ui->listWidgetCategories->count() != 0) {
                add->ui->pushSave->setEnabled(true);
            } else {
                add->ui->pushSave->setEnabled(false);
            }
        }
    }
}

// display add application message box
void MainWindow::addAppMsgBox()
{
    if (ui->pushSave->isEnabled() && !save()) {
        return;
    }
    add->lastSavedPath.clear();
    add->lastSavedCategories.clear();
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

void MainWindow::onCustomAppSaved()
{
    const QString newPath = add->lastSavedPath;
    if (newPath.isEmpty()) {
        return;
    }

    if (!all_local_desktop_files.contains(newPath)) {
        all_local_desktop_files << newPath;
    }
    insertAppIntoCategories(newPath, add->lastSavedCategories);
    filterTree(ui->lineEditSearch->text());
    findReloadItem(QFileInfo(newPath).fileName());
}

bool MainWindow::validateExecutable(const QString &execCommand)
{
    if (execCommand.isEmpty()) {
        return true; // Empty is allowed (will be caught elsewhere if required)
    }

    // Extract the executable path (first part before any arguments)
    auto executable = execCommand.split(QLatin1Char(' ')).first();

    // Remove quotes if present
    if (executable.startsWith(QLatin1Char('"')) && executable.endsWith(QLatin1Char('"'))) {
        executable = executable.mid(1, executable.length() - 2);
    }
    if (executable.startsWith(QLatin1Char('\'')) && executable.endsWith(QLatin1Char('\''))) {
        executable = executable.mid(1, executable.length() - 2);
    }

    // Check if the executable exists
    bool execExists = false;
    if (executable.startsWith(QLatin1Char('/'))) {
        // Absolute path - check directly
        execExists = QFile::exists(executable);
    } else {
        // Relative path or command name - check in PATH
        execExists = !QStandardPaths::findExecutable(executable).isEmpty();
    }

    if (!execExists) {
        auto answer = QMessageBox::question(
            this, tr("Warning"),
            tr("The executable '%1' does not exist or is not in PATH.\nDo you want to continue anyway?")
                .arg(executable),
            QMessageBox::Yes | QMessageBox::No);
        return answer == QMessageBox::Yes;
    }

    return true;
}

void MainWindow::pushSave_clicked()
{
    if (current_item == nullptr) {
        qWarning() << "pushSave_clicked: current_item is null";
        return;
    }

    const QString editorText = ui->advancedEditor->toPlainText();
    QString execCommand = ui->lineEditCommand->text().trimmed();
    const QRegularExpressionMatch execMatch = regexExecFull.match(editorText);
    if (execMatch.hasMatch()) {
        const QString execLine = execMatch.captured();
        const int idx = execLine.indexOf(QLatin1String("Exec="));
        if (idx != -1) {
            const auto extracted = execLine.mid(idx + 5).trimmed();
            if (!extracted.isEmpty()) {
                execCommand = extracted;
                ui->lineEditCommand->setText(execCommand);
            }
        }
    }

    // Validate the Exec command before saving
    if (!validateExecutable(execCommand)) {
        return;
    }

    const auto file_name = current_item->text(1);
    const QFileInfo fi(file_name);
    const auto base_name = fi.fileName();
    const auto applicationsDir = localApplicationsPath();
    if (!QFileInfo::exists(applicationsDir) && !QDir().mkpath(applicationsDir)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not create the applications directory"));
        return;
    }
    const auto out_name = applicationsDir + "/" + base_name;
    QFile out(out_name);
    if (!out.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not save the file"));
        return;
    }
    if (!all_local_desktop_files.contains(out_name)) {
        all_local_desktop_files << out_name;
    }
    out.write(editorText.toUtf8());
    out.flush();
    out.close();
    {
        QSignalBlocker editorBlocker(ui->advancedEditor);
        QSignalBlocker docBlocker(ui->advancedEditor->document());
        ui->advancedEditor->document()->setModified(false);
        ui->advancedEditor->document()->clearUndoRedoStacks(QTextDocument::UndoAndRedoStacks);
    }
    advancedEditorBaseline = editorText;
    {
        QSignalBlocker treeBlocker(ui->treeWidget);
        populateAllCategories();
    }
    restartPanel();
    ui->pushSave->setDisabled(true);
    findReloadItem(base_name);
}

void MainWindow::restartPanel()
{
    if (QProcess::execute(QStringLiteral("pgrep"), {QStringLiteral("xfce4-panel")}) == 0) {
        QProcess::execute(QStringLiteral("xfce4-panel"), {QStringLiteral("--restart")});
    }
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
        changelog->setAttribute(Qt::WA_DeleteOnClose);
        const int width = 500;
        const int height = 600;
        changelog->resize(width, height);

        auto *text = new QTextEdit;
        text->setReadOnly(true);
        QProcess process;
        process.start(QStringLiteral("zless"),
                      QStringList {"/usr/share/doc/" + QFileInfo(QCoreApplication::applicationFilePath()).fileName()
                                   + "/changelog.gz"});
        if (!process.waitForFinished(3000)) {
            process.kill();
            process.waitForFinished(1000);
            return;
        }
        if (process.exitCode() != 0) {
            return;
        }
        text->setText(process.readAllStandardOutput());

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

void MainWindow::setEnabled(const QString & /*unused*/)
{
    ui->pushSave->setEnabled(true);
}

void MainWindow::pushHelp_clicked()
{
    const QLocale locale;
    const auto lang = locale.bcp47Name();

    auto url = QStringLiteral("/usr/share/doc/mx-menu-editor/mx-menu-editor.html");

    if (lang.startsWith(QLatin1String("fr"))) {
        url = QStringLiteral("https://mxlinux.org/wiki/help-files/help-mx-editeur-de-menu");
    }

    QProcess::execute(QStringLiteral("xdg-open"), {url});
}

// Cancel button clicked
void MainWindow::pushCancel_clicked()
{
    if (ui->pushSave->isEnabled() && !save()) {
        return;
    }
    QApplication::quit();
}

// ask whether to save edits or not
bool MainWindow::save()
{
    if (!ui->pushSave->isEnabled()) {
        return true; // No unsaved changes, continue
    }

    const auto answer = QMessageBox::question(this, tr("Save changes?"), tr("Do you want to save your edits?"),
                                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Save) {
        pushSave_clicked();
        return true; // Saved, continue
    }
    if (answer == QMessageBox::Discard) {
        ui->pushSave->setDisabled(true);
        return true; // Discarded, continue
    }
    // Cancel was clicked
    return false; // Don't continue, stay on current item
}

// delete .local file and reload files
void MainWindow::pushRestoreApp_clicked()
{
    if (current_item == nullptr) {
        return;
    }
    const auto file_name = current_item->text(1);
    const QFileInfo fi(file_name);
    const auto base_name = fi.fileName();
    const auto applicationsDir = localApplicationsPath();
    const auto localPath = applicationsDir + "/" + base_name;
    QFile file(localPath);
    if (file.exists()) {
        file.remove();
    }
    all_local_desktop_files = listDesktopFiles(QLatin1String(""), localApplicationsPath());
    ui->pushRestoreApp->setDisabled(true);
    {
        QSignalBlocker treeBlocker(ui->treeWidget);
        populateAllCategories();
    }
    filterTree(ui->lineEditSearch->text());
    findReloadItem(base_name);
}

// find and reload item
void MainWindow::findReloadItem(const QString &baseName)
{
    const auto localPath = localApplicationsPath() + "/" + baseName;
    const auto usrPath = systemApplicationsPath() + "/" + baseName;

    QTreeWidgetItemIterator it(ui->treeWidget);
    while ((*it) != nullptr) {
        auto *item = *it;
        QFileInfo fi(item->text(1));
        if ((fi.fileName() == baseName)) {
            if (QFileInfo::exists(localPath)) {
                item->setText(1, localPath);
                item->setData(0, Qt::UserRole, "restore");
                ui->treeWidget->setCurrentItem(item);
                current_item = item;
                updateRestoreButtonState(item->text(1));
                // Ensure parent category is expanded so the item is visible
                auto *parent = item->parent();
                if (parent != nullptr) {
                    parent->setExpanded(true);
                }
            } else if (QFileInfo::exists(usrPath)) {
                item->setText(1, usrPath);
                item->setData(0, Qt::UserRole, QVariant());
                ui->treeWidget->setCurrentItem(item);
                current_item = item;
                updateRestoreButtonState(item->text(1));
                // Ensure parent category is expanded so the item is visible
                auto *parent = item->parent();
                if (parent != nullptr) {
                    parent->setExpanded(true);
                }
            } else {
                // Item doesn't exist anywhere, delete it from tree
                auto *parent = item->parent();
                if (parent != nullptr) {
                    // Select parent before deleting the item
                    ui->treeWidget->setCurrentItem(parent);
                    const int idx = parent->indexOfChild(item);
                    delete parent->takeChild(idx);
                    current_item = nullptr;
                    resetInterface();
                }
            }
            filterTree(ui->lineEditSearch->text());
            return;
        }
        ++it;
    }
    filterTree(ui->lineEditSearch->text());
}

namespace
{
struct IconKey {
    QString name;
    QSize size;

    bool operator==(const IconKey &other) const
    {
        return name == other.name && size == other.size;
    }
};

inline uint qHash(const IconKey &key, uint seed = 0) noexcept
{
    uint h = ::qHash(key.name, seed);
    h = ::qHash(key.size.width(), h);
    h = ::qHash(key.size.height(), h);
    return h;
}
} // namespace

QPixmap MainWindow::findIcon(const QString &iconName, const QSize &size)
{
    static QHash<IconKey, QIcon> iconCache;
    static const QRegularExpression re(QStringLiteral("\\.(png|svg|xpm)$"));
    static const QStringList extensions {QStringLiteral(".png"), QStringLiteral(".svg"), QStringLiteral(".xpm")};

    const auto makePixmap = [size](const QIcon &icon) {
        return icon.isNull() ? QPixmap() : icon.pixmap(size.isValid() ? size : QSize());
    };

    if (iconName.isEmpty()) {
        return QPixmap();
    }

    const IconKey key {iconName, size};
    if (iconCache.contains(key)) {
        return makePixmap(iconCache.value(key));
    }

    // Absolute path handling
    if (QFileInfo::exists(iconName) && QFileInfo(iconName).isAbsolute()) {
        QIcon icon(iconName);
        iconCache.insert(key, icon);
        return makePixmap(icon);
    }

    QString nameNoExt = iconName;
    nameNoExt.remove(re);

    // Themed icon
    QIcon themedIcon = QIcon::fromTheme(nameNoExt);
    if (!themedIcon.isNull()) {
        iconCache.insert(key, themedIcon);
        return makePixmap(themedIcon);
    }

    // Fallback: search common icon-theme locations even when the current theme doesn't provide the icon
    // (e.g. icons exist under /usr/share/icons/<theme>/<size>/actions but the running theme is different).
    const auto findFromAnyTheme = [&](const QString &requestedName) -> QIcon {
        QString baseName = QFileInfo(requestedName).fileName();
        baseName.remove(re);
        static const QList<int> commonSizes {16, 22, 24, 32, 48, 64, 128, 256};
        QStringList sizeDirs;
        if (size.isValid() && size.width() > 0 && size.height() > 0 && size.width() == size.height()) {
            const int px = size.width();
            sizeDirs << QStringLiteral("%1x%1").arg(px);
        }
        for (int px : commonSizes) {
            const QString dir = QStringLiteral("%1x%1").arg(px);
            if (!sizeDirs.contains(dir)) {
                sizeDirs << dir;
            }
        }

        QStringList relDirs;
        relDirs << QStringLiteral("scalable/actions") << QStringLiteral("scalable/apps");
        for (const QString &sd : sizeDirs) {
            relDirs << (sd + QStringLiteral("/actions")) << (sd + QStringLiteral("/apps"));
        }
        relDirs << QStringLiteral("48x48/legacy") << QStringLiteral("32x32/legacy") << QStringLiteral("16x16/legacy");

        const QStringList themeRoots = QIcon::themeSearchPaths();
        const auto tryFile = [&](const QString &filePath) -> QIcon {
            if (!QFile::exists(filePath)) {
                return QIcon();
            }
            QIcon icon(filePath);
            return icon.isNull() ? QIcon() : icon;
        };

        // Try /usr/share/pixmaps first (common for app icons)
        for (const QString &ext : extensions) {
            if (QIcon icon = tryFile(QDir(QStringLiteral("/usr/share/pixmaps")).filePath(baseName + ext));
                !icon.isNull()) {
                return icon;
            }
        }

        // Then scan installed themes under the standard icon roots.
        for (const QString &root : themeRoots) {
            const QDir rootDir(root);
            if (!rootDir.exists()) {
                continue;
            }

            const QStringList themes = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &theme : themes) {
                const QString themeBase = rootDir.filePath(theme);
                for (const QString &relDir : relDirs) {
                    const QString dirPath = QDir(themeBase).filePath(relDir);
                    for (const QString &ext : extensions) {
                        if (QIcon icon = tryFile(QDir(dirPath).filePath(baseName + ext)); !icon.isNull()) {
                            return icon;
                        }
                    }
                }
            }
        }
        return QIcon();
    };

    QIcon icon = findFromAnyTheme(iconName);

    if (!icon.isNull()) {
        iconCache.insert(key, icon);
        return makePixmap(icon);
    }

    return QPixmap();
}
