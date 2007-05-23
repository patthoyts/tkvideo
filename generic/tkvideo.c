/* tkvideo.c - Copyright (C) 2005 Pat Thoyts <patthoyts@users.sourceforge.net>
 *
 * --------------------------------------------------------------------------
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 * --------------------------------------------------------------------------
 * $Id$
 */

#include "tkvideo.h"
#include <stdio.h>

/* ---------------------------------------------------------------------- */
#define DEF_VIDEO_BACKGROUND   "SystemButtonFace"
#define DEF_VIDEO_WIDTH        "0"
#define DEF_VIDEO_HEIGHT       "0"
#define DEF_VIDEO_SOURCE       ""
#define DEF_VIDEO_SCROLL_CMD   ""
#define DEF_VIDEO_STRETCH      "0"
#define DEF_VIDEO_CURSOR       ""
#define DEF_VIDEO_TAKE_FOCUS   "0"
#define DEF_VIDEO_OUTPUT       ""
#define DEF_VIDEO_ANCHOR       "center"

#define VIDEO_SOURCE_CHANGED   0x01
#define VIDEO_GEOMETRY_CHANGED 0x02
#define VIDEO_OUTPUT_CHANGED   0x04

static Tk_OptionSpec videoOptionSpec[] = {
    {TK_OPTION_ANCHOR, "-anchor", "anchor", "Anchor",
        DEF_VIDEO_ANCHOR, Tk_Offset(Video, anchorPtr), -1, 0, 0, VIDEO_GEOMETRY_CHANGED },
    {TK_OPTION_STRING, "-audiosource", "audiosource", "AudioSource",
        DEF_VIDEO_SOURCE, Tk_Offset(Video, audioPtr), -1, 0, 0, VIDEO_SOURCE_CHANGED },
    {TK_OPTION_SYNONYM, "-bg", (char *) NULL, (char *) NULL,
        (char *) NULL, 0, -1, 0, (ClientData) "-background"},
    {TK_OPTION_BORDER, "-background", "background", "Background",
        DEF_VIDEO_BACKGROUND, Tk_Offset(Video, bgPtr), -1, 0, 0, 0},
    {TK_OPTION_CURSOR, "-cursor", "cursor", "Cursor",
        DEF_VIDEO_CURSOR, -1, Tk_Offset(Video, cursor),
        TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_STRING, "-height", "height", "Height",
        DEF_VIDEO_HEIGHT, Tk_Offset(Video, heightPtr), -1, 0, 0, VIDEO_GEOMETRY_CHANGED},
    {TK_OPTION_STRING, "-output", "output", "Output",
        DEF_VIDEO_OUTPUT, Tk_Offset(Video, outputPtr), -1, 0, 0, VIDEO_OUTPUT_CHANGED },
    {TK_OPTION_STRING, "-source", "source", "Source",
        DEF_VIDEO_SOURCE, Tk_Offset(Video, sourcePtr), -1, 0, 0, VIDEO_SOURCE_CHANGED },
    {TK_OPTION_BOOLEAN, "-stretch", "stretch", "Stretch",
        DEF_VIDEO_STRETCH, -1, Tk_Offset(Video, stretch), 0, 0, 0 },
    {TK_OPTION_STRING, "-takefocus", "takeFocus", "TakeFocus",
        DEF_VIDEO_TAKE_FOCUS, Tk_Offset(Video, takeFocusPtr), -1,
        TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_STRING, "-width", "width", "Width",
        DEF_VIDEO_WIDTH, Tk_Offset(Video, widthPtr), -1, 0, 0, VIDEO_GEOMETRY_CHANGED},
    {TK_OPTION_STRING, "-xscrollcommand", "xScrollCommand", "ScrollCommand",
        DEF_VIDEO_SCROLL_CMD, Tk_Offset(Video, xscrollcmdPtr), -1,
        TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_STRING, "-yscrollcommand", "yScrollCommand", "ScrollCommand",
        DEF_VIDEO_SCROLL_CMD, Tk_Offset(Video, yscrollcmdPtr), -1,
        TK_OPTION_NULL_OK, 0, 0},
    {TK_OPTION_END, (char *)NULL, (char *)NULL, (char*)NULL,
        (char *)NULL, 0, 0, 0, 0}
};

