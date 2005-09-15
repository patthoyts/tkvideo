# demo.tcl - Copyright (C) 2004 Pat Thoyts <patthoyts@users.sourceforge.net>
#
# Tk video widget demo application.
#
# $Id$

package require Tk 8
package require tkvideo 1.1.0
package require log
catch {package require Img}
variable nottk [catch {package require tile}]
variable vdevice -1
variable adevice 0
variable position 0
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
    variable nottk
    variable position
    set file [tk_getOpenFile -defaultextension avi \
                  -filetypes {
                      {"Movie files" {.avi .mpg .mpeg .wmv} {}}
                      {"Audio files" {.wma .mid .rmi .midi} {}}
                      {"Image files" {.jpg .gif .png .bmp}  {}}
                      {"All fles"    {.*}                   {}}}]
    if {$file != {}} {
        log::log notice "set source $file"
        .v configure -source [file nativename $file]
        catch {.v pause}
        foreach {cur stop dur} [.v tell] break
        if {$nottk} {.pos configure -state normal} else {.pos state !disabled}
        .pos configure -from 0 -to [expr {$dur/1000.0}]
        every 100 {UpdatePosition .pos .v}
    }
}

proc onSource {source} {
    variable nottk
    variable position
    set sources [.v devices video]
    set id [lsearch $sources $source]
    .v configure -source $id
    .pos configure -from 0 -to 0
    if {$nottk} {.pos configure -state disabled} else {.pos state disabled}
    catch {.v start; after 200 {.v pause}}
}

proc onAudioSource {source} {
    # Note: setting -audio to -1 or {} means no audio.
    set id [lsearch [.v devices audio] $source]
    .v configure -audio $id
}

proc onExit {} {
    destroy .
}

proc Record {w} {
    set file [tk_getSaveFile -title "Record As" \
                  -defaultextension avi -filetypes {
                      {"Video files"         {.avi} {}}
                      {"Windows media files" {.wmv} {}}
                      {"MPEG files"          {.mpg} {}}
                  }]
    if {$file != {}} {
        $w configure -output [file nativename $file]
    }
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

proc every {ms body} {uplevel #0 $body; after $ms [info level 0]}

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

proc UpdatePosition {w v} {
    variable nottk
    variable position
    catch {
        foreach {cur stp dur} [$v tell] break
        set position [expr {$cur / 1000.0}]
        if {!$nottk} {
            $w configure -value $position
        }
    }
}

proc Seek {w v} {
    catch {
        $w seek [expr {int($v * 1000)}]
    }
}

# -------------------------------------------------------------------------

#
# Create the basic widgets and menus.
#

wm title . {Tk video widget demo}

tkvideo .v -bg white \
    -xscrollcommand {sbset .sx} -yscrollcommand {sbset .sy}

bind .v <<VideoPaused>>   { puts stderr "VideoPaused" }
bind .v <<VideoComplete>> { %W pause }
bind .v <<VideoUserAbort>> { puts stderr "VideoUserAbort" }
bind .v <Configure> { puts stderr "VideoSizeChanged %W +%x+%y+%wx%h" }

if {$nottk} {frame .buttons} else {ttk::frame .buttons}

. configure -menu [menu .menu]
.menu add cascade -label File -underline 0 -menu [menu .menu.file -tearoff 0]
.menu.file add command -label "Open file..." -underline 0 -command onFileOpen
.menu.file add cascade -label "Video source" -underline 0 \
    -menu [menu .menu.file.sources -tearoff 0]
.menu.file add cascade -label "Audio source" -underline 0 \
    -menu [menu .menu.file.audio -tearoff 0]
.menu.file add separator
.menu.file add command -label "Record ..." -underline 1 \
    -command [list Record .v]
.menu.file add command -label "Snapshot ..." -underline 1 \
    -command [list Snapshot .v]
.menu.file add command -label "Repeat snap ..." -underline 0 \
    -command [list RepeatSnap .v]
.menu.file add separator
.menu.file add command -label Exit -underline 1 -command onExit

#
# Create a menu for all the available video sources.
#
foreach name [.v devices video] {
    .menu.file.sources add radiobutton -label $name \
        -command [list onSource $name] -variable vdevice
}

#
# Create a menu for all the available audio sources.
#
foreach name [linsert [.v devices audio] 0 "No audio"] {
    .menu.file.audio add radiobutton -label $name \
        -command [list onAudioSource $name] -variable adevice
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
            tile::setTheme $theme
            set Theme $theme
        }
    }
}

if {!$nottk} {
    foreach class {scrollbar scale button checkbutton} {
        rename ::$class ::tk::$class
        interp alias {} ::$class {} ::ttk::$class
    }
}

scrollbar .sy  -orient vertical -command {vview y}
scrollbar .sx  -orient horizontal -command {vview x}
scale     .pos -orient horizontal -from 0 -to 0 -variable ::position \
               -command [list Seek .v]
if {$nottk} {.pos configure -state disabled} else {.pos state disabled}

font create Btn -family Arial -size 14

button .buttons.run   -text "\u25BA" -width 5 -font Btn -command {.v start}
button .buttons.pause -text "\u25A1" -width 5 -font Btn -command {.v pause}
button .buttons.stop  -text "\u25A0" -width 5 -font Btn -command {.v stop}
button .buttons.snap  -text "\u263A" -width 5 -font Btn -command {Snapshot .v}
button .buttons.props -text {Properties} -command {.v prop filter} 
checkbutton .buttons.stretch -text Stretch -variable stretch -command onStretch

pack .buttons.run .buttons.pause .buttons.stop \
   .buttons.snap .buttons.props .buttons.stretch -side left

grid .v        .sy   -sticky news
grid .sx       -     -sticky news
grid .pos      -     -sticky news
grid .buttons  -     -sticky news

grid columnconfigure . 0 -weight 1
grid rowconfigure    . 0 -weight 1

#
# Connect the the first webcam and start previewing.
#

if {[llength [set vdevs [.v devices video]]] > 0} {
    set vdevice 0
    .v configure -source $vdevice
    catch {.v start}
}

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

# Local variables:
#   mode: tcl
#   indent-tabs-mode: nil
# End:
