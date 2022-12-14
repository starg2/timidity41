======================================================================
	TiMidity++ -- MIDI-to-WAVE converter and player

					Masanao Izumo
					<iz@onicos.co.jp>
					Mar.01.2004
					version 2.13.0 or later
======================================================================

======================================================================
What is this?
======================================================================

General information

TiMidity++ is a software synthesizer.  It can play MIDI files by con-
verting them into PCM waveform data; give it a MIDI data along with
digital instrument data files, then it synthesizes them in real-time,
and plays.  It can not only play sounds, but also can save the gener-
ated waveforms into hard disks as various audio file formats.

TiMidity++ is a free software, distributed under the terms of GNU gen-
eral public license.

The history

TiMidity++ is based on TiMidity 0.2i, written by Tuukka Toivonen (He
discontinued development because he was too busy with work), released
on 1995.  No new version of this original project is developed since
then.  Development has been continued by Masanao Izumo et al. in the
new project named TiMidity++. `++' is to show the difference from
original project.

======================================================================
Features
======================================================================

* Plays MIDI files without any external MIDI instruments at all
* Understands following formats:
  + SMF (Format 0, 1, 2)
  + MOD
  + RCP, R36, G18, G36 (Recomposer formats)
  + MFi (Version 3; Melody Format for i-Mode)
* Converts MIDI files into various audio file formats:
  + RIFF WAVE (*.wav)
  + SUN AU (*.au)
  + Apple Interchange File Format (*.aiff)
  + Ogg Vorbis (*.ogg)
  + MPEG-1 Audio layer 3 (*.mp3) (note: Windows only)
* Uses following formats as digital instrument data
  + Gravis Ultrasound compatible patch files
  + SoundFonts
  + AIFF and WAV data (Some restrictions are there with AIFF/WAV)
* Displays information about the music that is now playing
* Various user interfaces:
  + dumb terminal interface
  + ncurses interface
  + S-Lang interface
  + X Athena Widget interface
  + Tcl/Tk interface
  + Motif interface (runs with lesstif)
  + vt100 interface
  + Emacs front-end (type ``M-x timidity'' on your emacs)
  + skin interface: can use WinAmp? skin (Seems not maintained...)
  + GTK+ interface
  + ALSA sequencer interface
  + Windows synthesizer interface
  + Windows GUI interface
  + Windows GUI synthesizer interface
  + PortMIDI synthesizer interface
* Plays remote MIDI files over the network
  + HTTP
  + FTP
  + NetNews
* Plays MIDI files in archive files.  Supported formats are:
  + Tar archived (*.tar)
  + Gzip'ed tar (*.tar.gz, *.tgz)
  + Zip compressed (*.zip)
  + LHa compressed lh0, lh1, lh2, lh3, lh4, lh5, lh6, lz4, lzs and lz5 (*.lzh)
* Displays sound spectrogram for the playing music
* Trace playing

======================================================================
Where to get a copy
======================================================================

The latest release of TiMidity++ are available at:

http://www.timidity.jp
  The primary site.
http://timidity.sourceforge.net
  The development goes on this site.  Source codes here.
http://www.asahi-net.or.jp/~gb7t-ngm/timidity/
  Macintosh version
http://timidity.s11.xrea.com/index.en.html
  Windows version

======================================================================
How to install
======================================================================

Refer SETUP.txt file in the distribution.
