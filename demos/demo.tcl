load debug/tkvideo01g.dll

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

#
# Lets use the Tile styling stuff if we can find it :)
#

if {![catch {load [file join $env(TILE_LIBRARY) .. win debug tile01g.dll]}]} {

    tscrollbar .sy -orient vertical -command {vview y}
    tscrollbar .sx -orient horizontal -command {vview x}

    tbutton .buttons.run -text {>} -command {.v start}
    tbutton .buttons.pause -text {||} -command {.v pause}
    tbutton .buttons.props -text {Properties} -command {.v prop filter}
    tcheckbutton .buttons.stretch -text Stretch -variable stretch \
        -command onStretch

    .menu add cascade -label Theme -underline 0 \
        -menu [menu .menu.theme -tearoff 0]
    foreach themeid [lsearch -glob -all [package names] tile::*] {
        set name [lindex [package names] $themeid]
        set name [lindex [split $name :] end]
        .menu.theme add command -label $name -underline 0 \
            -command [list style settheme $name]
    }

} else {

    scrollbar .sy -orient vertical -command {vview y}
    scrollbar .sx -orient horizontal -command {vview x}

    button .buttons.run -text {>} -command {.v start} -width 5
    button .buttons.pause -text {||} -command {.v pause} -width 5
    button .buttons.props -text {Properties} -command {.v prop filter} 
    checkbutton .buttons.stretch -text Stretch -variable stretch \
        -command onStretch

}
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
