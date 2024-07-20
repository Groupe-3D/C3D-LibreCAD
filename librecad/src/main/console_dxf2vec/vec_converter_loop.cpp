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
#include "rs_arc.h"
#include "rs_block.h"
#include "rs_blocklist.h"
#include "rs_graphic.h"
#include "rs_insert.h"
#include "rs_line.h"
#include "rs_point.h"
#include "rs_polyline.h"
#include "rs_units.h"

#include "vec_converter_loop.h"

#define DXF2VEC_MIN_EPSILON 0.001
#define DXF2VEC_ANGULAR_FACTOR 1.0
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
void serializePolylines(const QList<std::pair<PolylineData, QList<QVector3D>>> &,
                        const VecConverterParams &);
double calculateEpsilon(double clientPrecision, const RS_Graphic *graphic = nullptr);
double pointToLineDistance(const QVector3D &point,
                           const QVector3D &lineStart,
                           const QVector3D &lineEnd);
void douglasPeuckerSimplify(const QList<QVector3D> &points,
                            double epsilon,
                            QList<QVector3D> &simplified,
                            int start,
                            int end);
void reorderPolylines(QList<std::pair<PolylineData, QList<QVector3D>>> &in,
                      QList<std::pair<PolylineData, QList<QVector3D>>> &out,
                      QVector3D start_point);

void VecConverterLoop::run()
{
    if (params.outFile.isEmpty()) {
        for (auto &&f : params.dxfFiles) {
            convertOneDxfToOneVec(f, params.precision, params.absolute_precision);
        }
    } else {
        qWarning() << "Outfile array is empty";
    }

    emit finished();
}

