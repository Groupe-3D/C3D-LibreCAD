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
#include <QDataStream>

#include "rs.h"
#include "rs_block.h"
#include "rs_blocklist.h"
#include "rs_graphic.h"
#include "rs_insert.h"
#include "rs_polyline.h"
#include "rs_units.h"

#include "vec_converter_loop.h"

#define DXF2VEC_MAGIC_NUM 1127433216
#define DXF2VEC_VERSION 1

struct alignas(16) PolylineData
{
    quint32 id = 0;
    qint32 color = 0;
    quint32 count = 0;
    qint16 padding = 0;
    bool visible = false;
    bool closed = false;
};

// void __debug(QString);

static bool openDocAndSetGraphic(RS_Document **, RS_Graphic **, const QString &);
void serializePolylines(const QList<std::pair<PolylineData, QList<QVector3D>>> &, const QString &);

void VecConverterLoop::run()
{
    /*
    if (params.outFile.isEmpty()) {
        for (auto &&f : params.dxfFiles) {
            convertOneDxfToOneVec(f);
        }
    } else {
        convertManyDxfToOneVec();
    }*/
    if (params.outFile.isEmpty()) {
        for (auto &&f : params.dxfFiles) {
            convertOneDxfToOneVec(f);
        }
    } else {
        qWarning() << "Outfile array is empty";
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

    if (!openDocAndSetGraphic(&doc, &graphic, dxfFile)) {
        return;
    }

    QList<std::pair<PolylineData, QList<QVector3D>>> allPolylinesPoints;

    auto processEntity = [&allPolylinesPoints, &graphic](RS_Entity *entity,
                                                         const RS_Vector& insertionPoint,
                                                         const RS_Vector& scaleFactorInput,
                                                         double rotation,
                                                         auto &&processEntityRef) -> void {
        RS_Vector scaleFactor = scaleFactorInput;
        if (scaleFactor.z == 0) {
            scaleFactor.z = 1;
        }

        if (entity->rtti() == RS2::EntityPolyline) {
            RS_Polyline *polyline = static_cast<RS_Polyline *>(entity);
            QList<QVector3D> polylinePoints;
            PolylineData polylineData;

            polylineData.id = polyline->getId();
            polylineData.color = polyline->getPen().getColor().toIntColor();
            polylineData.count = polyline->count();
            polylineData.padding = 0;
            polylineData.visible = polyline->isVisible();
            polylineData.closed = polyline->isClosed();

            for (const RS_Entity *e : *polyline) {
                if (e->rtti() == RS2::EntityLine) {
                    RS_Vector startPoint = e->getStartpoint();
                    startPoint = startPoint * scaleFactor;
                    startPoint.rotate(rotation);
                    startPoint += insertionPoint;
                    polylinePoints.append(QVector3D(startPoint.x, startPoint.y, startPoint.z));
                } else {
                    qDebug() << "Ignore entity with rtti=" << e->rtti()
                             << "(not an EntityLine) in EntityPolyline";
                }
            }

            const RS_Entity *lastEntity = polyline->last();
            if (lastEntity && lastEntity->rtti() == RS2::EntityLine) {
                RS_Vector endPoint = lastEntity->getEndpoint();
                endPoint = endPoint * scaleFactor;
                endPoint.rotate(rotation);
                endPoint += insertionPoint;
                polylinePoints.append(QVector3D(endPoint.x, endPoint.y, endPoint.z));
            }

            allPolylinesPoints.append(std::make_pair(polylineData, polylinePoints));

        } else if (entity->rtti() == RS2::EntityInsert) {
            RS_Insert *insert = static_cast<RS_Insert *>(entity);
            RS_Block *block = graphic->getBlockList()->find(insert->getName());

            if (block) {
                RS_Vector newInsertionPoint = insert->getInsertionPoint();
                RS_Vector newScaleFactor = insert->getScale();
                double newRotation = insert->getAngle();

                if (newScaleFactor.z == 0) {
                    newScaleFactor.z = 1;
                }

                newInsertionPoint = newInsertionPoint * scaleFactor;
                newInsertionPoint.rotate(rotation);
                newInsertionPoint += insertionPoint;

                newScaleFactor.x *= scaleFactor.x;
                newScaleFactor.y *= scaleFactor.y;
                newScaleFactor.z *= scaleFactor.z;

                newRotation += rotation;

                for (RS_Entity *subEntity : *block) {
                    processEntityRef(subEntity, newInsertionPoint, newScaleFactor, newRotation, processEntityRef);
                }
            }
        } else if (entity->rtti() == RS2::EntityLine) {

            QList<QVector3D> linePoints;
            PolylineData lineData;

            lineData.id = entity->getId();
            lineData.color = 0;
            lineData.count = 2;
            lineData.padding = 0;
            lineData.visible = entity->isVisible();
            lineData.closed = false;

            RS_Vector startPoint = entity->getStartpoint();
            RS_Vector endPoint = entity->getEndpoint();

            startPoint = startPoint * scaleFactor;
            startPoint.rotate(rotation);
            startPoint += insertionPoint;

            endPoint = endPoint * scaleFactor;
            endPoint.rotate(rotation);
            endPoint += insertionPoint;

            linePoints.append(QVector3D(startPoint.x, startPoint.y, startPoint.z));
            linePoints.append(QVector3D(endPoint.x, endPoint.y, endPoint.z));

            allPolylinesPoints.append(std::make_pair(lineData, linePoints));
        } else {
            qDebug() << "Ignore top level entity with rtti=" << entity->rtti()
                     << "(not an EntityPolyline, EntityInsert, or EntityLine)";
        }
    };

    for (RS_Entity *entity : *graphic) {
        processEntity(entity, RS_Vector(0, 0, 0), RS_Vector(1, 1, 1), 0, processEntity);
    }

    serializePolylines(allPolylinesPoints, params.outFile);

    //__debug(params.outFile);

    qDebug() << "Printing" << dxfFile << "to" << params.outFile << "DONE";

    delete doc;
}

void serializePolylines(const QList<std::pair<PolylineData, QList<QVector3D>>> &allPolylinesPoints,
                        const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Failed to open file for writing:" << filename;
        return;
    }

    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_6_6);
    out.setByteOrder(QDataStream::BigEndian);

    out << static_cast<quint32>(DXF2VEC_MAGIC_NUM + DXF2VEC_VERSION);
    out << static_cast<quint32>(allPolylinesPoints.size());

    for (const auto &polylinePair : allPolylinesPoints) {
        const PolylineData &polylineData = polylinePair.first;
        const QList<QVector3D> &polylinePoints = polylinePair.second;

        out << polylineData.id;
        out << polylineData.color;
        out << static_cast<quint32>(polylinePoints.count());
        out << polylineData.padding;
        out << polylineData.visible;
        out << polylineData.closed;

        for (const auto &point : polylinePoints) {
            out << point;
        }
    }

    file.close();
}

