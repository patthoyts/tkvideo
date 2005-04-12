# demo.tcl - Copyright (C) 2004 Pat Thoyts <patthoyts@users.sourceforge.net>
#
# Tk video widget demo application.
#
# $Id$

package require Tk 8
package require tkvideo 1.1.0
package require log
catch {package require Img}

# -------------------------------------------------------------------------

# Deal with 'tile' support.
# We sometimes need to _really_ use the Tk widgets at the moment...
#
rename ::label ::tk::label
rename ::radiobutton ::tk::radiobutton
if {![catch {package require tile 0.5}]} {
    namespace import -force ttk::*
} else {
    interp alias {} label {} ::tk::label
    interp alias {} radiobutton {} ::tk::radiobutton
}

# -------------------------------------------------------------------------

variable uid 0

proc sbset {sb args} {
	#log::log debug "$sb set $args"
	eval [linsert $args 0 $sb set]
}

proc vview {dir args} {
    log::log debug ".v ${dir}view $args"
	eval [linsert $args 0 .v ${dir}view]
}

proc onStretch {} {
    global stretch
    if {![info exists stretch]} { set stretch 0 }
    .v configure -stretch $stretch
}

proc onFileOpen {} {
    set file [tk_getOpenFile]
    if {$file != {}} {
        log::log notice "set source $file"
		.v stop
        .v configure -source [file nativename $file]
    }
}

proc onSource {source} {
    set sources [.v devices]
    set id [lsearch $sources $source]
	.v stop
    .v configure -source $id
}

proc onExit {} {
    destroy .
}

proc Snapshot {w} {
    set img [$w picture]
    if {$img ne {}} {
        variable uid
        set dlg [toplevel .t[incr uid]]
        pack [label $dlg.l]
        tkwait visibility $dlg.l
        $dlg.l configure -image $img
    }
}

proc every {ms body} {eval $body; after $ms [info level 0]}

proc RepeatSnap {w} {
    if {[package provide Img] eq {}} {
        tk_messageBox -message "Feature disabled: Img package not available."
    } else {
        set file [tk_getSaveFile -title "Snapshot image location"]
        if {$file != {}} {
            every 15000 [list RepeatSnapJob $w $file]
        }
    }
}

proc RepeatSnapJob {w filename} {
    set img [$w snapshot]
    $img write $filename -format JPEG
    image delete $img
}

# -------------------------------------------------------------------------

#
# Create the basic widgets and menus.
#

wm title . {Tk video widget demo}

tkvideo .v -bg white \
    -xscrollcommand {sbset .sx} -yscrollcommand {sbset .sy}
frame .buttons

. configure -menu [menu .menu]
.menu add cascade -label File -underline 0 -menu [menu .menu.file -tearoff 0]
.menu.file add command -label "Open file..." -underline 0 -command onFileOpen
.menu.file add cascade -label "Video source" -underline 0 \
    -menu [menu .menu.file.sources -tearoff 0]
.menu.file add separator
.menu.file add command -label "Snapshot ..." -underline 1 \
    -command [list Snapshot .v]
.menu.file add command -label "Repeat snap ..." -underline 0 \
    -command [list RepeatSnap .v]
.menu.file add separator
.menu.file add command -label Exit -underline 1 -command onExit

#
# Create a menu for all the available video sources.
#

foreach name [.v devices] {
    .menu.file.sources add command -label $name \
        -command [list onSource $name]
}

if {[package provide tile] != {}} {
    .menu add cascade -label "Tk themes" -menu [menu .menu.themes -tearoff 0]
    foreach theme [tile::availableThemes] {
        .menu.themes add radiobutton -label [string totitle $theme] \
            -variable ::Theme \
            -value $theme \
            -command [list SetTheme $theme]
    }
    proc SetTheme {theme} {
        global Theme
        catch {
            #style theme use $theme
			tile::setTheme $theme
            set Theme $theme
        }
    }
    if {[tk windowingsystem] == "win32"} {
        SetTheme xpnative
    }
}

scrollbar .sy -orient vertical -command {vview y}
scrollbar .sx -orient horizontal -command {vview x}

font create Btn -family Arial -size 14

button .buttons.run   -text "\u25BA" -width 5 -font Btn -command {.v start}
button .buttons.pause -text "\u25A1" -width 5 -font Btn -command {.v pause}
button .buttons.stop  -text "\u25A0" -width 5 -font Btn -command {.v stop}
button .buttons.snap  -text "\u263A" -width 5 -font Btn -command {Snapshot .v}
button .buttons.props -text {Properties} -command {.v prop filter} 
checkbutton .buttons.stretch -text Stretch -variable stretch \
    -command onStretch

pack .buttons.run .buttons.pause .buttons.stop \
   .buttons.snap .buttons.props .buttons.stretch -side left

grid .v        .sy   -sticky news
grid .sx       -     -sticky news
grid .buttons  -     -sticky news

grid columnconfigure . 0 -weight 1
grid rowconfigure    . 0 -weight 1

#
# Connect the the first webcam and start previewing.
#

.v configure -source 0
.v start

#
# Add a console menu item for windows.
#

if {[tk windowingsystem] == "win32"} {
    set console 0
    proc toggleconsole {} {
        global console
        if {$console} {console show} else {console hide}
    }
	
	bind . <Control-F2> {toggleconsole}

    set ndx [.menu.file index end]
    .menu.file insert $ndx checkbutton -label Console -underline 0 \
        -variable ::console -command toggleconsole -accel "Ctrl-F2"
}