void VecConverterLoop::convertOneDxfToOneVec(const QString &dxfFile,
                                             double epsilon_input,
                                             bool absolute)
{
    QFileInfo dxfFileInfo(dxfFile);
    params.outFile = (params.outDir.isEmpty() ? dxfFileInfo.path() : params.outDir) + "/"
                     + dxfFileInfo.completeBaseName() + ".vec";

    RS_Document *doc;
    RS_Graphic *graphic;

    if (!openDocAndSetGraphic(&doc, &graphic, dxfFile)) {
        return;
    }

    double epsilon = calculateEpsilon(epsilon_input, absolute ? nullptr : graphic);

    QList<std::pair<PolylineData, QList<QVector3D>>> allPolylinesPoints;

    RS2::Unit drawingUnit = graphic->getUnit();
    params.unit = static_cast<qint32>(drawingUnit);
    RS_Units::setCurrentDrawingUnits(drawingUnit);

    RS_Vector start_point;

    auto processEntity = [&allPolylinesPoints,
                          &graphic,
                          &start_point,
                          drawingUnit,
                          epsilon](RS_Entity *entity,
                                   const RS_Vector &insertionPoint,
                                   const RS_Vector &scaleFactorInput,
                                   double rotation,
                                   auto &&processEntityRef) -> void {
        RS_Vector scaleFactor = scaleFactorInput;
        if (scaleFactor.z == 0) {
            scaleFactor.z = 1;
        }

        auto convertAndScalePoint =
            [drawingUnit, &scaleFactor, &rotation, &insertionPoint](const RS_Vector &point) {
                RS_Vector convertedPoint = RS_Units::convert(point, drawingUnit, RS2::Millimeter);
                convertedPoint = convertedPoint * scaleFactor;
                convertedPoint.rotate(rotation);
                convertedPoint += insertionPoint;
                return convertedPoint;
            };

        switch (entity->rtti()) {
        case RS2::EntityPolyline: {
            qDebug() << "TOP RS2::EntityPolyline";
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
                    qDebug() << "RS2::EntityLine";
                    const RS_Line *line = dynamic_cast<const RS_Line *>(e);
                    if (line) {
                        RS_Vector startPoint = convertAndScalePoint(line->getStartpoint());
                        RS_Vector endPoint = convertAndScalePoint(line->getEndpoint());
                        polylinePoints.append(QVector3D(startPoint.x, startPoint.y, startPoint.z));
                        polylinePoints.append(QVector3D(endPoint.x, endPoint.y, endPoint.z));
                    } else {
                        qDebug() << "Failed to cast EntityLine";
                    }

                } else if (e->rtti() == RS2::EntityArc) {
                    qDebug() << "RS2::EntityArc";
                    const RS_Arc *arc = dynamic_cast<const RS_Arc *>(e);
                    if (arc) {
                        double arcLength = arc->getLength();
                        int numSegments = std::max(2,
                                                   static_cast<int>(
                                                       arcLength
                                                       / (epsilon * DXF2VEC_ANGULAR_FACTOR)));

                        RS_Vector center = arc->getCenter();
                        double radius = arc->getRadius();
                        double startAngle = arc->getAngle1();
                        double endAngle = arc->getAngle2();
                        bool reversed = arc->isReversed();

                        double angleStep = (endAngle - startAngle) / numSegments;
                        if (reversed) {
                            angleStep = -angleStep;
                        }

                        for (int i = 0; i <= numSegments; ++i) {
                            double angle = startAngle + i * angleStep;
                            RS_Vector point(center.x + radius * cos(angle),
                                            center.y + radius * sin(angle),
                                            center.z);
                            RS_Vector transformedPoint = convertAndScalePoint(point);
                            polylinePoints.append(QVector3D(transformedPoint.x,
                                                            transformedPoint.y,
                                                            transformedPoint.z));
                        }
                    } else {
                        qDebug() << "Failed to cast EntityArc";
                    }
                } else {
                    qDebug() << "Ignore entity with rtti=" << e->rtti()
                             << "(not an EntityLine or EntityArc) in EntityPolyline";
                }
            }

            if (polylineData.closed && !polylinePoints.isEmpty()) {
                polylinePoints.append(polylinePoints.first());
            }

            // Apply Douglas-Peucker simplification to this polyline
            QList<QVector3D> simplifiedPoints;
            douglasPeuckerSimplify(polylinePoints,
                                   epsilon,
                                   simplifiedPoints,
                                   0,
                                   polylinePoints.size() - 1);

            polylineData.count = simplifiedPoints.size();
            allPolylinesPoints.append(std::make_pair(polylineData, simplifiedPoints));
            break;
        }
        case RS2::EntityInsert: {
            qDebug() << "TOP RS2::EntityInsert";
            RS_Insert *insert = static_cast<RS_Insert *>(entity);
            RS_Block *block = graphic->getBlockList()->find(insert->getName());

            if (block) {
                RS_Vector newInsertionPoint = convertAndScalePoint(insert->getInsertionPoint());
                RS_Vector newScaleFactor = insert->getScale();
                double newRotation = insert->getAngle() + rotation;

                if (newScaleFactor.z == 0) {
                    newScaleFactor.z = 1;
                }

                newScaleFactor.x *= scaleFactor.x;
                newScaleFactor.y *= scaleFactor.y;
                newScaleFactor.z *= scaleFactor.z;

                for (RS_Entity *subEntity : *block) {
                    processEntityRef(subEntity,
                                     newInsertionPoint,
                                     newScaleFactor,
                                     newRotation,
                                     processEntityRef);
                }
            }
            break;
        }
        case RS2::EntityLine: {
            qDebug() << "TOP RS2::EntityLine";
            const RS_Line *line = dynamic_cast<const RS_Line *>(entity);

            if (line) {
                QList<QVector3D> linePoints;
                PolylineData lineData;

                lineData.id = line->getId();
                lineData.color = line->getPen().getColor().toIntColor();
                lineData.count = 2;
                lineData.padding = 0;
                lineData.visible = line->isVisible();
                lineData.closed = false;

                RS_Vector startPoint = convertAndScalePoint(line->getStartpoint());
                RS_Vector endPoint = convertAndScalePoint(line->getEndpoint());

                linePoints.append(QVector3D(startPoint.x, startPoint.y, startPoint.z));
                linePoints.append(QVector3D(endPoint.x, endPoint.y, endPoint.z));

                allPolylinesPoints.append(std::make_pair(lineData, linePoints));
            } else {
                qDebug() << "Failed to cast EntityLine";
            }

            break;
        }
        case RS2::EntityPoint: {
            qDebug() << "TOP RS2::EntityPoint";
            const RS_Point *point = dynamic_cast<const RS_Point *>(entity);
            qDebug() << "point color" << point->getPen().getColor().toQColor();
            if (point->getPen().getColor().toQColor() == QColor(Qt::green)) {
                start_point = convertAndScalePoint(point->getPos());
                qDebug() << "start point found=" << start_point.x << start_point.y << start_point.z;
            }
            break;
        }
        default: {
            qDebug() << "Ignore top level entity with rtti=" << entity->rtti()
                     << "(not an EntityPolyline, EntityInsert, or EntityLine)";
        }
        }
    };

    for (RS_Entity *entity : *graphic) {
        processEntity(entity, RS_Vector(0, 0, 0), RS_Vector(1, 1, 1), 0, processEntity);
    }

    QList<std::pair<PolylineData, QList<QVector3D>>> reorderedPolylines;

    reorderPolylines(allPolylinesPoints,
                     reorderedPolylines,
                     QVector3D(start_point.x, start_point.y, start_point.z));

    for (auto &polyline : reorderedPolylines) {
        for (auto &pt : polyline.second) {
            qDebug() << pt;
        }
    }

    serializePolylines(reorderedPolylines, params);

    qDebug() << "Printing" << dxfFile << "to" << params.outFile << "DONE";

    delete doc;
}

