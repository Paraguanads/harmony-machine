/*
 * side_bar_widget.h - base-class for all side-bar-widgets
 *
 * Copyright (c) 2004-2007 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 * 
 * This file is part of Linux MultiMedia Studio - http://lmms.sourceforge.net
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */


#ifndef _SIDE_BAR_WIDGET_H
#define _SIDE_BAR_WIDGET_H

#include <QtGui/QPixmap>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>


class sideBarWidget : public QWidget
{
	Q_OBJECT
public:
	sideBarWidget( const QString & _title, const QPixmap & _icon,
							QWidget * _parent );
	virtual ~sideBarWidget();
	inline const QPixmap & icon() const
	{
		return( m_icon );
	}
	inline const QString & title() const
	{
		return( m_title );
	}


protected:
	virtual void paintEvent( QPaintEvent * _pe );
	virtual void resizeEvent( QResizeEvent * _re );
	inline virtual void contextMenuEvent( QContextMenuEvent * )
	{
	}

	inline QWidget * contentParent()
	{
		return( m_contents );
	}
	inline void addContentWidget( QWidget * _w )
	{
		m_layout->addWidget( _w );
	}


private:
	QWidget * m_contents;
	QVBoxLayout * m_layout;
	QString m_title;
	QPixmap m_icon;

} ;


#endif
