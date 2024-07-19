#include <QtCore>
#include <QCoreApplication>
#include <QApplication>

#include "rs_debug.h"
#include "rs_fontlist.h"
#include "rs_patternlist.h"
#include "rs_settings.h"
#include "rs_system.h"

#include "main.h"

#include "console_dxf2vec.h"
#include "vec_converter_loop.h"


int console_dxf2vec(int argc, char* argv[])
{
    RS_DEBUG->setLevel(RS_Debug::D_NOTHING);

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("LibreCAD");
    QCoreApplication::setApplicationName("LibreCAD");
    QCoreApplication::setApplicationVersion(XSTR(LC_VERSION));

    QFileInfo prgInfo(QFile::decodeName(argv[0]));
    RS_SETTINGS->init(app.organizationName(), app.applicationName());
    RS_SYSTEM->init( app.applicationName(), app.applicationVersion(), XSTR(QC_APPDIR), argv[0]);

    QCommandLineParser parser;

    QStringList appDesc;
    QString librecad( prgInfo.filePath());
    if (prgInfo.baseName() != "dxf2vec") {
        librecad += " dxf2vec"; // executable is not dxf2vec, thus argv[1] must be 'dxf2vec'
        appDesc << "";
        appDesc << "dxf2vec " + QObject::tr( "usage: ") + librecad + QObject::tr( " [options] <dxf_files>");
    }
    appDesc << "";
    appDesc << "Turn a DXF file to VEC file.";
    appDesc << "";
    appDesc << "Example:";
    appDesc << "";
    appDesc << "  " + librecad + QObject::tr(" test.dxf");
    appDesc << "    " + QObject::tr("-- make test.dxf file to vec file with the same name.");
    parser.setApplicationDescription( appDesc.join( "\n"));

    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption outFileOpt(QStringList() << "o" << "outfile",
        QObject::tr( "Output VEC file.", "file"));
    parser.addOption(outFileOpt);

    QCommandLineOption outDirOpt(QStringList() << "t" << "directory",
        QObject::tr( "Target output directory."), "path");
    parser.addOption(outDirOpt);

    QCommandLineOption precisionOpt(QStringList() << "p"
                                                  << "precision",
                                    QObject::tr("Set the precision (default is 1.0)."),
                                    "value",
                                    "1.0");
    parser.addOption(precisionOpt);

    QCommandLineOption relativePrecisionOpt(QStringList() << "r"
                                                          << "relative",
                                            QObject::tr(
                                                "Use relative precision instead of absolute."));
    parser.addOption(relativePrecisionOpt);

    parser.addPositionalArgument(QObject::tr( "<dxf_files>"), QObject::tr( "Input DXF file(s)"));

    parser.process(app);

    const QStringList args = parser.positionalArguments();

    if (args.isEmpty() || (args.size() == 1 && args[0] == "dxf2vec")) {
        parser.showHelp(EXIT_FAILURE);
    }

    VecConverterParams params;

    params.outFile = parser.value(outFileOpt);
    params.outDir = parser.value(outDirOpt);

    bool ok;
    params.precision = parser.value(precisionOpt).toDouble(&ok);
    if (!ok) {
        qDebug() << "ERROR: Invalid precision value. Using default 1.0.";
        params.precision = 1.0;
    }

    params.absolute_precision = !parser.isSet(relativePrecisionOpt);

    for (auto &arg : args) {
        QFileInfo dxfFileInfo(arg);
        if (dxfFileInfo.suffix().toLower() != "dxf") {
            continue; // Skip files without .dxf extension
        }
        params.dxfFiles.append(arg);
    }

    if (params.dxfFiles.isEmpty()) {
        parser.showHelp(EXIT_FAILURE);
    }

    if (!params.outDir.isEmpty()) {
        // Create output directory
        if (!QDir().mkpath(params.outDir)) {
            qDebug() << "ERROR: Cannot create directory" << params.outDir;
            return EXIT_FAILURE;
        }
    }

    RS_FONTLIST->init();
    RS_PATTERNLIST->init();

    VecConverterLoop *loop = new VecConverterLoop(params, &app);

    QObject::connect(loop, SIGNAL(finished()), &app, SLOT(quit()));

    QTimer::singleShot(0, loop, SLOT(run()));

    return app.exec();
}
