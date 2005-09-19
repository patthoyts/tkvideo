# Microsoft Developer Studio Project File - Name="tkvideo" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=TKVIDEO - WIN32 DEBUG
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "tkvideo.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "tkvideo.mak" CFG="TKVIDEO - WIN32 DEBUG"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "tkvideo - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "tkvideo - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "tkvideo - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "tkvideo___Win32_Release"
# PROP BASE Intermediate_Dir "tkvideo___Win32_Release"
# PROP BASE Cmd_Line "NMAKE /f tkvideo.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "tkvideo.exe"
# PROP BASE Bsc_Name "tkvideo.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "tkvideo___Win32_Release"
# PROP Intermediate_Dir "tkvideo___Win32_Release"
# PROP Cmd_Line "nmake -f Makefile.vc INSTALLDIR=c:\opt\tcl OPTS=none all"
# PROP Rebuild_Opt "/a"
# PROP Target_File "Release/tkvideo121.dll"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "tkvideo - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "tkvideo___Win32_Debug"
# PROP BASE Intermediate_Dir "tkvideo___Win32_Debug"
# PROP BASE Cmd_Line "NMAKE /f tkvideo.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "tkvideo.exe"
# PROP BASE Bsc_Name "tkvideo.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "tkvideo___Win32_Debug"
# PROP Intermediate_Dir "tkvideo___Win32_Debug"
# PROP Cmd_Line "nmake -f Makefile.vc INSTALLDIR=c:\opt\tcl OPTS=symbols,msvcrt all"
# PROP Rebuild_Opt "/a"
# PROP Target_File "Debug/tkvideo121g.dll"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "tkvideo - Win32 Release"
# Name "tkvideo - Win32 Debug"

!IF  "$(CFG)" == "tkvideo - Win32 Release"

!ELSEIF  "$(CFG)" == "tkvideo - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\dshow_utils.cpp
# End Source File
# Begin Source File

SOURCE=..\generic\tkvideo.c
# End Source File
# Begin Source File

SOURCE=.\winvideo.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\dshow_utils.h
# End Source File
# Begin Source File

SOURCE=..\generic\tkvideo.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=..\demos\demo.tcl
# End Source File
# Begin Source File

SOURCE=.\makefile.vc
# End Source File
# Begin Source File

SOURCE=.\test.tcl
# End Source File
# End Target
# End Project
