#!/usr/local/bin/wishx -f
#
# TkMidity -- Tcl/Tk Interface for TiMidity
#	written by Takashi IWAI
#
# Tk control panel routine
#

#----------------
# initialize global variables
#
proc InitGlobal {} {

    global Stat tk_patchLevel

    if [catch {expr $Stat(new_tcltk) == 0 || $Stat(new_tcltk) == 1}] {
	set Stat(new_tcltk) 0
	if [regexp "(\[0-9\]+\.\[0-9\]+)" $tk_patchLevel cur] {
	    if {$cur >= 4.0} {
		set Stat(new_tcltk) 1
	    }
	}
    }

    # time table and volume
    set Stat(MaxSecs) 0
    set Stat(LastSec) 0
    set Stat(TotalTimeStr) "/ 0:00"
    set Stat(CurVol) 100

    # message lines
    set Stat(CurMsgs) 0
    set Stat(MaxMsgs) 500

    # current status
    set Stat(Playing) 0
    set Stat(Paused) 0
    set Stat(Blinking) 0

    # MIDI file list
    set Stat(CurIdx) -1
    set Stat(MaxFiles) 0
    set Stat(FileList) {}
    set Stat(ShuffleList) {}

    global Config
    # playing mode
    set Config(RepeatPlay) 0
    set Config(ShufflePlay) 0
    set Config(AutoStart) 0
    set Config(AutoExit) 0

    # display configuration
    set Config(Disp:file) 1
    set Config(Disp:time) 1
    set Config(Disp:text) 1
    set Config(Disp:volume) 1
    set Config(Disp:button) 1
}


#----------------
# read a message from stdin
#
proc HandleInput {} {
    global Stat Config

    set mlist [gets stdin]
    set msg [lindex $mlist 0]

    if {$msg == "TIME"} {
	# set total time
	set csecs [expr [lindex $mlist 1] / 100]
	set mins [expr $csecs / 60]
	set secs [expr $csecs % 60]
	set Stat(TotalTimeStr) [format "/ %d:%02d" $mins $secs]
	set Stat(MaxSecs) $csecs
	set tics [expr $csecs / 8]
	set tics [expr (($tics + 4) / 5) * 5]
	.body.time.scale configure -tickinterval $tics -to $csecs
	SetTime 0

    } elseif {$msg == "MVOL"} {
	# set master volume
	SetVolume [lindex $mlist 1]

    } elseif {$msg == "FILE"} {
	# set playing file
	set path [lindex $mlist 1]
	set divs [split $path /]
	set file [lindex $divs [expr [llength $divs] - 1]]
	wm title . "TkMidity: $file"
	wm iconname . "TkMidity: $file"
	AppendMsg "------"

    } elseif {$msg == "LIST"} {
	# set file list
	.body.file.list delete 0 end
	set Stat(MaxFiles) [lindex $mlist 1]
	set Stat(FileList) {}
	for {set i 0} {$i < $Stat(MaxFiles)} {incr i} {
	    set file [gets stdin]
	    .body.file.list insert end $file
	    lappend Stat(FileList) $file
	}
	MakeShuffleList

	set Stat(CurIdx) -1
	SelectNumber 

    } elseif {$msg == "PREV"} {
	# previous file
	set Stat(CurIdx) [expr $Stat(CurIdx) - 1]
	if {$Stat(CurIdx) < 0} {set Stat(CurIdx) 0}
	SelectNumber 

    } elseif {$msg == "NEXT" || $msg == "TEND"} {
	# next file
	incr Stat(CurIdx)
	if {$Stat(CurIdx) >= $Stat(MaxFiles)} {
	    if {$Config(RepeatPlay)} {
		set Stat(CurIdx) 0
	    } elseif {$Config(AutoExit)} {
		QuitCmd
	    } else {
		StopCmd
	    }
	}
	SelectNumber

    } elseif {$msg == "CURT"} {
	# set current time
	SetTime [expr [lindex $mlist 1] / 100]

    } elseif {$msg == "CMSG"} {
	# put message
	set type [lindex $mlist 1]
	set str [gets stdin]
	AppendMsg $str

    } elseif {$msg == "QUIT"} {
	# quit
	exit
    } elseif {$msg == "RSTA"} {
	# restart file
	SelectNumber
    }
}


#----------------
# make shuffled list
#
proc MakeShuffleList {} {
    global Stat
    set tmplist {}
    for {set i 0} {$i < $Stat(MaxFiles)} {incr i} {
	lappend tmplist $i
    }
    set Stat(ShuffleList) {}
    set len $Stat(MaxFiles)
    while {$len > 0} {
	set pos [my-random $len]
	lappend Stat(ShuffleList) [lindex $tmplist $pos]
	set tmplist [lreplace $tmplist $pos $pos]
	lappend Stat(ShuffleList) [lindex $tmplist $pos]
	set len [expr $len - 1]
    }
}

