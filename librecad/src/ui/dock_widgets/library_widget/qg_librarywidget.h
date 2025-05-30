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
#ifndef QG_LIBRARYWIDGET_H
#define QG_LIBRARYWIDGET_H

#include <memory>

#include <QWidget>
#include <QModelIndex>

class QG_ActionHandler;
class QListView;
class QModelIndex;
class QPushButton;
class QStandardItemModel;
class QStandardItem;
class QTreeView;

class QG_LibraryWidget : public QWidget
{
    Q_OBJECT

public:
    QG_LibraryWidget(QWidget* parent = nullptr, const char* name = nullptr, Qt::WindowFlags fl = {});
    virtual ~QG_LibraryWidget();

    QPushButton* getInsertButton() const
    {
        return bInsert;
    }

private:
    QPushButton *bInsert=nullptr;

    virtual QString getItemDir( QStandardItem * item );
    virtual QString getItemPath( QStandardItem * item );
    virtual QIcon getIcon( const QString & dir, const QString & dxfFile, const QString & dxfPath );
    virtual QString getPathToPixmap( const QString & dir, const QString & dxfFile, const QString & dxfPath );

public slots:
    virtual void setActionHandler( QG_ActionHandler * ah );
    void keyPressEvent( QKeyEvent *e ) override;
    virtual void insert();
    virtual void refresh();
    virtual void scanTree();
    virtual void buildTree();
    virtual void appendTree( QStandardItem * item, QString directory );
    virtual void updatePreview( QModelIndex idx );
    virtual void expandView( QModelIndex idx );
    virtual void collapseView( QModelIndex idx );
    void updateWidgetSettings();

signals:
    void escape();

private:
    QG_ActionHandler* actionHandler = nullptr;
    std::unique_ptr<QStandardItemModel> dirModel;
    std::unique_ptr<QStandardItemModel> iconModel;
    QTreeView *dirView = nullptr;
    QListView *ivPreview = nullptr;
    QPushButton *bRefresh = nullptr;
    QPushButton *bRebuild = nullptr;
};

#endif // QG_LIBRARYWIDGET_H
