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

#include <QDataStream>
#include <QList>
#include <QMap>
#include <QVector3D>
#include <QtCore>
#include <optional>

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
void douglasPeuckerSimplify(const QList<QVector3D> &points,
                            double epsilon,
                            QList<QVector3D> &simplified);
void reorderPolylines(const QList<std::pair<PolylineData, QList<QVector3D>>> &in,
                      QList<std::pair<PolylineData, QList<QVector3D>>> &out,
                      std::optional<QVector3D> start_point = std::nullopt);

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
    qDebug() << "p=" << params.precision << " abs=" << params.absolute_precision;

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

    std::optional<QVector3D> start_point = std::nullopt;

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
            douglasPeuckerSimplify(polylinePoints, epsilon, simplifiedPoints);

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
                RS_Vector seam_point = convertAndScalePoint(point->getPos());
                start_point = QVector3D(seam_point.x, seam_point.y, seam_point.z);
                qDebug() << "Start Point set:" << *start_point;
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
    reorderPolylines(allPolylinesPoints, reorderedPolylines, start_point);

    for (auto &polyline : reorderedPolylines) {
        for (auto &pt : polyline.second) {
            qDebug() << pt;
        }
    }

    serializePolylines(reorderedPolylines, params);

    qDebug() << "Printing" << dxfFile << "to" << params.outFile << "DONE";

    delete doc;
}

void reorderPolylines(const QList<std::pair<PolylineData, QList<QVector3D>>> &in,
                      QList<std::pair<PolylineData, QList<QVector3D>>> &out,
                      std::optional<QVector3D> start_point)
{
    out.clear();
    QList<std::pair<PolylineData, QList<QVector3D>>> remaining = in;

    auto findClosestPolyline = [](const QVector3D &point,
                                  QList<std::pair<PolylineData, QList<QVector3D>>> &polylines,
                                  bool allowHigherZ) {
        auto closestIt = polylines.end();
        float minDist = std::numeric_limits<float>::max();
        bool needInvert = false;
        bool needRotate = false;

        for (auto it = polylines.begin(); it != polylines.end(); ++it) {
            const auto &[data, points] = *it;
            QVector3D start = points.first();
            QVector3D end = points.last();

            auto checkPoint = [&](const QVector3D &p, bool invert) {
                if ((allowHigherZ || p.z() <= point.z()) && (p - point).lengthSquared() < minDist) {
                    minDist = (p - point).lengthSquared();
                    closestIt = it;
                    needInvert = invert;
                    needRotate = false;
                }
            };

            checkPoint(start, false);
            checkPoint(end, !data.closed);

            if (data.closed) {
                int rotateIndex = 0;
                for (int i = 1; i < points.size(); ++i) {
                    if ((allowHigherZ || points[i].z() <= point.z())
                        && (points[i] - point).lengthSquared() < minDist) {
                        minDist = (points[i] - point).lengthSquared();
                        closestIt = it;
                        needInvert = false;
                        needRotate = true;
                        rotateIndex = i;
                    }
                }
                if (needRotate) {
                    auto &mutablePoints = const_cast<QList<QVector3D> &>(closestIt->second);
                    std::rotate(mutablePoints.begin(),
                                mutablePoints.begin() + rotateIndex,
                                mutablePoints.end());
                }
            }
        }

        if (closestIt != polylines.end() && needInvert) {
            std::reverse(closestIt->second.begin(), closestIt->second.end());
        }

        return closestIt;
    };

    QVector3D currentPoint = start_point.value_or(QVector3D());
    bool firstPolyline = true;

    while (!remaining.empty()) {
        auto it = firstPolyline && start_point.has_value()
                      ? std::find_if(remaining.begin(),
                                     remaining.end(),
                                     [&](const auto &p) { return p.second.first() == currentPoint; })
                      : findClosestPolyline(currentPoint, remaining, !firstPolyline);

        if (it == remaining.end()) {
            // If no suitable polyline found, try again allowing higher Z
            it = findClosestPolyline(currentPoint, remaining, true);
        }

        if (it != remaining.end()) {
            out.append(*it);
            currentPoint = it->second.last();
            remaining.erase(it);
        } else {
            // No suitable polyline found, break the loop
            break;
        }

        firstPolyline = false;
    }

    // Append any remaining polylines
    out.append(remaining);
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

void douglasPeuckerSimplify(const QList<QVector3D> &points,
                            double epsilon,
                            QList<QVector3D> &simplified)
{
    simplified.clear();

    if (points.size() <= 2) {
        simplified = points;
        return;
    }

    QList<QPair<int, int>> stack;
    stack.push_back({0, points.size() - 1});

    QVector<bool> pointUsed(points.size(), false);

    while (!stack.isEmpty()) {
        int start = stack.back().first;
        int end = stack.back().second;
        stack.pop_back();

        double maxDistance = 0;
        int maxIndex = start;

        QVector3D startPoint = points[start];
        QVector3D endPoint = points[end];
        QVector3D direction = (endPoint - startPoint).normalized();

        for (int i = start + 1; i < end; ++i) {
            double distance = points[i].distanceToLine(startPoint, direction);
            if (distance > maxDistance) {
                maxDistance = distance;
                maxIndex = i;
            }
        }

        if (maxDistance > epsilon) {
            stack.push_back({maxIndex, end});
            stack.push_back({start, maxIndex});
        } else {
            pointUsed[start] = true;
            pointUsed[end] = true;
        }
    }

    // Add all used points to the simplified list in original order
    for (int i = 0; i < points.size(); ++i) {
        if (pointUsed[i]) {
            simplified.append(points[i]);
        }
    }

    // Ensure the last point is added if it's not already there
    if (simplified.last() != points.last()) {
        simplified.append(points.last());
    }
}