#
# append a string to message buffer
#
proc AppendMsg {str} {
    global Stat

    incr Stat(CurMsgs)
    if {$Stat(CurMsgs) >= $Stat(MaxMsgs)} { ClearMsg }
    .body.text.buf insert end $str\n
    .body.text.buf yview -pickplace end
}

#
# clear message buffer
#
proc ClearMsg {} {
    global Stat
    .body.text.buf delete 0.0 end
    .body.text.buf yview 0
    set Stat(CurMsgs) 0
}


#----------------
# select the file in listbox and load it
#
proc SelectNumber {} {
    global Stat Config
    if {$Stat(new_tcltk)} {
	.body.file.list select clear 0 end
    } else {
	.body.file.list select clear
    }
    set idx -1
    if {$Stat(CurIdx) >= 0 && $Stat(CurIdx) < [llength $Stat(FileList)]} {
	if {$Config(ShufflePlay)} {
	    set idx [lindex $Stat(ShuffleList) $Stat(CurIdx)]
	} else {
	    set idx $Stat(CurIdx)
	}
	set thefile [lindex $Stat(FileList) $idx]
    }
    if {$idx >= 0 && ![file exists $thefile]} {
	warning "Can't open file \"$thefile\"."
	set idx -1
    }

    if {$idx >= 0} {
	if {$Stat(new_tcltk)} {
	    .body.file.list select set $idx
	} else {
	    .body.file.list select from $idx
	    .body.file.list select to $idx
	}
	.body.curfile configure -text\
		"Playing: [lindex $Stat(FileList) $idx]"
	LoadCmd $idx
	set Stat(Playing) 1
    } else {
	SetTime 0
	.body.curfile configure -text "Playing:"
	set Stat(Playing) 0
	set Stat(Paused) 0
    }
    DispButtonPlay
}


#
# update current time
#
proc SetTime {val} {
    global Stat
    if {$val != $Stat(LastSec)} {
	set Stat(LastSec) $val
	set mins [expr $val / 60]
	set secs [expr $val % 60]
	.body.time.label configure\
		-text [format "%d:%02d %s" $mins $secs $Stat(TotalTimeStr)]
	.body.time.scale set $val
	DispButtonPlay
    }
}


#
# colorize buttons with each state
#
proc DispButtonPlay {} {
    global Stat
    if {$Stat(Playing)} {
	if {$Stat(Blinking)} {
	    set color green
	    set Stat(Blinking) 0
	} else {
	    set color red
	    set Stat(Blinking) 1
	}
	.body.button.play configure -fg $color -activeforeground $color
    } else {
	.body.button.play configure -fg black -activeforeground black
    }

    if {$Stat(Playing) && $Stat(Paused)} {
	.body.button.pause configure -fg red -activeforeground red
    } else {
	.body.button.pause configure -fg black -activeforeground black
    }
}    


#
# update current volume
#
proc SetVolume {val} {
    global Stat
    set Stat(CurVol) $val
    .body.volume.label configure -text [format "Volume: %d%%" $val]
    .body.volume.scale set $val
}


#----------------
# write message
# messages are: PREV, NEXT, QUIT, FWRD, BACK, RSET, STOP
#	LOAD\n<filename>, JUMP <time>, VOLM <volume>
#

proc WriteMsg {str} {
    puts stdout $str
    flush stdout
}


#----------------
# callback commands
#

#
# jump to specified time
#
proc JumpCmd {val} {
    global Stat
    if {$val != $Stat(LastSec)} {
	WriteMsg [format "JUMP %d" [expr $val * 100]]
    }
}

#
# change volume amplitude
#
proc VolumeCmd {val} {
    global Stat
    if {$val < 0} {set val 0}
    if {$val > 200} {set val 200}
    if {$val != $Stat(CurVol)} {
	WriteMsg [format "VOLM %d" $val]
    }
}

#
# load the specified file
#
proc LoadCmd {idx} {
    global Stat
    WriteMsg "LOAD"
    WriteMsg [lindex $Stat(FileList) $idx]
}

#
# play the first file
#
proc PlayCmd {} {
    global Stat
    if {$Stat(Playing) == 0} {
	WriteMsg "NEXT"
    }
}

#
# pause music
#
proc PauseCmd {} {
    global Stat
    if {$Stat(Playing)} {
	if {$Stat(Paused)} {
	    set Stat(Paused) 0
	} else {
	    set Stat(Paused) 1
	}
	DispButtonPlay
	WriteMsg "STOP"
    }
}