void reorderPolylines(QList<std::pair<PolylineData, QList<QVector3D>>> &allPolylinesPoints,
                      QList<std::pair<PolylineData, QList<QVector3D>>> &reorderedPolylines,
                      QVector3D start_point)
{
    // Find the polyline with the closest point to start_point
    int closestPolylineIndex = -1;
    int closestPointIndex = -1;
    double minDistance = std::numeric_limits<double>::max();

    for (int i = 0; i < allPolylinesPoints.size(); ++i) {
        const auto &polyline = allPolylinesPoints[i].second;
        for (int j = 0; j < polyline.size(); ++j) {
            double distance = start_point.distanceToPoint(polyline[j]);
            if (distance < minDistance) {
                minDistance = distance;
                closestPolylineIndex = i;
                closestPointIndex = j;
            }
        }
    }

    if (closestPolylineIndex != -1) {
        // Rotate the closest polyline if it's closed
        auto &closestPolyline = allPolylinesPoints[closestPolylineIndex];
        if (closestPolyline.first.closed) {
            std::rotate(closestPolyline.second.begin(),
                        closestPolyline.second.begin() + closestPointIndex,
                        closestPolyline.second.end());
        }
        reorderedPolylines.append(closestPolyline);
        allPolylinesPoints.removeAt(closestPolylineIndex);
    }

    // Add remaining polylines in order of closest endpoint
    while (!allPolylinesPoints.isEmpty()) {
        QVector3D lastPoint = reorderedPolylines.last().second.last();
        int nextPolylineIndex = -1;
        double minDistance = std::numeric_limits<double>::max();

        for (int i = 0; i < allPolylinesPoints.size(); ++i) {
            const auto &polyline = allPolylinesPoints[i].second;
            double distanceStart = lastPoint.distanceToPoint(polyline.first());
            double distanceEnd = lastPoint.distanceToPoint(polyline.last());

            if (distanceStart < minDistance) {
                minDistance = distanceStart;
                nextPolylineIndex = i;
            }
            if (distanceEnd < minDistance) {
                minDistance = distanceEnd;
                nextPolylineIndex = i;
            }
        }

        if (nextPolylineIndex != -1) {
            auto &nextPolyline = allPolylinesPoints[nextPolylineIndex];
            if (nextPolyline.first.closed) {
                // Rotate closed polyline to start with the closest point
                int closestIndex = 0;
                minDistance = std::numeric_limits<double>::max();
                for (int i = 0; i < nextPolyline.second.size(); ++i) {
                    double distance = lastPoint.distanceToPoint(nextPolyline.second[i]);
                    if (distance < minDistance) {
                        minDistance = distance;
                        closestIndex = i;
                    }
                }
                std::rotate(nextPolyline.second.begin(),
                            nextPolyline.second.begin() + closestIndex,
                            nextPolyline.second.end());
            } else if (lastPoint.distanceToPoint(nextPolyline.second.last())
                       < lastPoint.distanceToPoint(nextPolyline.second.first())) {
                // Reverse open polyline if its end is closer
                std::reverse(nextPolyline.second.begin(), nextPolyline.second.end());
            }
            reorderedPolylines.append(nextPolyline);
            allPolylinesPoints.removeAt(nextPolylineIndex);
        }
    }
}

void serializePolylines(const QList<std::pair<PolylineData, QList<QVector3D>>> &allPolylinesPoints,
                        const VecConverterParams &params)
{
    QString filename = params.outFile;
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

    out << static_cast<qint32>(params.unit);
    out << static_cast<qreal>(params.paperScale);

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

double calculateEpsilon(double clientPrecision, const RS_Graphic *graphic)
{
    double epsilon = std::max(clientPrecision, DXF2VEC_MIN_EPSILON);

    if (graphic) {
        RS_Vector size = graphic->getSize();
        double maxDimension = std::max(size.x, size.y);
        epsilon = maxDimension * (clientPrecision / 100.0);
    }

    return epsilon;
}

/*
double pointToLineDistance(const QVector3D &point,
                           const QVector3D &lineStart,
                           const QVector3D &lineEnd)
{
    QVector3D line = lineEnd - lineStart;
    QVector3D pointVector = point - lineStart;
    QVector3D crossProduct = QVector3D::crossProduct(line, pointVector);
    return crossProduct.length() / line.length();
}*/

void douglasPeuckerSimplify(const QList<QVector3D> &points,
                            double epsilon,
                            QList<QVector3D> &simplified,
                            int start,
                            int end)
{
    if (end - start <= 1) {
        simplified.append(points[start]);
        return;
    }

    double maxDistance = 0;
    int maxIndex = start;

    for (int i = start + 1; i < end; ++i) {
        double distance = pointToLineDistance(points[i], points[start], points[end]);
        if (distance > maxDistance) {
            maxDistance = distance;
            maxIndex = i;
        }
    }

    if (maxDistance > epsilon) {
        douglasPeuckerSimplify(points, epsilon, simplified, start, maxIndex);
        douglasPeuckerSimplify(points, epsilon, simplified, maxIndex, end);
    } else {
        simplified.append(points[start]);
        simplified.append(points[end]);
    }
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