/* ---------------------------------------------------------------------- */

/*
 *  Flag bits for the video widget
 */

#define REDRAW_PENDING   0x01
#define UPDATE_V_SCROLL  0x02
#define UPDATE_H_SCROLL  0x04

/* ---------------------------------------------------------------------- */

static int VideoObjCmd(ClientData clientData, Tcl_Interp *interp, 
    int objc, Tcl_Obj *CONST objv[]);
static int VideoWidgetObjCmd(ClientData clientData, Tcl_Interp *interp, 
    int objc, Tcl_Obj *CONST objv[]);
static void VideoDeletedProc(ClientData clientData);
static void VideoCleanup(char *memPtr);
static void VideoDisplay(ClientData clientData);
static void VideoObjEventProc(ClientData clientData, XEvent *evPtr);
static int  VideoConfigure(Tcl_Interp *interp, Video *videoPtr, int objc, Tcl_Obj *CONST objv[]);
static int  VideoWidgetCgetCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
static int  VideoWidgetConfigureCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
static int  VideoWidgetXviewCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
static int  VideoWidgetYviewCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
static int  VideoWorldChanged(ClientData clientData);
static void VideoCalculateGeometry(Video *videoPtr);
static void VideoUpdateVScrollbar(Video* videoPtr);
static void VideoUpdateHScrollbar(Video* videoPtr);

/* ---------------------------------------------------------------------- */

struct Ensemble {
    const char *name;          /* subcommand name */
    Tcl_ObjCmdProc *command;   /* subcommand implementation OR */
    struct Ensemble *ensemble; /* subcommand ensemble */
};

struct Ensemble VideoWidgetEnsemble[] = {
    { "configure", VideoWidgetConfigureCmd, NULL },
    { "cget",      VideoWidgetCgetCmd, NULL },
    { "xview",     VideoWidgetXviewCmd, NULL },
    { "yview",     VideoWidgetYviewCmd, NULL },
    { NULL, NULL, NULL }
};

/* ---------------------------------------------------------------------- */

int DLLEXPORT
Tkvideo_Init(Tcl_Interp *interp)
{
    int r = TCL_OK;
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL)
        return TCL_ERROR;
#endif
#ifdef USE_TK_STUBS
    if (Tk_InitStubs(interp, TK_VERSION, 0) == NULL)
        return TCL_ERROR;
#endif
    r = VideopInit(interp);
    if (r == TCL_OK) {
        Tcl_CreateObjCommand(interp, "tkvideo", VideoObjCmd, NULL, NULL);
        r = Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_VERSION); 
    }
    return r;
}

/* ---------------------------------------------------------------------- */

static int 
VideoObjCmd(ClientData clientData, Tcl_Interp *interp, 
            int objc, Tcl_Obj *CONST objv[])
{
    Video *videoPtr;
    Tk_Window tkwin;
    Tk_OptionTable optionTable;
    int r = TCL_OK, flags = 0;

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "pathName ?options?");
        return TCL_ERROR;
    }

    tkwin = Tk_CreateWindowFromPath(interp, Tk_MainWindow(interp),
        Tcl_GetStringFromObj(objv[1], NULL), (char *)NULL);
    if (tkwin == NULL) {
        return TCL_ERROR;
    }

    Tk_SetClass(tkwin, "Video");

    optionTable = Tk_CreateOptionTable(interp, videoOptionSpec);

    videoPtr = (Video *)ckalloc(sizeof(Video));
    memset(videoPtr, 0, sizeof(Video));
    
    videoPtr->tkwin = tkwin;
    videoPtr->display = Tk_Display(tkwin);
    videoPtr->interp = interp;
    videoPtr->widgetCmd = Tcl_CreateObjCommand(interp, 
        Tk_PathName(videoPtr->tkwin), VideoWidgetObjCmd, (ClientData)videoPtr,
        VideoDeletedProc);
    videoPtr->optionTable = optionTable;
    videoPtr->platformData = (ClientData)NULL;
    videoPtr->offset.x = 0;
    videoPtr->offset.y = 0;
    videoPtr->cursor = None;
    videoPtr->takeFocusPtr = NULL;

    if (Tk_InitOptions(interp, (char *)videoPtr, optionTable, tkwin) 
        != TCL_OK) {
        Tk_DestroyWindow(videoPtr->tkwin);
        ckfree((char *)videoPtr);
        return TCL_ERROR;
    }

    Tk_CreateEventHandler(videoPtr->tkwin, ExposureMask | StructureNotifyMask,
        VideoObjEventProc, (ClientData)videoPtr);
    if (r == TCL_OK)
        r = VideopCreateWidget(videoPtr);
    if (r == TCL_OK)
        r = VideoConfigure(interp, videoPtr, objc - 2, objv + 2);
    if (r == TCL_OK)
        Tcl_SetObjResult(interp, 
            Tcl_NewStringObj(Tk_PathName(videoPtr->tkwin), -1));
    else
        Tk_DestroyWindow(videoPtr->tkwin);

    return r;
}