#
# stop playing
#
proc StopCmd {} {
    global Stat
    if {$Stat(Playing)} {
	WriteMsg "QUIT"
	WriteMsg "XTND"
	set Stat(CurIdx) -1
	SelectNumber
    }
}

#
# quit TkMidity
#
proc QuitCmd {} {
    global Config Stat
    set oldpause $Stat(Paused)
    if {!$oldpause} {PauseCmd}
    if {$Config(AutoExit) || [question "Really Quit TkMidity?" 0]} {
	WriteMsg "QUIT"
	WriteMsg "ZAPP"
	return
    }
    if {!$oldpause} {PauseCmd}
}

#
# play previous file
#
proc PrevCmd {} {
    global Stat
    if {$Stat(Playing)} {
	WriteMsg "PREV"
    }
}

#
# play next file
#
proc NextCmd {} {
    global Stat
    if {$Stat(Playing)} {
	WriteMsg "NEXT"
    }
}

proc VolUpCmd {} {
    global Stat
    if {$Stat(Playing)} {
	VolumeCmd [expr $Stat(CurVol) + 10]
    }
}

proc VolDownCmd {} {
    global Stat
    if {$Stat(Playing)} {
	VolumeCmd [expr $Stat(CurVol) - 10]
    }
}

#----------------
# display configured tables
#
proc DispTables {} {
    global Config
    set allitems {file time text volume button}

    foreach i $allitems {
	pack forget .body.$i
	if {$Config(Disp:$i)} {
	    pack .body.$i -side top -fill x
	} 
    }
}

#
# save configuration and playing mode
#
proc SaveConfig {} {
    global Config ConfigFile
    set fd [open $ConfigFile w]
    if {$fd != ""} {
	puts $fd "global Config"
	foreach i [array names Config] {
	    puts $fd "set Config($i) $Config($i)"
	}
	close $fd
    }
}

#
# load configuration file
#
proc LoadConfig {} {
    global ConfigFile
    catch {source $ConfigFile}
}

#
# selection callback of the playing file from listbox
#
proc SelectList {lw pos} {
    global Stat
    set idx [$lw nearest $pos]
    if {$idx >= 0 && $idx < $Stat(MaxFiles)} {
	set Stat(CurIdx) $idx
	set Stat(Playing) 1
	SelectNumber
    }
}
    

#
#
#
proc OpenFiles {} {
    global Stat
    set files [filebrowser .browser "" "*.mid*"]
    if {$files != ""} {
	set Stat(MaxFiles) [expr $Stat(MaxFiles) + [llength $files]]
	foreach i $files {
	    .body.file.list insert end $i
	    lappend Stat(FileList) $i
	}
	MakeShuffleList
    }
}

#
#
#
proc CloseFiles {} {
    global Stat
    if {[question "Really Clear List?" 0]} {
	StopCmd
	.body.file.list delete 0 end
	set Stat(MaxFiles) 0
	set Stat(FileList) {}
	set Stat(SuffleList) {}
    }
}

#----------------
# create main window
#

