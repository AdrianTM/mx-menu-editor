# **********************************************************************
# * Copyright (C) 2015 MX Authors
# *
# * Authors: Adrian
# *          MX Linux <http://mxlinux.org>
# *
# * This file is part of MX Menu Editor.
# *
# * MX Menu Editor is free software: you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation, either version 3 of the License, or
# * (at your option) any later version.
# *
# * MX Menu Editor is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with MX Menu Editor.  If not, see <http://www.gnu.org/licenses/>.
# **********************************************************************/

QT       += core gui widgets

TARGET = mx-menu-editor
TEMPLATE = app


SOURCES += main.cpp\
        mxmenueditor.cpp \
    addappdialog.cpp

HEADERS  += mxmenueditor.h \
    addappdialog.h

FORMS    += mxmenueditor.ui \
    addappdialog.ui

TRANSLATIONS += translations/mx-menu-editor_am.ts \
                translations/mx-menu-editor_ca.ts \
                translations/mx-menu-editor_cs.ts \
                translations/mx-menu-editor_da.ts \
                translations/mx-menu-editor_de.ts \
                translations/mx-menu-editor_el.ts \
                translations/mx-menu-editor_es.ts \
                translations/mx-menu-editor_fi.ts \
                translations/mx-menu-editor_fr.ts \
                translations/mx-menu-editor_hi.ts \
                translations/mx-menu-editor_hr.ts \
                translations/mx-menu-editor_hu.ts \
                translations/mx-menu-editor_it.ts \
                translations/mx-menu-editor_ja.ts \
                translations/mx-menu-editor_kk.ts \
                translations/mx-menu-editor_lt.ts \
                translations/mx-menu-editor_nl.ts \
                translations/mx-menu-editor_pl.ts \
                translations/mx-menu-editor_pt.ts \
                translations/mx-menu-editor_pt_BR.ts \
                translations/mx-menu-editor_ro.ts \
                translations/mx-menu-editor_ru.ts \
                translations/mx-menu-editor_sk.ts \
                translations/mx-menu-editor_sv.ts \
                translations/mx-menu-editor_tr.ts \
                translations/mx-menu-editor_uk.ts \
                translations/mx-menu-editor_zh_TW.ts

RESOURCES += \
    images.qrc