/* ---------------------------------------------------------------------- */

/*
 * Process the ensemble structure and any commands not found are passed along to the platform code.
 */
static int
VideoWidgetObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    struct Ensemble *ensemble = VideoWidgetEnsemble;
    int optPtr = 1;
    int index;

    while (optPtr < objc) {
        if (Tcl_GetIndexFromObjStruct(interp, objv[optPtr], ensemble, sizeof(ensemble[0]), "command", 0, &index) != TCL_OK)
        {
            if (optPtr == 1)
                return VideopWidgetObjCmd(clientData, interp, objc, objv);
            else
                return TCL_ERROR;
        }

        if (ensemble[index].command) {
            return ensemble[index].command(clientData, interp, objc, objv);
        }
        ensemble = ensemble[index].ensemble;
        ++optPtr;
    }
    Tcl_WrongNumArgs(interp, optPtr, objv, "option ?arg arg...?");
    return TCL_ERROR;
}

static int
VideoWidgetCgetCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    Video *videoPtr = (Video *)clientData;
    Tk_Window tkwin = videoPtr->tkwin;
    Tcl_Obj *resultPtr = NULL;
    int r = TCL_OK;

    Tcl_Preserve(clientData);

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "option");
        r = TCL_ERROR;
    } else {
        resultPtr = Tk_GetOptionValue(interp, (char *)videoPtr, 
            videoPtr->optionTable, objv[2], tkwin);
        if (resultPtr == NULL)
            r = TCL_ERROR;
        else
            Tcl_SetObjResult(interp, resultPtr);
    }
    
    Tcl_Release(clientData);
    return r;
}

static int
VideoWidgetConfigureCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    Video *videoPtr = (Video *)clientData;
    Tk_Window tkwin = videoPtr->tkwin;
    Tcl_Obj *resultPtr = NULL;
    int r = TCL_OK;

    Tcl_Preserve(clientData);

    if (objc < 4) {
        Tcl_Obj *optionPtr = NULL;
        if (objc == 3)
            optionPtr = objv[2];
        resultPtr = Tk_GetOptionInfo(interp, (char *)videoPtr,
            videoPtr->optionTable, (Tcl_Obj*)optionPtr, tkwin);
        r = (resultPtr != NULL) ? TCL_OK : TCL_ERROR;
    } else {
        r = VideoConfigure(interp, videoPtr, objc - 2, objv + 2);
    }            
    if (resultPtr != NULL)
        Tcl_SetObjResult(interp, resultPtr);

    Tcl_Release(clientData);
    return r;
}

/* ---------------------------------------------------------------------- */

