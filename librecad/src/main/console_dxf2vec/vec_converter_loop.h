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
#ifndef VEC_CONVERTER_LOOP_H
#define VEC_CONVERTER_LOOP_H

#include <QtCore>
#include <QPrinter>
#include "rs_units.h"

// Precision of 1 means TODO
struct VecConverterParams {
    QStringList dxfFiles;
    QString outDir;
    QString outFile;
    qint32 precision;
    qint32 unit;
    qreal paperScale;
};

class VecConverterLoop : public QObject {

    Q_OBJECT

public:

    VecConverterLoop(VecConverterParams& params, QObject* parent=0) :
        QObject(parent)
        , params{params}
    {
    }

public slots:

    void run();

signals:

    void finished();

private:
    VecConverterParams params{};

    void convertOneDxfToOneVec(const QString&);
    void convertManyDxfToOneVec();
};

#endif // VEC_CONVERTER_LOOP_H
