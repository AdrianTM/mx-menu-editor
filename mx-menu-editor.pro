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

QT       += widgets
CONFIG   += c++1z warn_on

TARGET = mx-menu-editor
TEMPLATE = app

# The following define makes your compiler warn you if you use any
# feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += main.cpp\
    addappdialog.cpp \
    mainwindow.cpp

HEADERS  += \
    addappdialog.h \
    mainwindow.h \
    version.h

FORMS    += \
    addappdialog.ui \
    mainwindow.ui

TRANSLATIONS += translations/mx-menu-editor_am.ts \
                translations/mx-menu-editor_ar.ts \
                translations/mx-menu-editor_bg.ts \
                translations/mx-menu-editor_ca.ts \
                translations/mx-menu-editor_cs.ts \
                translations/mx-menu-editor_da.ts \
                translations/mx-menu-editor_de.ts \
                translations/mx-menu-editor_el.ts \
                translations/mx-menu-editor_en.ts \
                translations/mx-menu-editor_es.ts \
                translations/mx-menu-editor_et.ts \
                translations/mx-menu-editor_eu.ts \
                translations/mx-menu-editor_fa.ts \
                translations/mx-menu-editor_fi.ts \
                translations/mx-menu-editor_fr.ts \
                translations/mx-menu-editor_fr_BE.ts \
                translations/mx-menu-editor_he_IL.ts \
                translations/mx-menu-editor_hi.ts \
                translations/mx-menu-editor_hr.ts \
                translations/mx-menu-editor_hu.ts \
                translations/mx-menu-editor_id.ts \
                translations/mx-menu-editor_is.ts \
                translations/mx-menu-editor_it.ts \
                translations/mx-menu-editor_ja.ts \
                translations/mx-menu-editor_kk.ts \
                translations/mx-menu-editor_ko.ts \
                translations/mx-menu-editor_lt.ts \
                translations/mx-menu-editor_mk.ts \
                translations/mx-menu-editor_mr.ts \
                translations/mx-menu-editor_nb.ts \
                translations/mx-menu-editor_nl.ts \
                translations/mx-menu-editor_pl.ts \
                translations/mx-menu-editor_pt.ts \
                translations/mx-menu-editor_pt_BR.ts \
                translations/mx-menu-editor_ro.ts \
                translations/mx-menu-editor_ru.ts \
                translations/mx-menu-editor_sk.ts \
                translations/mx-menu-editor_sl.ts \
                translations/mx-menu-editor_sq.ts \
                translations/mx-menu-editor_sr.ts \
                translations/mx-menu-editor_sv.ts \
                translations/mx-menu-editor_tr.ts \
                translations/mx-menu-editor_uk.ts \
                translations/mx-menu-editor_zh_CN.ts \
                translations/mx-menu-editor_zh_TW.ts

RESOURCES += \
    images.qrc