static int
VideoConfigure(Tcl_Interp *interp, Video *videoPtr,
    int objc, Tcl_Obj *CONST objv[])
{
    Tk_Window tkwin = videoPtr->tkwin;
    Tk_SavedOptions savedOptions;
    Tk_3DBorder bg;
    int flags = 0, r = TCL_OK;

    r = Tk_SetOptions(interp, (char *)videoPtr,
        videoPtr->optionTable, objc, objv,
        videoPtr->tkwin, &savedOptions, &flags);
    if (r == TCL_OK)
        r = VideoWorldChanged((ClientData) videoPtr);
    else
        Tk_RestoreSavedOptions(&savedOptions);
    Tk_FreeSavedOptions(&savedOptions);

    if (r == TCL_OK)
        r = Tk_GetAnchorFromObj(videoPtr->interp, videoPtr->anchorPtr, &videoPtr->anchor);

    if (r == TCL_OK) {
        bg = Tk_Get3DBorderFromObj(tkwin, videoPtr->bgPtr);
        Tk_SetWindowBackground(tkwin, Tk_3DBorderColor(bg)->pixel);

        if (Tk_GetPixelsFromObj(interp, tkwin, videoPtr->widthPtr, &videoPtr->width) != TCL_OK) {
            Tcl_AddErrorInfo(interp, "\n    (processing -width option)");
        }
        if (Tk_GetPixelsFromObj(interp, tkwin, videoPtr->heightPtr, &videoPtr->height) != TCL_OK) {
            Tcl_AddErrorInfo(interp, "\n    (processing -height option)");
        }

        if (flags & VIDEO_SOURCE_CHANGED || flags & VIDEO_OUTPUT_CHANGED) {
            if (!Tk_IsMapped(tkwin)) {
                Tk_MakeWindowExist(tkwin);
            }
            VideopInitializeSource(videoPtr);
        }

        VideoCalculateGeometry(videoPtr);

        r = VideoWorldChanged((ClientData) videoPtr);
    }
    return r;
}

/*
 *---------------------------------------------------------------------------
 *
 * VideoCalculateGeometry --
 *
 *      ?
 */
