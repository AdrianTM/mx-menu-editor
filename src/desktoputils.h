/**********************************************************************
 *  desktoputils.h
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

#ifndef DESKTOPUTILS_H
#define DESKTOPUTILS_H

#include <QHash>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

// Pure, GUI-free helpers for working with .desktop entry text and metadata.
// Kept free of QWidget/QObject so they can be exercised directly by CTest.
namespace DesktopUtils
{
// Sets a single-line "Key=Value" entry in desktop-entry text, replacing the first
// existing line for that key or appending a freshly-formatted one if absent.
[[nodiscard]] QString setEntryValue(QString text, const QRegularExpression &fullKeyRegex, const QString &key,
                                     const QString &value);

// Picks the best display name given a set of candidate locale keys, most preferred
// first: a localized "Name[locale]=" matching one of the keys, falling back to the
// plain "Name=", falling back to any localized name at all if even that is missing.
[[nodiscard]] QString pickLocalizedNameForKeys(const QString &defaultName,
                                                const QHash<QString, QString> &localizedNames,
                                                const QStringList &localeKeys);

// Candidate "Name[locale]=" keys for the current system locale, most preferred first
// (e.g. "pt_BR" before "pt").
[[nodiscard]] QStringList systemLocaleNameKeys();

// Convenience wrapper: pickLocalizedNameForKeys() using the current system locale.
[[nodiscard]] QString pickLocalizedName(const QString &defaultName, const QHash<QString, QString> &localizedNames);

// Regex pattern matching a desktop category as a whole semicolon-delimited token in a
// "Categories=" line, not as a substring of another category.
[[nodiscard]] QString categoryMatchPattern(const QString &category);

// Extracts the executable token (first word, respecting quotes/escapes) from an Exec=
// command line.
[[nodiscard]] QString parseCommandExecutable(const QString &command);

// True if the executable can be found, either as an absolute path or in PATH.
[[nodiscard]] bool checkExecutableExists(const QString &executable);

// True if text contains a newline or other control character (tab excepted) that would
// corrupt the single-line Key=Value format of a .desktop entry.
[[nodiscard]] bool containsInvalidDesktopChars(const QString &text);

// Sanitizes a string for use as a filename: replaces characters invalid in filenames
// and spaces with "-", strips leading dots, and falls back to "application" if empty.
[[nodiscard]] QString sanitizeFileName(const QString &name);
}

#endif // DESKTOPUTILS_H
