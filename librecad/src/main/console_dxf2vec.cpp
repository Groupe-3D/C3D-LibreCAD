#include <QList>
#include <QVector3D>
#include <QString>
#include <QFileInfo>
#include <memory>
#include "rs_document.h"
#include "rs_graphic.h"
#include "rs_entity.h"
#include "rs_polyline.h"
#include "rs_arc.h"
#include "rs_circle.h"
#include "rs_ellipse.h"
#include "rs_line.h"
#include "rs_math.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include "rs_system.h"
#include "rs_settings.h"


///////////////////////////////////////////////////////////////////////
/// \brief dxfToPolygons opens a DXF file and converts it into points
/// \return
//////////////////////////////////////////////////////////////////////
std::unique_ptr<QList<QList<QVector3D>>> dxfToPolygons(const QString&);

/////////
/// \brief console_dxf2vec is called if librecad
/// as console dxf2vec tool for converting DXF to VEC.
/// \param argc
/// \param argv
/// \return
///
int console_dxf2vec(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("LibreCAD");
    QCoreApplication::setApplicationName("LibreCAD");
    QCoreApplication::setApplicationVersion(XSTR(LC_VERSION));

    QFileInfo prgInfo(QFile::decodeName(argv[0]));
    QString prgDir(prgInfo.absolutePath());
    RS_SETTINGS->init(app.organizationName(), app.applicationName());
    RS_SYSTEM->init(app.applicationName(), app.applicationVersion(),
        XSTR(QC_APPDIR), prgDir.toLatin1().data());

    QCommandLineParser parser;
    parser.setApplicationDescription("Convert a DXF file to a binary file containing QList<QList<QVector3D>>.");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption outFileOpt(QStringList() << "o" << "outfile",
        "Output binary file.", "file");
    parser.addOption(outFileOpt);

    parser.addPositionalArgument("dxffile", "Input DXF file.");

    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.isEmpty())
        parser.showHelp(EXIT_FAILURE);

    QString dxfFile = args.at(0);
    QFileInfo dxfFileInfo(dxfFile);
    if (!dxfFileInfo.exists() || !dxfFileInfo.isFile()) {
        qCritical() << "Input DXF file does not exist or is not a file:" << dxfFile;
        return EXIT_FAILURE;
    }

    QString outFile = parser.value(outFileOpt);
    if (outFile.isEmpty())
        outFile = dxfFileInfo.completeBaseName() + ".vec";

    std::unique_ptr<QList<QList<QVector3D>>> polygons = dxfToPolygons(dxfFile);

    QFile file(outFile);
    if (!file.open(QIODevice::WriteOnly)) {
        qCritical() << "Failed to open output file for writing:" << outFile;
        return EXIT_FAILURE;
    }

    QDataStream stream(&file);
    stream << *polygons;

    qInfo() << "Successfully converted" << dxfFile << "to" << outFile;

    return EXIT_SUCCESS;
}

std::unique_ptr<QList<QList<QVector3D>>> dxfToPolygons(const QString& dxfFile)
{
    auto polygons = std::make_unique<QList<QList<QVector3D>>>();

    std::unique_ptr<RS_Document> doc = std::make_unique<RS_Document>();
    if (!doc->open(dxfFile, RS2::FormatUnknown)){
        return polygons;
    }

    RS_Graphic* graphic = doc->getGraphic();
    if (graphic == nullptr) {
        return polygons;
    }

    // TODO unify a way to specify precision (angular): pi/18 radians (10-degrees) for now
    for (RS_Entity* entity : *graphic) {
        QList<QVector3D> polygon;

        switch (entity->rtti()) {
        case RS2::EntityPolyline: {
            RS_Polyline* polyline = static_cast<RS_Polyline*>(entity);
            for (RS_Entity* vertex : *polyline) {
                RS_Vector pos = vertex->getStartpoint();
                polygon.append(QVector3D(pos.x, pos.y, 0.0)); // z should be reachable here
            }
            break;
        }
        case RS2::EntityArc: {
            RS_Arc* arc = static_cast<RS_Arc*>(entity);
            RS_Vector center = arc->getCenter();
            double radius = arc->getRadius();
            double startAngle = arc->getAngle1();
            double endAngle = arc->getAngle2();
            int segments = static_cast<int>(std::ceil(std::abs(endAngle - startAngle) / (M_PI / 18))); // Divide into pi/18 radians segments
            for (int i = 0; i <= segments; ++i) {
                double angle = startAngle + i * (endAngle - startAngle) / segments;
                RS_Vector pos = center + RS_Vector(radius * std::cos(angle), radius * std::sin(angle));
                polygon.append(QVector3D(pos.x, pos.y, 0.0));
            }
            break;
        }
        case RS2::EntityCircle: {
            RS_Circle* circle = static_cast<RS_Circle*>(entity);
            RS_Vector center = circle->getCenter();
            double radius = circle->getRadius();
            for (int i = 0; i < 36; ++i) { // Divide into pi/18 radians segments
                double angle = i * (2 * M_PI) / 36;
                RS_Vector pos = center + RS_Vector(radius * std::cos(angle), radius * std::sin(angle));
                polygon.append(QVector3D(pos.x, pos.y, 0.0));
            }
            break;
        }
        case RS2::EntityEllipse: {
            RS_Ellipse* ellipse = static_cast<RS_Ellipse*>(entity);
            RS_Vector center = ellipse->getCenter();
            RS_Vector majorP = ellipse->getMajorP();
            double ratio = ellipse->getRatio();
            double startAngle = ellipse->getAngle1();
            double endAngle = ellipse->getAngle2();
            int segments = static_cast<int>(std::ceil(std::abs(endAngle - startAngle) / (M_PI / 18))); // Divide into pi/18 radians segments
            for (int i = 0; i <= segments; ++i) {
                double angle = startAngle + i * (endAngle - startAngle) / segments;
                RS_Vector pos = center + majorP * std::cos(angle) + majorP.perpendicular() * ratio * std::sin(angle);
                polygon.append(QVector3D(pos.x, pos.y, 0.0));
            }
            break;
        }
        case RS2::EntityLine: {
            RS_Line* line = static_cast<RS_Line*>(entity);
            RS_Vector startPos = line->getStartpoint();
            RS_Vector endPos = line->getEndpoint();
            polygon.append(QVector3D(startPos.x, startPos.y, 0.0));
            polygon.append(QVector3D(endPos.x, endPos.y, 0.0));
            break;
        }
        default:
            break;
        }

        if (!polygon.isEmpty()) {
            polygons.append(polygon);
        }
    }

    return polygons;
}