static void
VideoCalculateGeometry(Video *videoPtr)
{
    int width, height, maxoff_x, maxoff_y, visible_x, visible_y;

    width  = (videoPtr->width > 0)  ? videoPtr->width  : videoPtr->videoWidth;
    height = (videoPtr->height > 0) ? videoPtr->height : videoPtr->videoHeight;
    Tk_GeometryRequest(videoPtr->tkwin, width, height);

    // If the visible region is smaller than the video size then set
    // offset to the difference. If there is enough space to show the
    // video then offset is 0.
    visible_x = Tk_Width(videoPtr->tkwin);
    maxoff_x = videoPtr->videoWidth - visible_x;
    if (videoPtr->offset.x > maxoff_x) videoPtr->offset.x = maxoff_x;
    if (videoPtr->offset.x < 0) videoPtr->offset.x = 0;

    visible_y = Tk_Height(videoPtr->tkwin);
    maxoff_y = videoPtr->videoHeight - visible_y;
    if (videoPtr->offset.y > maxoff_y) videoPtr->offset.y = maxoff_y;
    if (videoPtr->offset.y < 0) videoPtr->offset.y = 0;

    videoPtr->flags |= UPDATE_V_SCROLL | UPDATE_H_SCROLL;

    VideopCalculateGeometry(videoPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * VideoWorldChanged --
 *
 *      This procedure is called when the world has changed in some
 *      way and the widget needs to recompute all its graphics contexts
 *        and determine its new geometry.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Video widget will be redisplayed.
 *
 *---------------------------------------------------------------------------
 */

static int
VideoWorldChanged(ClientData clientData)
{
    Video *videoPtr = (Video *)clientData;
    Tk_Window tkwin = videoPtr->tkwin;
    Tcl_Interp *interp = videoPtr->interp;

    if (!(videoPtr->flags & REDRAW_PENDING)) {
        Tcl_DoWhenIdle(VideoDisplay, (ClientData)videoPtr);
        videoPtr->flags |= REDRAW_PENDING;
    }
    
    return TCL_OK;
}

/* ---------------------------------------------------------------------- */

static void
VideoObjEventProc(ClientData clientData, XEvent *eventPtr)
{
    Video *videoPtr = (Video *)clientData;
    
    if (eventPtr->type == Expose) {

        if (!(videoPtr->flags & REDRAW_PENDING)) {
            Tcl_DoWhenIdle(VideoDisplay, clientData);
            videoPtr->flags |= REDRAW_PENDING;
        }

    } else if (eventPtr->type == ConfigureNotify) {

        VideoCalculateGeometry(videoPtr);
        VideoWorldChanged((ClientData) videoPtr);

    } else if (eventPtr->type == DestroyNotify) {

        if (videoPtr->tkwin != NULL) {
            VideopDestroy(videoPtr);
            Tk_FreeConfigOptions((char *)videoPtr, videoPtr->optionTable,
                videoPtr->tkwin);
            videoPtr->tkwin = NULL;
            Tcl_DeleteCommandFromToken(videoPtr->interp, videoPtr->widgetCmd);
        }
        if (videoPtr->flags & REDRAW_PENDING) {
            Tcl_CancelIdleCall(VideoDisplay, clientData);
            videoPtr->flags &= ~REDRAW_PENDING;
        }
        Tcl_EventuallyFree(clientData, VideoCleanup);

    }
}

/* ---------------------------------------------------------------------- */

static void
VideoDeletedProc(ClientData clientData)
{
    Video *videoPtr = (Video *)clientData;
    Tk_Window tkwin = videoPtr->tkwin;

    if (tkwin != NULL) {
        Tk_DestroyWindow(tkwin);
    }
}

static void
VideoCleanup(char *memPtr)
{
    VideopCleanup(memPtr);
    ckfree(memPtr);
}

/* ---------------------------------------------------------------------- */

static void
VideoDisplay(ClientData clientData)
{
    Video *videoPtr = (Video *)clientData;
    Tk_Window tkwin = videoPtr->tkwin;
    Drawable d = Tk_WindowId(tkwin);
    Tk_3DBorder bg;


    videoPtr->flags &= ~REDRAW_PENDING;
    if (!Tk_IsMapped(tkwin)) {
        return;
    }

    if (videoPtr->flags & UPDATE_V_SCROLL) {
        VideoUpdateVScrollbar(videoPtr);
    }

    if (videoPtr->flags & UPDATE_H_SCROLL) {
        VideoUpdateHScrollbar(videoPtr);
    }

    bg = Tk_Get3DBorderFromObj(tkwin, videoPtr->bgPtr);

    Tk_Fill3DRectangle(tkwin, d, bg, 0, 0,
        Tk_Width(tkwin), Tk_Height(tkwin), 0, TK_RELIEF_FLAT);
}

/*
 *----------------------------------------------------------------------
 *
 * VideoYviewSubCmd --
 *
 *        Process the video widget's "yview" subcommand.
 *
 * Results:
 *        Standard Tcl result.
 *
 * Side effects:
 *        May change the listbox viewing area; may set the interpreter's result.
 *
 *----------------------------------------------------------------------
 */

static int
VideoWidgetYviewCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    Video *videoPtr = (Video *)clientData;
    Tk_Window tkwin = videoPtr->tkwin;
    int count, type, offset = -1, r = TCL_OK;
    double fraction, fraction2;

    if (objc == 2) {
        char buf[TCL_DOUBLE_SPACE * 2];

        if (videoPtr->stretch) {
            fraction = 0.0;
            fraction2 = 1.0;
        } else {
            fraction = (double)videoPtr->offset.y / (double)videoPtr->videoHeight;
            fraction2 = (double)(videoPtr->offset.y + Tk_Height(tkwin)) / (double)videoPtr->videoHeight;
        }
        if (fraction < 0.0)
            fraction = 0.0;
        if (fraction2 > 1.0) {
            fraction2 = 1.0;
        }
        sprintf(buf, "%g %g", fraction, fraction2);
        Tcl_SetObjResult(interp, Tcl_NewStringObj(buf, -1));
        r = TCL_OK;

    } else if (objc == 3) {
        // display beginning at Tcl_GetDoubleFromObj(objv[2], &d);
        Tcl_WrongNumArgs(interp, 3, objv, "index");
        r = TCL_ERROR;
        // Update display
    } else {
        // "yview moveto fraction" or "yview scroll N unit|pages"
        type = Tk_GetScrollInfoObj(interp, objc, objv, &fraction, &count);
        switch (type) {
            case TK_SCROLL_ERROR:
                return TCL_ERROR;
            case TK_SCROLL_MOVETO:
                videoPtr->offset.y = (int)(videoPtr->videoHeight * fraction);
                break;
            case TK_SCROLL_PAGES:
                videoPtr->offset.y += (videoPtr->videoHeight / 10) * count;
                break;
            case TK_SCROLL_UNITS:
                videoPtr->offset.y += count;
                break;
            default:
                Tcl_SetResult(interp, "yview option not implemented", TCL_STATIC);
                r = TCL_ERROR;
        }
        if (r == TCL_OK) {
            videoPtr->flags |= UPDATE_V_SCROLL;
            VideoCalculateGeometry(videoPtr);
        }
    }
    return r;
}

/*
 *----------------------------------------------------------------------
 *
 * VideoXviewSubCmd --
 *
 *        Process the video widget's "xview" subcommand.
 *
 * Results:
 *        Standard Tcl result.
 *
 * Side effects:
 *        May change the listbox viewing area; may set the interpreter's result.
 *
 *----------------------------------------------------------------------
 */

static int
VideoWidgetXviewCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    Video *videoPtr = (Video *)clientData;
    Tk_Window tkwin = videoPtr->tkwin;
    int count, type, r = TCL_OK;
    double fraction, fraction2;

    if (objc == 2) {
        char buf[TCL_DOUBLE_SPACE * 2];

        if (videoPtr->stretch) {
            fraction = 0.0;
            fraction2 = 1.0;
        } else {
            fraction = (double)videoPtr->offset.x / (double)videoPtr->videoWidth;
            fraction2 = (double)(videoPtr->offset.x + Tk_Width(tkwin)) / (double)videoPtr->videoWidth;
        }

        if (fraction < 0.0)
            fraction = 0.0;
        if (fraction2 > 1.0)
            fraction2 = 1.0;

        sprintf(buf, "%g %g", fraction, fraction2);
        Tcl_SetObjResult(interp, Tcl_NewStringObj(buf, -1));
        r = TCL_OK;
        
    } else if (objc == 3) {
        // display beginning at Tcl_GetDoubleFromObj(objv[2], &d);
        Tcl_WrongNumArgs(interp, 3, objv, "index");
        r = TCL_ERROR;
        // Update display
    } else {
        type = Tk_GetScrollInfoObj(interp, objc, objv, &fraction, &count);
        switch (type) {
            case TK_SCROLL_ERROR:
                return TCL_ERROR;
            case TK_SCROLL_MOVETO:
                videoPtr->offset.x = (int)(videoPtr->videoWidth * fraction);
                break;
            case TK_SCROLL_PAGES:
                videoPtr->offset.x += (videoPtr->videoWidth / 10) * count;
                break;
            case TK_SCROLL_UNITS:
                videoPtr->offset.x += count;
                break;
            default:
                Tcl_SetResult(interp, "xview option not implemented", TCL_STATIC);
                r = TCL_ERROR;
        }
        if (r == TCL_OK) {
            videoPtr->flags |= UPDATE_H_SCROLL;
            VideoCalculateGeometry(videoPtr);
        }
    }
    return r;
}

