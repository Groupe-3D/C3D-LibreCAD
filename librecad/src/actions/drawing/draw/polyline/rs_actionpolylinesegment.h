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
#ifndef RS_ACTIONPOLYLINESEGMENT_H
#define RS_ACTIONPOLYLINESEGMENT_H

#include "rs_previewactioninterface.h"

class RS_Entity;
class RS_EntityContainer;
class RS_GraphicView;
class RS_Polyline;
class RS_Vector;

/**
 * This action class can handle Create Polyline Existing from Segments
 *
 * @author Andrew Mustun
 */
class RS_ActionPolylineSegment:public RS_PreviewActionInterface {
    Q_OBJECT
public:
    RS_ActionPolylineSegment(
        RS_EntityContainer &container,
        RS_GraphicView &graphicView);
    RS_ActionPolylineSegment(
        RS_EntityContainer &container,
        RS_GraphicView &graphicView,
        RS_Entity *targetEntity);
    void init(int status) override;
    void drawSnapper() override;
protected:
    /**
     * Action States.
     */
    enum Status {
        ChooseEntity = 0 /**< Choosing one of the polyline segments. */
    };

    //! create polyline from segments
//! @param useSelected only create from selected entities
    RS_Polyline* convertPolyline(RS_EntityContainer* cnt, RS_Entity *selectedEntity, bool useSelected = false, bool createOnly=false);
    RS_Vector appendPol(RS_Polyline *current, RS_Polyline *toAdd, bool reversed);
    RS_Entity *targetEntity = nullptr;
    bool initWithTarget{false};

    RS2::CursorType doGetMouseCursor(int status) override;
    void onMouseLeftButtonRelease(int status, LC_MouseEvent *e) override;
    void onMouseRightButtonRelease(int status, LC_MouseEvent *e) override;
    void onMouseMoveEvent(int status, LC_MouseEvent *event) override;
    void updateMouseButtonHints() override;
    void doTrigger() override;
};
#endif