static bool openDocAndSetGraphic(RS_Document **doc, RS_Graphic **graphic, const QString &dxfFile)
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

/*
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

bool deserializePolylines(QList<std::pair<PolylineData, QList<QVector3D>>> &outPolylinesPoints,
                          const QString &filename)
{
    QFile file(filename);

    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open file for reading:" << filename;
        return false;
    }

    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_6_6);
    in.setByteOrder(QDataStream::BigEndian);

    quint32 paddingMagicHeader;
    in >> paddingMagicHeader;

    if (paddingMagicHeader != DXF2VEC_MAGIC_NUM) {
        qFatal() << "Error: This file is not a well formatted VEC file";
        return false;
    }

    quint32 totalElements;
    in >> totalElements;

    outPolylinesPoints.reserve(totalElements);

    for (quint32 i = 0; i < totalElements; ++i) {
        PolylineData polylineData;

        in >> polylineData.id;
        in >> polylineData.color;
        in >> polylineData.count;
        in >> polylineData.padding;
        in >> polylineData.visible;
        in >> polylineData.closed;

        quint32 numPoints = polylineData.count;

        QList<QVector3D> polylinePoints;
        polylinePoints.resize(numPoints);
        for (quint32 j = 0; j < numPoints; ++j) {
            in >> polylinePoints[j];
        }

        outPolylinesPoints.append({ polylineData, polylinePoints });
    }

    file.close();
    return true;
}

QColor fromIntColor(int color)
{
    if (color >= 0) {
        return QColor((color >> 16) & 0xFF, (color >> 8) & 0xFF, (color) & 0xFF);
    } else if (color == -1) {
        // RS2::FlagByLayer
        return QColor(0, 0, 0); // Layer color
    } else if (color == -2) {
        // RS2::FlagByBlock
        return QColor(0, 0, 0); // Block color
    }
    return QColor(0, 0, 0); // ¯\_(ツ)_/¯
}

void __debug(QString filename)
{
    QList<std::pair<PolylineData, QList<QVector3D>>> resultPolylinesPoints;
    deserializePolylines(resultPolylinesPoints, filename);

    quint16 polylineCount = 0;
    for (const auto &polylinePair : resultPolylinesPoints) {
        const PolylineData &polylineData = polylinePair.first;
        const QList<QVector3D> &polylinePoints = polylinePair.second;
        const QColor colorPoly = fromIntColor(polylineData.color);
        bool extrude = (colorPoly != QColor("red"));

        qDebug() << "# Polyline n" << polylineCount;
        qDebug() << "Visible:" << (polylineData.visible ? "true" : "false");
        qDebug() << "Closed:" << (polylineData.closed ? "true" : "false");
        qDebug() << "ID:" << polylineData.id;
        qDebug() << "Color:" << colorPoly << "| Extrude:" << (extrude ? "true" : "false");
        qDebug() << "Count:" << polylineData.count;

        for (const auto &point : polylinePoints) {
            // qDebug() << polylineCount << point.x() << point.z() << -point.y() << extrude;
            qDebug() << QVector3D{ point.x(), point.z(), -point.y() } << polylineData.id
                     << polylineCount << extrude;
        }
        polylineCount++;

        qDebug() << "";
    }

    qDebug() << "DONE";
}
*/