proc CreateWindow {} {
    global Config Stat

    # menu bar
    frame .menu -relief raised -bd 1
    pack .menu -side top -expand 1 -fill x

    # File menu
    menubutton .menu.file -text "File" -menu .menu.file.m\
	    -underline 0
    menu .menu.file.m
    .menu.file.m add command -label "Open Files" -underline 0\
	    -command OpenFiles
    .menu.file.m add command -label "Clear List" -underline 0\
	    -command CloseFiles
    .menu.file.m add command -label "Save Config" -underline 0\
	    -command SaveConfig
    .menu.file.m add command -label "About" -underline 0\
	    -command {
	information "TkMidity -- TiMidty Tcl/Tk Version\n  written by T.IWAI"
    }
    .menu.file.m add command -label "Quit" -underline 0\
	    -command QuitCmd

    # Mode menu
    menubutton .menu.mode -text "Mode" -menu .menu.mode.m\
	    -underline 0
    menu .menu.mode.m
    .menu.mode.m add check -label "Repeat" -underline 0\
	    -variable Config(RepeatPlay)
    .menu.mode.m add check -label "Shuffle" -underline 0\
	    -variable Config(ShufflePlay) -command "MakeShuffleList"
    .menu.mode.m add check -label "Auto Start" -underline 5\
	    -variable Config(AutoStart)
    .menu.mode.m add check -label "Auto Exit" -underline 5\
	    -variable Config(AutoExit)

    # Displays menu
    menubutton .menu.disp -text "Displays" -menu .menu.disp.m\
	    -underline 0
    menu .menu.disp.m
    .menu.disp.m add check -label "File List" -underline 0\
	    -variable Config(Disp:file) -command "DispTables"
    .menu.disp.m add check -label "Time" -underline 0\
	    -variable Config(Disp:time) -command "DispTables"
    .menu.disp.m add check -label "Messages" -underline 0\
	    -variable Config(Disp:text) -command "DispTables"
    .menu.disp.m add check -label "Volume" -underline 0\
	    -variable Config(Disp:volume) -command "DispTables"
    .menu.disp.m add check -label "Buttons" -underline 0\
	    -variable Config(Disp:button) -command "DispTables"

    pack .menu.file .menu.mode .menu.disp -side left

    # display body
    if {$Stat(new_tcltk)} {
	frame .body -relief flat
    } else {
	frame .body -relief raised -bd 1 
    }
    pack .body -side top -expand 1 -fill both

    # current playing file
    label .body.curfile -text "Playing:" -relief ridge
    pack .body.curfile -side top -expand 1 -fill x

    # playing files list
    frame .body.file -relief raised -bd 1
    scrollbar .body.file.bar -relief sunken\
	    -command ".body.file.list yview"
    pack .body.file.bar -side right -fill y
    if {$Stat(new_tcltk)} {
	listbox .body.file.list -width 40 -height 10 -relief sunken -bd 2\
		-yscroll ".body.file.bar set"
    } else {
	listbox .body.file.list -geometry 40x10 -relief sunken -bd 2\
		-yscroll ".body.file.bar set"
    }
    bind .body.file.list <Button-1> {SelectList %W %y}

    pack .body.file.list -side top -expand 1 -fill both

    # time label and scale
    frame .body.time -relief raised -bd 1
    label .body.time.label -text "0:00 / 0:00"
    pack .body.time.label -side top
    scale .body.time.scale -orient horizontal -length 280\
	    -from 0 -to 100 -tickinterval 10
    bind .body.time.scale <ButtonRelease-1> {JumpCmd [%W get]}
    pack .body.time.scale -side bottom -expand 1 -fill x

    # text browser
    frame .body.text -relief raised -bd 1
    scrollbar .body.text.bar -relief sunken\
	    -command ".body.text.buf yview"
    pack .body.text.bar -side right -fill y
    text .body.text.buf -width 40 -height 12 -relief sunken -bd 2\
	    -wrap word -yscroll ".body.text.bar set"
    bind .body.text.buf <Button-1> { }
    bind .body.text.buf <Any-Key> { }
    pack .body.text.buf -side top -expand 1 -fill both
    button .body.text.clear -text "Clear"\
	    -command ClearMsg
    pack .body.text.clear -side bottom

    # volume label and scale
    frame .body.volume -relief raised -bd 1
    label .body.volume.label -text "Volume:"
    pack .body.volume.label -side top
    scale .body.volume.scale -orient horizontal -length 280\
	    -from 0 -to 200 -tickinterval 25
    bind .body.volume.scale <ButtonRelease-1> {VolumeCmd [%W get]}
    pack .body.volume.scale -side bottom -expand 1 -fill x

    # buttons
    global bitmap_path
    frame .body.button -relief raised -bd 1
    button .body.button.play -bitmap @$bitmap_path/play.xbm\
	    -command "PlayCmd"
    button .body.button.stop -bitmap @$bitmap_path/stop.xbm\
	    -command "StopCmd"
    button .body.button.prev -bitmap @$bitmap_path/prev.xbm\
	    -command "PrevCmd"
    button .body.button.next -bitmap @$bitmap_path/next.xbm\
	    -command "NextCmd"
    button .body.button.pause -bitmap @$bitmap_path/pause.xbm\
	    -command "PauseCmd"
    button .body.button.quit -bitmap @$bitmap_path/quit.xbm\
	    -command "QuitCmd"
    pack .body.button.play .body.button.stop\
	    .body.button.prev .body.button.next\
	    .body.button.pause .body.button.quit\
	    -side left -padx 5 -pady 5

    # pack all items
    DispTables

    focus .
    tk_menuBar .menu .menu.file .menu.mode .menu.disp
    bind . <Key-Right> "NextCmd"
    bind . <Key-n> "NextCmd"
    bind . <Key-Left> "PrevCmd"
    bind . <Key-p> "PrevCmd"
    bind . <Key-s> "PauseCmd"
    bind . <Key-Down> "VolDownCmd"
    bind . <Key-v> "VolDownCmd"
    bind . <Key-Up> "VolUpCmd"
    bind . <Key-V> "VolUpCmd"
    bind . <Key-space> "PauseCmd"
    bind . <Return> "PlayCmd"
    bind . <Key-c> "StopCmd"
    bind . <Key-q> "QuitCmd"
}
