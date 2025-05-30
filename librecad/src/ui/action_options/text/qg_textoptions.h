/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2010 R. van Twisk (librecad@rvt.dds.nl)
** Copyright (C) 2001-2003 RibbonSoft. All rights reserved.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software 
** Foundation and appearing in the file gpl-2.0.txt included in the
** packaging of this file.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
**
** This copyright notice MUST APPEAR in all copies of the script!  
**
**********************************************************************/
#ifndef QG_TEXTOPTIONS_H
#define QG_TEXTOPTIONS_H

#include<memory>
#include<QWidget>
#include "lc_actionoptionswidgetbase.h"

class RS_ActionInterface;
class RS_ActionDrawText;
namespace Ui {
    class Ui_TextOptions;
}

class QG_TextOptions : public LC_ActionOptionsWidgetBase{
    Q_OBJECT
public:
    QG_TextOptions();
    ~QG_TextOptions() override;
public slots:
    void onTextChanged();
    void onAngleChanged();
    void languageChange() override;
protected:
    RS_ActionDrawText* action = nullptr;
    void doSaveSettings() override;
    void doSetAction(RS_ActionInterface *a, bool update) override;
    std::unique_ptr<Ui::Ui_TextOptions> ui;
};
#endif // QG_TEXTOPTIONS_H
