/*
 * song_editor.h - declaration of class songEditor, a window where you can
 *                 setup your songs
 *
 * Copyright (c) 2004-2008 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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


#ifndef _SONG_EDITOR_H
#define _SONG_EDITOR_H

#include "track_container_view.h"

class QLabel;
class QScrollBar;

class automatableSlider;
class comboBox;
class lcdSpinBox;
class meterDialog;
class song;
class textFloat;
class timeLine;
class toolButton;

class positionLine : public QWidget
{
public:
	positionLine( QWidget * _parent );

private:
	virtual void paintEvent( QPaintEvent * _pe );

} ;


class songEditor : public trackContainerView
{
	Q_OBJECT
public:
	songEditor( song * _song, songEditor * & _engine_ptr );
	virtual ~songEditor();


public slots:
	void scrolled( int _new_pos );


private slots:
	void setHighQuality( bool );

	void play( void );
	void record( void );
	void recordAccompany( void );
	void stop( void );

	void masterVolumeChanged( int _new_val );
	void masterVolumePressed( void );
	void masterVolumeMoved( int _new_val );
	void masterVolumeReleased( void );
	void masterPitchChanged( int _new_val );
	void masterPitchPressed( void );
	void masterPitchMoved( int _new_val );
	void masterPitchReleased( void );

	void updatePosition( const midiTime & _t );

	void zoomingChanged( void );


private:
	virtual void keyPressEvent( QKeyEvent * _ke );
	virtual void paintEvent( QPaintEvent * _pe );
	virtual void wheelEvent( QWheelEvent * _we );

	virtual bool allowRubberband( void ) const;


	song * m_s;

	QScrollBar * m_leftRightScroll;

	QWidget * m_toolBar;

	toolButton * m_playButton;
	toolButton * m_recordButton;
	toolButton * m_recordAccompanyButton;
	toolButton * m_stopButton;
	lcdSpinBox * m_tempoSpinBox;

	meterDialog * m_timeSigDisplay;
	automatableSlider * m_masterVolumeSlider;
	automatableSlider * m_masterPitchSlider;

	textFloat * m_mvsStatus;
	textFloat * m_mpsStatus;

	toolButton * m_addBBTrackButton;
	toolButton * m_addSampleTrackButton;
	toolButton * m_addAutomationTrackButton;

	toolButton * m_drawModeButton;
	toolButton * m_editModeButton;

	comboBox * m_zoomingComboBox;

	positionLine * m_positionLine;

	bool m_scrollBack;

} ;



#endif
