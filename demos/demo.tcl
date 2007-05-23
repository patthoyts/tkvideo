# demo.tcl - Copyright (C) 2004 Pat Thoyts <patthoyts@users.sourceforge.net>
#
# Demonstration of the Tkvideo widget.
#
# --------------------------------------------------------------------------
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# --------------------------------------------------------------------------
# $Id$

package require Tk 8.4
package require tkvideo 1.2.0

variable noimg [catch {package require Img}]
variable nottk [catch {package require tile}]

variable uid  ; if {![info exists uid]}  { set uid 0 }
variable init ; if {![info exists init]} { set init 0 }

# data for the button images - 10 16x16 icons in a row.
variable images
variable imgdata {
    R0lGODlhoAAQAMIEAAAAAHd2drGxsbOzs////////////////yH5BAEKAAcA
    LAAAAACgABAAAAP+eLrc/jDKSau9OOvNOwXgAgJeaTYkk4rOqroHnMoorNht
    eOvv6PO10UkDBMZktqJLp3w9jLmVMPibUq0VaPUXFRmx2KNXyuRZoUfceH2t
    Rt1vuIpAr9eR5e8ZPJv6dnl9T2E9bD1beIgoA4yNjjIEAAGTlJGHJFyXgGaY
    f5lcoDiZXYBxcptbi40jqyh2r2SIep6fsn9ptqKjhaVtu7ipKqt3jJCUx5ao
    wWNCcLl+z7q3QYbMy8q/AMN02gOQr3ea2bejIaHQzbSDhL1ppuOyqsUgrXOS
    xwHJzJtU+178uACSk6bGXTtrp7BpaSTAUTFX4LiRMlgtDKdY7fask7A+J4mt
    ddN8EQIQUWIOJyj/1UgppuUOliojdHwXUpOJXBBoqGgxCIXPGD9j7pwwc4jR
    o0iTKl3KtKnTp1AjJAAAOw==
}

proc onStretch {Application} {
    upvar #0 $Application app
    $app(video) configure -stretch $app(stretch)
}

proc onFileOpen {Application} {
    upvar #0 $Application app
    variable nottk
    set file [tk_getOpenFile -defaultextension avi \
                  -filetypes {
                      {"Movie files" {.avi .mpg .mpeg .wmv} {}}
                      {"Audio files" {.wma .mid .rmi .midi} {}}
                      {"Image files" {.jpg .gif .png .bmp}  {}}
                      {"All fles"    {.*}                   {}}}]
    if {$file != {}} {
        SetFileSource $Application $file
    }
}

proc onSource {Application} {
    upvar #0 $Application app
    SetDeviceSource $Application
    catch {$app(video) start}
}

proc Exit {Application} {
    upvar #0 $Application app
    catch {after cancel $app(after)}
    set mw $app(main)
    unset $Application
    destroy $mw
}