static void
VideoUpdateVScrollbar(Video* videoPtr)
{
    Tcl_Interp *interp = videoPtr->interp;
    Tk_Window tkwin = videoPtr->tkwin;
    double fraction, fraction2;
    int r = TCL_OK;
    Tcl_Obj *cmdPtr;
    
    videoPtr->flags &= ~UPDATE_V_SCROLL;

    if (videoPtr->yscrollcmdPtr == NULL) {
        return;
    }
    
    if (videoPtr->stretch) {
        fraction = 0.0;
        fraction2 = 1.0;
    } else {
        fraction = (double)videoPtr->offset.y / (double)videoPtr->videoHeight;
        fraction2 = (double)(videoPtr->offset.y + Tk_Height(tkwin)) / (double)videoPtr->videoHeight;
    }
    if (fraction2 > 1.0) {
        fraction2 = 1.0;
    }

    cmdPtr = Tcl_DuplicateObj(videoPtr->yscrollcmdPtr);
    Tcl_ListObjAppendElement(interp, cmdPtr, Tcl_NewDoubleObj(fraction));
    Tcl_ListObjAppendElement(interp, cmdPtr, Tcl_NewDoubleObj(fraction2));

    /*
     * We must hold onto the interpreter from the listPtr because the data
     * at listPtr might be freed as a result of the Tcl_VarEval.
     */
    
    Tcl_Preserve((ClientData) interp);
    r = Tcl_EvalObjEx(interp, cmdPtr, TCL_EVAL_GLOBAL);
    if (r != TCL_OK) {
        Tcl_AddErrorInfo(interp,
                "\n    (vertical scrolling command executed by video widget)");
        Tcl_BackgroundError(interp);
    }
    Tcl_Release((ClientData) interp);
}

static void
VideoUpdateHScrollbar(Video* videoPtr)
{
    Tcl_Interp *interp = videoPtr->interp;
    Tk_Window tkwin = videoPtr->tkwin;
    double fraction, fraction2;
    int r = TCL_OK;
    Tcl_Obj *cmdPtr;
    
    videoPtr->flags &= ~UPDATE_H_SCROLL;

    if (videoPtr->xscrollcmdPtr == NULL) {
        return;
    }
    
    if (videoPtr->stretch) {
        fraction = 0.0;
        fraction2 = 1.0;
    } else {
        fraction = (double)videoPtr->offset.x / (double)videoPtr->videoWidth;
        fraction2 = (double)(videoPtr->offset.x + Tk_Width(tkwin)) / (double)videoPtr->videoWidth;
    }
    if (fraction2 > 1.0) {
        fraction2 = 1.0;
    }

    cmdPtr = Tcl_DuplicateObj(videoPtr->xscrollcmdPtr);
    Tcl_ListObjAppendElement(interp, cmdPtr, Tcl_NewDoubleObj(fraction));
    Tcl_ListObjAppendElement(interp, cmdPtr, Tcl_NewDoubleObj(fraction2));

    /*
     * We must hold onto the interpreter from the listPtr because the data
     * at listPtr might be freed as a result of the Tcl_VarEval.
     */
    
    Tcl_Preserve((ClientData) interp);
    r = Tcl_EvalObjEx(interp, cmdPtr, TCL_EVAL_GLOBAL);
    if (r != TCL_OK) {
        Tcl_AddErrorInfo(interp,
              "\n    (horizontal scrolling command executed by video widget)");
	Tcl_BackgroundError(interp);
    }
    Tcl_Release((ClientData) interp);
}

/* --- Nicked from 'tile' --- */
/* SendVirtualEvent --
 *      Send a virtual event notification to the specified target window.
 *      Equivalent to "event generate $tgtWindow <<$eventName>>"
 *
 *      Note that we use Tk_QueueWindowEvent, not Tk_HandleEvent,
 *      so this routine does not reenter the interpreter.
 */

void 
SendVirtualEvent(Tk_Window tgtWin, const char *eventName, unsigned int state)
{
    XEvent event;
    XVirtualEvent *eventPtr = (XVirtualEvent *)&event;

    memset(&event, 0, sizeof(event));
    event.xany.type = VirtualEvent;
    event.xany.serial = NextRequest(Tk_Display(tgtWin));
    event.xany.send_event = False;
    event.xany.window = Tk_WindowId(tgtWin);
    event.xany.display = Tk_Display(tgtWin);
    eventPtr->name = Tk_GetUid(eventName);
    eventPtr->state = state;
    /* eventPtr->x = pointer X */
    /* eventPtr->y = pointer Y */
    Tk_GetRootCoords(tgtWin, &eventPtr->x_root, &eventPtr->y_root);

    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
}

void 
SendConfigureEvent(Tk_Window tgtWin, int x, int y, int width, int height)
{
    XEvent event;

    memset(&event, 0, sizeof(event));
    event.xany.type = ConfigureNotify;
    event.xany.serial = NextRequest(Tk_Display(tgtWin));
    event.xany.send_event = False;
    event.xany.window = Tk_WindowId(tgtWin);
    event.xany.display = Tk_Display(tgtWin);
    event.xconfigure.width = width;
    event.xconfigure.height = height;
    event.xconfigure.x = x;
    event.xconfigure.y = y;

    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
}

/*
 * Local variables:
 * indent-tabs-mode: nil
 * End:
 */
