/******************************************************************************
**
** This file was created for the LibreCAD project, a 2D CAD program.
** File added by Kidev in Constructions-3D fork of the project
**
** Copyright (C) 2020 Nikita Letov <letovnn@gmail.com>
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
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
**
** This copyright notice MUST APPEAR in all copies of the script!
**
******************************************************************************/

#include <QList>
#include <QVector3D>
#include <QtCore>

#include "rs.h"
#include "rs_block.h"
#include "rs_blocklist.h"
#include "rs_graphic.h"
#include "rs_insert.h"
#include "rs_polyline.h"
#include "rs_units.h"

#include "vec_converter_loop.h"

static bool openDocAndSetGraphic(RS_Document **, RS_Graphic **, const QString &);

void VecConverterLoop::run()
{
    if (params.outFile.isEmpty()) {
        for (auto &&f : params.dxfFiles) {
            convertOneDxfToOneVec(f);
        }
    } else {
        convertManyDxfToOneVec();
    }

    emit finished();
}

void VecConverterLoop::convertOneDxfToOneVec(const QString &dxfFile)
{
    QFileInfo dxfFileInfo(dxfFile);
    params.outFile = (params.outDir.isEmpty() ? dxfFileInfo.path() : params.outDir) + "/"
                     + dxfFileInfo.completeBaseName() + ".vec";

    RS_Document *doc;
    RS_Graphic *graphic;

    if (!openDocAndSetGraphic(&doc, &graphic, dxfFile))
        return;

    qDebug() << "Printing" << dxfFile << "to" << params.outFile << ">>>>";

    QList<QList<QVector3D>> allPolylinesPoints;

    auto processEntity = [&allPolylinesPoints, &graphic](RS_Entity *entity,
                                                         auto &&processEntityRef) -> void {
        if (entity->rtti() == RS2::EntityPolyline) {
            RS_Polyline *polyline = static_cast<RS_Polyline *>(entity);
            QList<QVector3D> polylinePoints;
            for (const RS_Entity *e : *polyline) {
                if (e->rtti() == RS2::EntityLine) {
                    RS_Vector startPoint = e->getStartpoint();
                    polylinePoints.append(QVector3D(startPoint.x, startPoint.y, startPoint.z));
                } else {
                    qDebug() << "Ignore entity with rtti=" << e->rtti()
                             << "(not an EntityLine) in EntityPolyline";
                }
            }
            allPolylinesPoints.append(polylinePoints);
        } else if (entity->rtti() == RS2::EntityInsert) {
            RS_Block *block = graphic->getBlockList()->find(
                static_cast<RS_Insert *>(entity)->getName());
            if (block) {
                for (RS_Entity *subEntity : *block) {
                    processEntityRef(subEntity, processEntityRef);
                }
            }
        } else {
            qDebug() << "Ignore top level entity with rtti=" << entity->rtti()
                     << "(not an EntityPolyline)";
        }
    };

    for (RS_Entity *entity : *graphic) {
        processEntity(entity, processEntity);
    }

    // DEBUG
    int polylineCount = 0;

    for (const auto &polylinePoints : allPolylinesPoints) {
        qDebug() << "Polyline no=" << polylineCount++ << ":";
        for (const auto &point : polylinePoints) {
            qDebug() << point;
        }
    }

    qDebug() << "Printing" << dxfFile << "to" << params.outFile << "DONE";

    // TODO exchange data with shared memory buffer here, or use .vec

    delete doc;
}

void VecConverterLoop::convertManyDxfToOneVec() {

    struct DxfPage {
        RS_Document* doc;
        RS_Graphic* graphic;
        QString dxfFile;
        QPageSize::PageSizeId paperSize;
    };

    if (!params.outDir.isEmpty()) {
        QFileInfo outFileInfo(params.outFile);
        params.outFile = params.outDir + "/" + outFileInfo.fileName();
    }

    QVector<DxfPage> pages;

    for (auto &dxfFile : params.dxfFiles) {
        DxfPage page;

        page.dxfFile = dxfFile;

        if (!openDocAndSetGraphic(&page.doc, &page.graphic, dxfFile))
            continue;

        qDebug() << "Opened" << dxfFile;

        //touchGraphic(page.graphic, params);

        pages.append(page);

        //nrPages++;
    }

    // TODO
}


static bool openDocAndSetGraphic(RS_Document** doc, RS_Graphic** graphic,
    const QString& dxfFile)
{
    *doc = new RS_Graphic();

    if (!(*doc)->open(dxfFile, RS2::FormatUnknown)) {
        qDebug() << "ERROR: Failed to open document" << dxfFile;
        delete *doc;
        return false;
    }

    *graphic = (*doc)->getGraphic();
    if (*graphic == nullptr) {
        qDebug() << "ERROR: No graphic in" << dxfFile;
        delete *doc;
        return false;
    }

    return true;
}