proc every {ms body} {uplevel #0 $body; after $ms [info level 0]}

proc RepeatSnap {Application} {
    upvar #0 $Application app
    if {[package provide Img] eq {}} {
        tk_messageBox -message "Feature disabled: Img package not available."
    } else {
        set file [tk_getSaveFile -title "Snapshot image location"]
        if {$file != {}} {
            every 15000 [list RepeatSnapJob $Application $file]
        }
    }
}

proc RepeatSnapJob {Application filename} {
    upvar #0 $Application app
    set img [$app(video) snapshot]
    $img write $filename -format JPEG
    image delete $img
}

proc UpdatePosition {Application} {
    upvar #0 $Application app
    variable nottk
    catch {
        foreach {cur stp dur} [$app(video) tell] break
        set app(position) [expr {$cur / 1000.0}]
        SetScaleLabel $app(slider) $app(sliderlabel) \
            [set Application](poslabel) $cur
        if {!$nottk} {
            $app(slider) configure -value $app(position)
        }
    }
    set app(after) [after 100 [list UpdatePosition $Application]]
}

proc SetScaleLabel {scalew labelw varname value} {
    set $varname [DisplayTime $value]
    foreach {x y} [$scalew coords] break
    foreach {sw sh sx sy} [split [winfo geometry $scalew] {x +}] break
    set tw [winfo width $labelw]
    set x [expr {$x - ($tw / 2)}]
    if {$x < $sx} {set x $sx}
    if {$x + $tw > $sx + $sw} {set x [expr {$sx + $sw - $tw}]}
    place $labelw -x $x -y [expr {$sy - [winfo height $labelw]}]
}

proc DisplayTime {time} {
    set hrs [expr {int($time) / 3600000}]
    set min [expr {(int($time) % 3600000) / 60000}]
    set sec [expr {(int($time) % 60000) / 1000}]
    set ms  [expr {int($time) % 1000}]
    return [format "%02d:%02d:%02d:%03d" $hrs $min $sec $ms]
}


proc Seek {Application value} {
    upvar #0 $Application app
    catch {
        $app(video) seek [expr {int($value * 1000)}]
    }
}

proc Start {Application} {
    upvar #0 $Application app
    variable images
    $app(play) configure -image $images(5) -command [list Pause $Application]
    $app(video) start
}

proc Pause {Application} {
    upvar #0 $Application app
    variable images
    $app(play) configure -image $images(4) -command [list Start $Application]
    $app(video) pause
}

proc Stop {Application} {
    upvar #0 $Application app
    variable images
    $app(play) configure -image $images(4) -command [list Start $Application]
    $app(video) stop
    set app(savefile) {}
}

proc Rewind {Application} {
    upvar #0 $Application app
    $app(video) seek 0
}

proc FastForward {Application} {
    upvar #0 $Application app
    $app(video) seek [lindex [$app(video) tell] 2]
}

proc Skip {Application amount} {
    upvar \#0 $Application app
    foreach {cur stop max} [$app(video) tell] break
    incr cur $amount
    if {$cur < 0} {set cur 0}
    if {$cur > $stop} {set cur $stop}
    $app(video) seek $cur
}

proc Record {Application} {
    upvar #0 $Application app
    set file [tk_getSaveFile -title "Record As" \
                  -defaultextension wmv -filetypes {
                      {"Windows media files" {.wmv} {}}
                      {"Video files"         {.avi} {}}
                      {"MPEG files"          {.mpg} {}}
                  }]
    if {$file != {}} {
        set app(savefile) $file
        SetDeviceSource $Application
    }
}

proc onComplete {Application} {
    Pause $Application
}

proc onConfigure {W x y w h} {
    puts stderr "Configure $W +${x}+${y}+${w}x${h}"
    $W configure -width $w -height $h
}

proc SetFileSource {Application filename} {
    upvar #0 $Application app
    variable nottk
    $app(video) configure -source $filename
    Pause $Application
    foreach {cur stop dur} [$app(video) tell] break
    if {$nottk} {
        $app(slider) configure -state normal
    } else {
        $app(slider) state !disabled
    }
    $app(slider) configure -from 0 -to [expr {$dur/1000.0}]
}

proc SetDeviceSource {Application} {
    upvar #0 $Application app
    variable nottk
    puts stderr "$app(video) configure -source $app(videoindex)\
        -audio $app(audioindex) -output [file nativename $app(savefile)]"
    set vndx [lsearch [$app(video) devices video] $app(videoindex)]
    set andx [lsearch [$app(video) devices audio] $app(audioindex)]
    $app(video) configure -source $vndx -audio $andx \
        -output [file nativename $app(savefile)]
    $app(slider) configure -from 0 -to 0
    if {$nottk} {
        $app(slider) configure -state disabled
    } else {
        $app(slider) state disabled
    }
    Pause $Application
}

# -------------------------------------------------------------------------
#
# Snapshots are displayed in their own dialog window
#
proc Snapshot {Application} {
    upvar #0 $Application app
    set img [$app(video) picture]
    if {$img ne {}} {
        variable uid
        set dlg [toplevel .t[incr uid] -class SnapshotDialog]
        #wm maxsize $dlg [$img cget -width] [$img cget -height]
        label $dlg.im -image $img
        button $dlg.bx -text "Close" -command [list SnapClose $dlg $img]
        button $dlg.bs -text "Save as" -command [list SnapSaveAs $img]
        grid $dlg.im - -sticky news
        grid $dlg.bs $dlg.bx -sticky nse
        grid rowconfigure $dlg 0 -weight 1
        grid columnconfigure $dlg 0 -weight 1
        tkwait visibility $dlg.im
    }
}

proc SnapClose {dlg img} {
    if {[winfo exists $dlg]} {destroy $dlg}
    catch {image delete $img}
}

proc SnapSaveAs {img} {
    set file [tk_getSaveFile \
                  -defaultextension .jpg -filetypes {
                      {"JPEG files"   .jpg {}}
                      {"Bitmap files" .bmp {}}
                      {"GIF files"    .gif {}}
                      {"PNG files"    .png {}}
                      {"PPM files"    .ppm {}}
                      {"All image files" {.jpg .gif .png .ppm .bmp} {}}
                      {"All fles"    {.*} {}}}]
    if {$file != {}} {
        switch -exact -- [set fmt [string tolower [file extension $file]]] {
            .gif { set fmt gif }
            .jpg { set fmt jpeg }
            .png { set fmt png }
            .ppm { set fmt ppm }
            .bmp { set fmt bmp }
            default {
                tk_messageBox -icon error -title "Bad image format" \
                    -message "Unrecognised image format \"$fmt\". The file\
                       type must be one of .jpg, .bmp, .gif, .png or .ppm"
                return
            }
        }
        $img write $file -format $fmt
    }
}

# -------------------------------------------------------------------------
#
# Use Tile widgets if available
#
proc Init {} {
    variable nottk
    variable init
    variable imgdata
    variable images

    if {!$init} {
        font create Btn -family Arial -size 14
        font create Web -family Webdings -size 14
        font create Tim -family Courier -size 8

        set image [image create photo -data $imgdata]
        for {set n 0} {$n < 10} {incr n} {
            set images($n) [image create photo]
            $images($n) copy $image \
                -from [expr {16 * $n}] 0 [expr {16 * ($n + 1)}] 16
        }
        
        if {!$nottk} {
            foreach class {scrollbar scale button checkbutton label} {
                rename ::$class ::tk::$class
                interp alias {} ::$class {} ::ttk::$class
            }
        }
        set init 1
    }
    return
}

proc ::bgerror {msg} {
    tk_messageBox -icon error -title "Application Error" \
	    -message $msg
}

# -------------------------------------------------------------------------
#
# Create the basic widgets and menus.
#
proc Main {mw {filename {}}} {
    variable nottk
    variable uid
    variable images
    set Application [namespace current]::demo[incr uid]
    upvar #0 $Application app
    array set app [list main $mw stretch 1 videoindex 0 \
                       audioindex "No audio" savefile {} \
                       poslabel [DisplayTime 0] ]

    wm title $mw {Tk video widget demo}
    wm iconname $mw {Tkvideo}

    set v [tkvideo $mw.v \
               -stretch $app(stretch) \
               -background SteelBlue \
               -width 320 -height 240]
    set app(video) $v

    bind $v <<VideoPaused>>   { puts stderr "VideoPaused" }
    bind $v <<VideoComplete>> [list onComplete $Application]
    bind $v <<VideoUserAbort>> { puts stderr "VideoUserAbort" }
    bind $v <<VideoErrorAbort>> { puts stderr "VideoErrorAbort [format 0x%08x %s]" }
    bind $v <<VideoRepaint>> { puts stderr "VideoRepaint" }
    bind $v <<VideoDeviceLost>> { puts stderr "VideoDeviceLost" }
    bind $v <Configure> { onConfigure %W %x %y %w %h }
    
    if {$nottk} {
        set buttons [frame $mw.buttons]
    } else {
        set buttons [ttk::frame $mw.buttons]
    }

    set sy [scrollbar $mw.sy  -orient vertical   -command [list $v yview]]
    set sx [scrollbar $mw.sx  -orient horizontal -command [list $v xview]]
    $v configure -xscrollcommand [list $sx set] -yscrollcommand [list $sy set]

    set app(sliderlabel) [label $mw.float \
                              -textvariable [set Application](poslabel)]
    set app(slider) [scale $mw.pos -orient horizontal -from 0 -to 0 \
                         -variable [set Application](position) \
                         -command [list Seek $Application]];#  -showvalue 0
    if {$nottk} {$app(slider) configure -state disabled} else {$app(slider) state disabled}
    
    set app(rewd) [button $buttons.rewind -image $images(2) \
                       -command [list Rewind $Application]]
    set app(back) [button $buttons.back -image $images(0) \
                       -command [list Skip $Application -250]]
    set app(play) [button $buttons.play   -image $images(4) \
                       -command [list Start $Application]]
    set app(fwd)  [button $buttons.fwd  -image $images(1) \
                       -command [list Skip $Application 250]]
    set app(ffwd) [button $buttons.ffwd   -image $images(3) \
                       -command [list FastForward $Application]]
    set app(stop) [button $buttons.stop   -image $images(6) \
                       -command [list Stop $Application]]
    set app(snap) [button $buttons.snap   -image $images(8) \
                       -command [list Snapshot $Application]]
    set app(prop) [button $buttons.props  -image $images(9) \
                       -command [list $v prop filter]]
    checkbutton $buttons.stretch -text Stretch \
        -variable [set Application](stretch) \
        -command [list onStretch $Application]

    pack $app(rewd) $app(back) $app(play) $app(fwd) $app(ffwd) \
        $app(stop) $app(snap) $app(prop) \
        $buttons.stretch -side left
    
    grid $v        $sy   -sticky news
    grid $sx       -     -sticky news
    grid $app(slider) -  -sticky news -pady {20 0}
    grid $buttons  -     -sticky news
    
    grid columnconfigure $mw 0 -weight 1
    grid rowconfigure    $mw 0 -weight 1
    
    $mw configure -menu [set menu [menu $mw.menu]]
    $menu add cascade -label File -underline 0 \
        -menu [menu $menu.file -tearoff 0]
    $menu.file add command -label "Open file..." -underline 0 \
        -command [list onFileOpen $Application]
    $menu.file add cascade -label "Video source" -underline 0 \
        -menu [menu $menu.file.sources -tearoff 0]
    $menu.file add cascade -label "Audio source" -underline 0 \
        -menu [menu $menu.file.audio -tearoff 0]
    $menu.file add separator
    $menu.file add command -label "Record ..." -underline 1 \
        -command [list Record $Application]
    $menu.file add command -label "Snapshot" -underline 1 \
        -command [list Snapshot $Application]
    $menu.file add separator
    $menu.file add command -label Exit -underline 1 \
        -command [list Exit $Application]
    
    #
    # Create a menu for all the available video sources.
    #
    set vdevs [$v devices video]
    foreach name [linsert $vdevs 0 "No video"] {
        $menu.file.sources add radiobutton -label $name \
            -command [list onSource $Application] \
            -variable [set Application](videoindex)
    }

    #
    # Create a menu for all the available audio sources.
    #
    set adevs [$v devices audio]
    foreach name [linsert $adevs 0 "No audio"] {
        $menu.file.audio add radiobutton -label $name \
            -command [list onSource $Application] \
            -variable [set Application](audioindex)
    }

    #
    # If there are no devices, disable the menu items
    #
    if {[llength $vdevs] < 1} {
        if {![catch {set ndx [$menu.file index "Video source"]}]} {
            $menu.file entryconfigure $ndx -state disabled
        }
    }
    if {[llength $adevs] < 1} {
        if {![catch {set ndx [$menu.file index "Audio source"]}]} {
            $menu.file entryconfigure $ndx -state disabled
        }
    }

    #
    # Create a themes menu if tile is loaded
    #
    if {[package provide tile] != {}} {
        $menu add cascade -label "Tk themes" \
            -menu [menu $menu.themes -tearoff 0]
        foreach theme [tile::availableThemes] {
            $menu.themes add radiobutton -label [string totitle $theme] \
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
    
    #
    # Add a console menu item for windows.
    #
    if {[tk windowingsystem] == "win32"} {
        set console 0
        proc toggleconsole {} {
            global console
            if {$console} {console show} else {console hide}
        }
        
        bind $mw <Control-F2> {toggleconsole}
        
        set ndx [$menu.file index end]
        $menu.file insert $ndx checkbutton -label Console -underline 0 \
            -variable ::console -command toggleconsole -accel "Ctrl-F2"
    }

    #
    # Connect the the first webcam or hook up the file and start previewing.
    #
    if {$filename ne ""} {
        SetFileSource $Application $filename
    } elseif {[llength $vdevs] > 0} {
        set app(videoindex) [lindex $vdevs 0]
        set app(audioindex) [lindex $adevs 0]
        SetDeviceSource $Application
        Start $Application
    }
        
    set app(after) [after 100 [list UpdatePosition $Application]]
    tkwait window $mw
    return
}

if {!$tcl_interactive} {
    wm withdraw .
    Init
    Main [toplevel .demo -class TkvideoDemo] [lindex $argv 0]
    exit 0
}

# Local variables:
#   mode: tcl
#   indent-tabs-mode: nil
# End:
