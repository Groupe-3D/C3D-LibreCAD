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
#include "rs_blocklist.h"
#include "rs_graphic.h"
#include "rs_painterqt.h"
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

    for (auto entity : *graphic) {
        if (entity->rtti() == RS2::EntityPolyline) {
            RS_Polyline *polyline = static_cast<RS_Polyline *>(entity);
            QList<QVector3D> polylinePoints;

            for (auto v : polyline->getRefPoints()) {
                polylinePoints.append(QVector3D(v.x, v.y, 0.0));
            }
            allPolylinesPoints.append(polylinePoints);
        }
    }

    // DEBUG
    for (const auto &polylinePoints : allPolylinesPoints) {
        qDebug() << "Polyline:";
        for (const auto &point : polylinePoints) {
            qDebug() << point;
        }
    }

    qDebug() << "Printing" << dxfFile << "to" << params.outFile << "DONE";

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
    int nrPages = 0;

    // FIXME: Should probably open and print all dxf files in one 'for' loop.
    // Tried but failed to do this. It looks like some 'chicken and egg'
    // situation for the QPrinter and RS_PainterQt. Therefore, first open
    // all dxf files and apply required actions. Then run another 'for'
    // loop for actual printing.
    for (auto &dxfFile : params.dxfFiles) {
        DxfPage page;

        page.dxfFile = dxfFile;

        if (!openDocAndSetGraphic(&page.doc, &page.graphic, dxfFile))
            continue;

        qDebug() << "Opened" << dxfFile;

        //touchGraphic(page.graphic, params);

        pages.append(page);

        nrPages++;
    }

    QPrinter printer(QPrinter::HighResolution);

    if (nrPages > 0) {
        // FIXME: Is it possible to set up printer and paper for every
        // opened dxf file and tie them with painter? For now just using
        // data extracted from the first opened dxf file for all pages.
        //setupPrinterAndPaper(pages.at(0).graphic, printer, params);
    }

    RS_PainterQt painter(&printer);

    // And now it's time to actually print all previously opened dxf files.
    for (auto &page : pages) {
        nrPages--;

        qDebug() << "Printing" << page.dxfFile
                 << "to" << params.outFile << ">>>>";

        //drawPage(page.graphic, printer, painter);

        qDebug() << "Printing" << page.dxfFile
                 << "to" << params.outFile << "DONE";

        delete page.doc;

        if (nrPages > 0)
            printer.newPage();
    }

    painter.end();
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
