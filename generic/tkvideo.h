/* tkvideo.h - Copyright (C) 2005 Pat Thoyts <patthoyts@users.sourceforge.net>
 *
 * --------------------------------------------------------------------------
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 * --------------------------------------------------------------------------
 * $Id$
 */

#ifndef _tkvideo_h_INCLUDE
#define _tkvideo_h_INCLUDE

#include <tk.h>
#include <string.h>

/* Tcl 8.4 CONST support */
#ifndef CONST84
#define CONST84
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
                           /* widget core */
    Tk_Window tkwin;
    Display *display;
    Tcl_Interp *interp;
    Tcl_Command widgetCmd;
    Tk_OptionTable optionTable;

    int      flags;        /* set of flags for the next draw */
    int      stretch;      /* flag */

    Tcl_Obj  *widthPtr;
    int       width;
    Tcl_Obj  *heightPtr;
    int       height;
    Tcl_Obj  *anchorPtr;
    Tk_Anchor anchor;
    Tcl_Obj  *bgPtr;

    int      videoWidth;   /* the width of the video source */
    int      videoHeight;  /* the height of the video source */
    XPoint   offset;       /* the offset into the video window of the */
			   /* visible region */

    Tcl_Obj *xscrollcmdPtr;
    Tcl_Obj *yscrollcmdPtr;

    Tcl_Obj *sourcePtr;
    Tcl_Obj *audioPtr;
    Tcl_Obj *outputPtr;

    Tk_Cursor cursor;      /* support alternate cursor */
    Tcl_Obj *takeFocusPtr; /* used for keyboard traversal */

    ClientData platformData;

} Video;

enum {
    VIDEO_CGET, VIDEO_CONFIGURE, VIDEO_XVIEW, VIDEO_YVIEW, VIDEO_PROPERTYPAGE,
    VIDEO_STOP, VIDEO_START, VIDEO_PAUSE, VIDEO_DEVICES, VIDEO_PICTURE,
    VIDEO_SEEK, VIDEO_TELL
};

int  VideopInit(Tcl_Interp *interp);
int  VideopCreateWidget(Video *videoPtr);
void VideopDestroy(Video *videoPtr);
void VideopCleanup(char *memPtr);
int  VideopWidgetObjCmd(ClientData clientData, Tcl_Interp *interp,
                        int index, int objc, Tcl_Obj *CONST objv[]);
void VideopCalculateGeometry(Video *videoPtr);

int  VideopInitializeSource(Video *videoPtr);
void SendVirtualEvent(Tk_Window targetwin, const char *eventName);
void SendConfigureEvent(Tk_Window tgtWin, int x, int y, int height, int width);

#ifdef __cplusplus
}
#endif

#endif /* _tkvideo_h_INCLUDE */

/*
 * Local variables:
 * indent-tabs-mode: nil
 * End:
 */
