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


static RS_Vector parsePageSizeArg(QString);
static void parsePagesNumArg(QString, PdfPrintParams&);
static void parseMarginsArg(QString, PdfPrintParams&);


int console_dxf2pdf(int argc, char* argv[])
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
    appDesc << "Turn a bunch of DXF files to VEC file(s).";
    appDesc << "";
    appDesc << "Examples:";
    appDesc << "";
    appDesc << "  " + librecad + QObject::tr( " *.dxf");
    appDesc << "    " + QObject::tr( "-- make all dxf files to vec files with the same names.");
    appDesc << "";
    appDesc << "  " + librecad + QObject::tr( " -o some.vec *.dxf");
    appDesc << "    " + QObject::tr( "-- print all dxf files to 'some.vec' file.");
    parser.setApplicationDescription( appDesc.join( "\n"));

    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption outFileOpt(QStringList() << "o" << "outfile",
        QObject::tr( "Output VEC file.", "file"));
    parser.addOption(outFileOpt);

    QCommandLineOption outDirOpt(QStringList() << "t" << "directory",
        QObject::tr( "Target output directory."), "path");
    parser.addOption(outDirOpt);

    parser.addPositionalArgument(QObject::tr( "<dxf_files>"), QObject::tr( "Input DXF file(s)"));

    parser.process(app);

    const QStringList args = parser.positionalArguments();

    if (args.isEmpty() || (args.size() == 1 && args[0] == "dxf2vec"))
        parser.showHelp(EXIT_FAILURE);

    PdfPrintParams params;

    params.outFile = parser.value(outFileOpt);
    params.outDir = parser.value(outDirOpt);

    for (auto arg : args) {
        QFileInfo dxfFileInfo(arg);
        if (dxfFileInfo.suffix().toLower() != "dxf")
            continue; // Skip files without .dxf extension
        params.dxfFiles.append(arg);
    }

    if (params.dxfFiles.isEmpty())
        parser.showHelp(EXIT_FAILURE);

    if (!params.outDir.isEmpty()) {
        // Create output directory
        if (!QDir().mkpath(params.outDir)) {
            qDebug() << "ERROR: Cannot create directory" << params.outDir;
            return EXIT_FAILURE;
        }
    }

    RS_FONTLIST->init();
    RS_PATTERNLIST->init();

    return app.exec();
}
