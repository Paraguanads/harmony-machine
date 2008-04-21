/*
 * sf2_player.cpp - a soundfont2 player using fluidSynth
 *
 * Copyright (c) 2008 Paul Giblock <drfaygo/at/gmail/dot/com>
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

#include <QTextStream>

#include <QtGui/QLayout>
#include <QtGui/QLabel>
#include <QtGui/QFileDialog>
#include <Qt/QtXml>

#include "sf2_player.h"
#include "engine.h"
#include "instrument_track.h"
#include "instrument_play_handle.h"
#include "note_play_handle.h"
#include "knob.h"
#include "song.h"
#include "automatable_model_templates.h"

#include "main_window.h"
#include "patches_dialog.h"
#include "tooltip.h"
#include "lcd_spinbox.h"

#undef SINGLE_SOURCE_COMPILE
#include "embed.cpp"


extern "C"
{

plugin::descriptor sf2player_plugin_descriptor =
{
	STRINGIFY_PLUGIN_NAME( PLUGIN_NAME ),
	"Sf2Player",
	QT_TRANSLATE_NOOP( "pluginBrowser",
				"SoundFont synthesizer" ),
	"Paul Giblock <drfaygo/at/gmail/dot/com>",
	0x0100,
	plugin::Instrument,
	new QPixmap( PLUGIN_NAME::getIconPixmap( "logo" ) ),
	NULL
} ;

}


// Static map of current sfonts
QMap<QString, sf2Font*> sf2Instrument::s_fonts;



sf2Instrument::sf2Instrument( instrumentTrack * _instrument_track ) :
	instrument( _instrument_track, &sf2player_plugin_descriptor ),
	m_srcState( NULL ),
	m_font( NULL ),
	m_fontId( 0 ),
	m_filename( "" ),
	m_bankNum( -1, -1, 999, 1, this ),
	m_patchNum( -1, -1, 127, 1, this )
{
	for( int i = 0; i < 128; ++i )
	{
		m_notesRunning[i] = 0;
	}

	m_settings = new_fluid_settings();

	fluid_settings_setint( m_settings, "audio.period-size", engine::getMixer()->framesPerPeriod() );

	// This is just our starting instance of synth.  It is recreated
	// everytime we load a new soundfont.
	m_synth = new_fluid_synth( m_settings );

	instrumentPlayHandle * iph = new instrumentPlayHandle( this );
	engine::getMixer()->addPlayHandle( iph );

	updateSampleRate();

	connect( &m_bankNum, SIGNAL( dataChanged() ),
			this, SLOT( updatePatch() ) );

	connect( &m_patchNum, SIGNAL( dataChanged() ),
			this, SLOT( updatePatch() ) );
	
	connect( engine::getMixer(), SIGNAL( sampleRateChanged() ),
			this, SLOT( updateSampleRate() ) );
}



sf2Instrument::~sf2Instrument()
{
	engine::getMixer()->removePlayHandles( getInstrumentTrack() );
	freeFont();
	delete_fluid_synth( m_synth );
	delete_fluid_settings( m_settings );
	if( m_srcState != NULL )
	{
		src_delete( m_srcState );
	}

}



void sf2Instrument::saveSettings( QDomDocument & _doc, QDomElement & _this )
{
	_this.setAttribute( "src", m_filename );
	m_patchNum.saveSettings( _doc, _this, "patch" );
	m_bankNum.saveSettings( _doc, _this, "bank" );
}



void sf2Instrument::loadSettings( const QDomElement & _this )
{
	openFile( _this.attribute( "src" ) );
	m_patchNum.loadSettings( _this, "patch" );
	m_bankNum.loadSettings( _this, "bank" );
}




QString sf2Instrument::nodeName( void ) const
{
	return( sf2player_plugin_descriptor.name );
}




void sf2Instrument::freeFont( void )
{
	QTextStream cout( stdout, QIODevice::WriteOnly );

	m_synthMutex.lock();
	
	if ( m_font != NULL )
	{
		--(m_font->refCount);
	
		// No more references
		if( m_font->refCount <= 0 )
		{
			cout << "Really deleting " << m_filename << endl;

			sf2Instrument::s_fonts.remove( m_filename );
			fluid_synth_sfunload( m_synth, m_fontId, TRUE );

			delete m_font;
		}
		// Just remove our reference
		else
		{
			cout << "un-referencing " << m_filename << endl;

			fluid_synth_remove_sfont( m_synth, m_font->fluidFont );
		}

		m_font = NULL;
	}
	m_synthMutex.unlock();
}



void sf2Instrument::openFile( const QString & _sf2File )
{
	emit fileLoading();

	char * sf2Ascii = qstrdup( qPrintable( _sf2File ) );

	// free reference to soundfont if one is selected
	freeFont();

	m_synthMutex.lock();

	// Increment Reference
	if( s_fonts.contains( _sf2File ) )
	{
		QTextStream cout( stdout, QIODevice::WriteOnly );
		cout << "Using existing reference to " << _sf2File << endl;

		m_font = s_fonts[ _sf2File ];

		m_font->refCount++;

		m_fontId = fluid_synth_add_sfont( m_synth, m_font->fluidFont );		
	}

	// Add to map, if doesn't exist.  
	else
	{
		m_fontId = fluid_synth_sfload( m_synth, sf2Ascii, TRUE );

		if( fluid_synth_sfcount( m_synth ) > 0 )
		{
			// Grab this sf from the top of the stack and add to list
			m_font = new sf2Font( fluid_synth_get_sfont( m_synth, 0 ) );
			s_fonts.insert( _sf2File, m_font );
		}
		else
		{
			// TODO: Couldn't load file!
		}
	}

	m_synthMutex.unlock();

	if( m_fontId >= 0 )
	{
		m_patchNum.setValue( 0 );
		m_bankNum.setValue( 0 );
		m_filename = _sf2File;

		emit fileChanged();
	}

	delete[] sf2Ascii;
}




void sf2Instrument::updatePatch( void )
{
	if( m_bankNum.value() >= 0 && m_patchNum.value() >= 0 )
	{
		fluid_synth_program_select( m_synth, 1, m_fontId,
				m_bankNum.value(), m_patchNum.value() );
	}
}




void sf2Instrument::updateSampleRate( void )
{	
	double tempRate;
	
	// Set & get, returns the true sample rate
	fluid_settings_setnum( m_settings, "synth.sample-rate", engine::getMixer()->sampleRate() );
	fluid_settings_getnum( m_settings, "synth.sample-rate", &tempRate );
	m_internalSampleRate = static_cast<int>( tempRate );

	if( m_font )
	{
		// Now, delete the old one and replace
		m_synthMutex.lock();
		fluid_synth_remove_sfont( m_synth, m_font->fluidFont ); 
		delete_fluid_synth( m_synth );
		
		// New synth
		m_synth = new_fluid_synth( m_settings );
		m_fontId = fluid_synth_add_sfont( m_synth, m_font->fluidFont );		
		m_synthMutex.unlock();
		
		// synth program change (set bank and patch)
		updatePatch();
	}
	else
	{
		// Recreate synth with no soundfonts
		m_synthMutex.lock();
		delete_fluid_synth( m_synth );
		m_synth = new_fluid_synth( m_settings );
		m_synthMutex.unlock();
	}

	if( m_internalSampleRate < engine::getMixer()->sampleRate() )
	{
		m_synthMutex.lock();
		if( m_srcState != NULL )
		{
			src_delete( m_srcState );
		}
		int error;
		m_srcState = src_new( SRC_SINC_MEDIUM_QUALITY,
					DEFAULT_CHANNELS, &error );
		if( m_srcState == NULL || error )
		{
			printf( "error while creating SRC-data-"
				"structure in sf2Instrument::"
				"updateSampleRate()\n" );
		}
		m_synthMutex.unlock();
	}
}




void sf2Instrument::playNote( notePlayHandle * _n, bool, sampleFrame * )
{
	const double LOG440 = 2.643452676486187f;

	const f_cnt_t tfp = _n->totalFramesPlayed();

	int midiNote = (int)floor( 12.0 * ( log2( _n->frequency() ) - LOG440 ) - 4.0 );

	// out of range?
	if( midiNote <= 0 || midiNote >= 128 )
	{
		return;
	}

	if ( tfp == 0 )
	{
		_n->m_pluginData = new int( midiNote );

		m_synthMutex.lock();
		fluid_synth_noteon( m_synth, 1, midiNote, _n->getVolume() );
		m_synthMutex.unlock();

		m_notesRunningMutex.lock();
		++m_notesRunning[midiNote];
		m_notesRunningMutex.unlock();
	}
	else if( _n->released() )
	{
		// Doesn't happen with release frames = 0
	}

}



void sf2Instrument::play( bool _try_parallelizing,
						sampleFrame * _working_buffer )
{
	const fpp_t frames = engine::getMixer()->framesPerPeriod();

	m_synthMutex.lock();

	if( m_internalSampleRate < engine::getMixer()->sampleRate() &&
							m_srcState != NULL )
	{
		const fpp_t f = frames * m_internalSampleRate /
					engine::getMixer()->sampleRate();
		sampleFrame * tmp = new sampleFrame[f];
		fluid_synth_write_float( m_synth, f, tmp, 0, 2, tmp, 1, 2 );

		SRC_DATA src_data;
		src_data.data_in = tmp[0];
		src_data.data_out = _working_buffer[0];
		src_data.input_frames = f;
		src_data.output_frames = frames;
		src_data.src_ratio = (float) frames / f;
		src_data.end_of_input = 0;
		int error = src_process( m_srcState, &src_data );
		delete[] tmp;
		if( error )
		{
			printf( "sf2Instrument: error while resampling: %s\n",
							src_strerror( error ) );
		}
		if( src_data.output_frames_gen > frames )
		{
			printf( "sf2Instrument: not enough frames: %ld / %d\n",
					src_data.output_frames_gen, frames );
		}
	}
	else
	{
		fluid_synth_write_float( m_synth, frames, _working_buffer, 0, 2,
							_working_buffer, 1, 2 );
	}
	m_synthMutex.unlock();

	getInstrumentTrack()->processAudioBuffer( _working_buffer, frames,
									NULL );
}




void sf2Instrument::deleteNotePluginData( notePlayHandle * _n )
{
	int * midiNote = static_cast<int *>( _n->m_pluginData );
	m_notesRunningMutex.lock();
	const int n = --m_notesRunning[*midiNote];
	m_notesRunningMutex.unlock();
	if( n <= 0 )
	{
		m_synthMutex.lock();
		fluid_synth_noteoff( m_synth, 1, *midiNote );
		m_synthMutex.unlock();
	}

	delete midiNote;
}




pluginView * sf2Instrument::instantiateView( QWidget * _parent )
{
	return( new sf2InstrumentView( this, _parent ) );
}






sf2InstrumentView::sf2InstrumentView( instrument * _instrument,
			QWidget * _parent ) :
	instrumentView( _instrument, _parent )
{
	QVBoxLayout * vl = new QVBoxLayout( this );
	QHBoxLayout * hl = new QHBoxLayout();
	
	// File Button
	m_fileDialogButton = new pixmapButton( this, NULL );
	m_fileDialogButton->setCursor( QCursor( Qt::PointingHandCursor ) );
	m_fileDialogButton->setActiveGraphic( embed::getIconPixmap(
			"project_open_down" ) );
	m_fileDialogButton->setInactiveGraphic( embed::getIconPixmap(
			"project_open" ) );
	connect( m_fileDialogButton, SIGNAL( clicked() ),
			this, SLOT( showFileDialog() ) );
	toolTip::add( m_fileDialogButton, tr( "Open other SoundFont file" ) );

	m_fileDialogButton->setWhatsThis(
			tr( "Click here to open another SF2 file" ) );

	// Patch Button
	m_patchDialogButton = new pixmapButton( this, NULL );
	m_patchDialogButton->setCursor( QCursor( Qt::PointingHandCursor ) );
	m_patchDialogButton->setActiveGraphic( embed::getIconPixmap(
							"track_op_menu" ) );
	m_patchDialogButton->setInactiveGraphic( embed::getIconPixmap(
							"track_op_menu" ) );
	m_patchDialogButton->setEnabled( FALSE );

	connect( m_patchDialogButton, SIGNAL( clicked() ),
			this, SLOT( showPatchDialog() ) );
	toolTip::add( m_patchDialogButton, tr( "Choose the patch" ) );


	// LCDs
	m_bankNumLcd = new lcdSpinBox( 3, this, "Bank" );
	m_bankNumLcd->setLabel( "BANK" );
	m_bankNumLcd->addTextForValue( -1, "---" );
	m_bankNumLcd->setEnabled( FALSE );

	m_patchNumLcd = new lcdSpinBox( 3, this, "Patch" );
	m_patchNumLcd->setLabel( "PATCH" );
	m_patchNumLcd->addTextForValue( -1, "---" );
	m_patchNumLcd->setEnabled( FALSE );

	hl->addWidget( m_fileDialogButton );
	hl->addWidget( m_bankNumLcd );
	hl->addWidget( m_patchNumLcd );
	hl->addWidget( m_patchDialogButton );

	vl->addLayout( hl );

	// Next row

	hl = new QHBoxLayout();

	m_filenameLabel = new QLabel( this );

	hl->addWidget( m_filenameLabel );
	vl->addLayout( hl );

	setAutoFillBackground( TRUE );
	QPalette pal;
	pal.setBrush( backgroundRole(), PLUGIN_NAME::getIconPixmap(
								"artwork" ) );
	setPalette( pal );

	updateFilename();
}




sf2InstrumentView::~sf2InstrumentView()
{
}




void sf2InstrumentView::modelChanged( void )
{
	sf2Instrument * k = castModel<sf2Instrument>();
	m_bankNumLcd->setModel( &k->m_bankNum );
	m_patchNumLcd->setModel( &k->m_patchNum );

	connect(k, SIGNAL( fileChanged( void ) ),
			this, SLOT( updateFilename( void ) ) );

	connect(k, SIGNAL( fileLoading( void ) ),
			this, SLOT( invalidateFile( void ) ) );

	updateFilename();

}




void sf2InstrumentView::updateFilename( void )
{
	sf2Instrument * i = castModel<sf2Instrument>();
	m_filenameLabel->setText( "File: " +
					i->m_filename + "\nPatch: TODO" );

	m_patchDialogButton->setEnabled( !i->m_filename.isEmpty() );

	update();
}




void sf2InstrumentView::invalidateFile( void )
{
	m_patchDialogButton->setEnabled( false );
}




void sf2InstrumentView::showFileDialog( void )
{
	sf2Instrument * k = castModel<sf2Instrument>();

	QFileDialog ofd( NULL, tr( "Open SoundFont file" ) );
	ofd.setFileMode( QFileDialog::ExistingFiles );

	QStringList types;
	types << tr( "SoundFont2-Files (*.sf2)" );
	ofd.setFilters( types );

	QString dir;
	if( k->m_filename != "" )
	{
		QString f = k->m_filename;
		if( QFileInfo( f ).isRelative() )
		{
			f = configManager::inst()->userSamplesDir() + f;
			if( QFileInfo( f ).exists() == FALSE )
			{
				f = configManager::inst()->factorySamplesDir() +
						k->m_filename;
			}
		}
		ofd.setDirectory( QFileInfo( f ).absolutePath() );
		ofd.selectFile( QFileInfo( f ).fileName() );
	}
	else
	{
		ofd.setDirectory( configManager::inst()->userSamplesDir() );
	}

	m_fileDialogButton->setEnabled( false );
	
	if( ofd.exec() == QDialog::Accepted && !ofd.selectedFiles().isEmpty() )
	{
		QString f = ofd.selectedFiles()[0];
		if( f != "" )
		{
			k->openFile( f );
			engine::getSong()->setModified();
		}
	}

	m_fileDialogButton->setEnabled( true );
}



// Single instance of the patch dialog
patchesDialog * sf2InstrumentView::s_patchDialog = NULL;

void sf2InstrumentView::showPatchDialog( void )
{
	sf2Instrument * k = castModel<sf2Instrument>();

	if( s_patchDialog == NULL )
	{
		s_patchDialog = new patchesDialog( this );
	}

	s_patchDialog->setup( k->m_synth, 1, k->getInstrumentTrack()->name(),
		&k->m_bankNum, &k->m_patchNum );

	s_patchDialog->exec();
}



extern "C"
{

// neccessary for getting instance out of shared lib
plugin * lmms_plugin_main( model *, void * _data )
{
	return( new sf2Instrument(
			static_cast<instrumentTrack *>( _data ) ) );
}


}

#include "sf2_player.moc"

