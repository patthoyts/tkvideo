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

    int      flags;             /* set of flags for the next draw */
    int      stretch;           /* flag */

    Tcl_Obj *widthPtr;
    int      width;
    Tcl_Obj *heightPtr;
    int height;
    Tcl_Obj *bgPtr;

    int      videoWidth;   /* the width of the video source */
    int      videoHeight;  /* the height of the video source */
    XPoint   offset;  /* the offset into the video window of the visible region */

    Tcl_Obj *xscrollcmdPtr;
    Tcl_Obj *yscrollcmdPtr;

    Tcl_Obj *sourcePtr;

    ClientData platformData;

} Video;

enum {
    VIDEO_CGET, VIDEO_CONFIGURE, VIDEO_XVIEW, VIDEO_YVIEW, VIDEO_PROPERTYPAGE,
    VIDEO_STOP, VIDEO_START, VIDEO_PAUSE, VIDEO_DEVICES,
};

int  VideopInit(Tcl_Interp *interp);
int  VideopCreateWidget(Video *videoPtr);
void VideopDestroy(char *memPtr);
int  VideopWidgetObjCmd(ClientData clientData, Tcl_Interp *interp,
		       int index, int objc, Tcl_Obj *CONST objv[]);
void VideopCalculateGeometry(Video *videoPtr);

int InitVideoSource(Video *videoPtr);

#ifdef __cplusplus
}
#endif

#endif /* _tkvideo_h_INCLUDE */
