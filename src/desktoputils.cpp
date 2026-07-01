/**********************************************************************
 *  desktoputils.cpp
 **********************************************************************
 * Copyright (C) 2026 MX Authors
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
#include "desktoputils.h"

#include <QFile>
#include <QLocale>
#include <QStandardPaths>

QString DesktopUtils::setEntryValue(QString text, const QRegularExpression &fullKeyRegex, const QString &key,
                                     const QString &value)
{
    const QRegularExpressionMatch match = fullKeyRegex.match(text);
    const int index = match.capturedStart();
    const int length = match.capturedLength();
    if (index != -1) {
        text.replace(index, length, "\n" + key + "=" + value + "\n");
    } else {
        text = text.trimmed();
        text.append("\n" + key + "=" + value + "\n");
    }
    return text;
}

QString DesktopUtils::pickLocalizedNameForKeys(const QString &defaultName,
                                                const QHash<QString, QString> &localizedNames,
                                                const QStringList &localeKeys)
{
    for (const auto &key : localeKeys) {
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

QStringList DesktopUtils::systemLocaleNameKeys()
{
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    QStringList result;
    result.reserve(uiLanguages.size() * 2);
    for (const auto &lang : uiLanguages) {
        const auto normalized = QString(lang).replace(QLatin1Char('-'), QLatin1Char('_'));
        result << normalized;
        const int sepIndex = normalized.indexOf(QLatin1Char('_'));
        if (sepIndex > 0) {
            result << normalized.left(sepIndex);
        }
    }
    return result;
}

QString DesktopUtils::pickLocalizedName(const QString &defaultName, const QHash<QString, QString> &localizedNames)
{
    // Cached since the system locale doesn't change mid-run and this is consulted
    // once per desktop file.
    static const QStringList keys = systemLocaleNameKeys();
    return pickLocalizedNameForKeys(defaultName, localizedNames, keys);
}

QString DesktopUtils::categoryMatchPattern(const QString &category)
{
    // Match the category as a whole semicolon-delimited token, not a substring of
    // another category.
    return QStringLiteral("Categories=(?:[^;]*;)*") + QRegularExpression::escape(category) + QStringLiteral("(?:;|$)");
}

QString DesktopUtils::parseCommandExecutable(const QString &command)
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

bool DesktopUtils::checkExecutableExists(const QString &executable)
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

bool DesktopUtils::containsInvalidDesktopChars(const QString &text)
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

QString DesktopUtils::sanitizeFileName(const QString &name)
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
