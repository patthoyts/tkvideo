# demo.tcl - Copyright (C) 2004 Pat Thoyts <patthoyts@users.sourceforge.net>
#
# Tk video widget demo application.
#
# $Id$

package require Tk
package require tkvideo

# -------------------------------------------------------------------------

# Deal with 'tile' support.
# We sometimes need to _really_ use the Tk widgets at the moment...
#
rename ::label ::tk::label
rename ::radiobutton ::tk::radiobutton
if {![catch {package require tile 0.4}]} {
    namespace import -force tile::*
} else {
    interp alias {} label {} ::tk::label
    interp alias {} radiobutton {} ::tk::radiobutton
}

# -------------------------------------------------------------------------

proc sbset {sb args} {
	puts "$sb set $args"
	eval [list $sb set] $args
}

proc vview {dir args} {
    puts ".v ${dir}view $args"
	eval [list .v ${dir}view] $args
}

proc onStretch {} {
    global stretch
    if {![info exists stretch]} { set stretch 0 }
    .v configure -stretch $stretch
}

proc onFileOpen {} {
    set file [tk_getOpenFile]
    if {$file != {}} {
        puts "set source $file"
        .v configure -source [file nativename $file]
    }
}

proc onSource {source} {
    set sources [.v devices]
    set id [lsearch $sources $source]
    .v configure -source $id
}

proc onExit {} {
    destroy .
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
    foreach theme [style theme names] {
        .menu.themes add radiobutton -label [string totitle $theme] \
            -variable ::Theme \
            -value $theme \
            -command [list SetTheme $theme]
    }
    proc SetTheme {theme} {
        global Theme
        catch {
            style theme use $theme
            set Theme $theme
        }
    }
    if {[tk windowingsystem] == "win32"} {
        SetTheme xpnative
    }
}

scrollbar .sy -orient vertical -command {vview y}
scrollbar .sx -orient horizontal -command {vview x}

button .buttons.run -text {>} -command {.v start} -width 5
button .buttons.pause -text {||} -command {.v pause} -width 5
button .buttons.props -text {Properties} -command {.v prop filter} 
checkbutton .buttons.stretch -text Stretch -variable stretch \
    -command onStretch

pack .buttons.run .buttons.pause .buttons.props .buttons.stretch -side left

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

    set ndx [.menu.file index end]
    .menu.file insert $ndx checkbutton -label Console -underline 0 \
        -variable ::console -command toggleconsole 
}
