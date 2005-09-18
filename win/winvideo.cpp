/* winvidep.cpp - Copyright (C) 2003 Pat Thoyts <patthoyts@users.sf.net>
 *
 *                 ---  THIS IS C++ ---
 *
 * This provides the Windows platform specific code for the tkvideo
 * widget. This uses the DirectX DirectShow API and objects to hook
 * up either a video input device or a file source and render this
 * to the widget window.
 *
 * --------------------------------------------------------------------------
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 * --------------------------------------------------------------------------
 * $Id$
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#include "tkvideo.h"
#include <tkPlatDecls.h>
#include "dshow_utils.h"
#include <qedit.h>

#ifdef HAVE_WMF_SDK
#include <dshowasf.h>
#endif // HAVE_WMF_SDK

#if _MSC_VER >= 100
#pragma comment(lib, "amstrmid")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "shlwapi")
#endif

/** Application specific window message for filter graph notifications */
#define WM_GRAPHNOTIFY WM_APP + 82

/**
 * Windows platform specific data to be added to the tkvide widget structure
 */

typedef struct {
    IGraphBuilder *pFilterGraph;
    IMediaEventEx *pMediaEvent;
    IMediaSeeking *pMediaSeeking;
    IMediaControl *pMediaControl;
    IVideoWindow  *pVideoWindow;
    DWORD          dwRegistrationId;
    WNDPROC        wndproc;
} VideoPlatformData;

static HRESULT ShowCaptureFilterProperties(IGraphBuilder *pFilterGraph, HWND hwnd);
static HRESULT ShowCapturePinProperties(IGraphBuilder *pFilterGraph, HWND hwnd);
static HRESULT ConstructCaptureGraph(int nDevice, int nAudio, LPCWSTR sSource, LPCWSTR sOutput, IGraphBuilder **ppGraphBuilder);
static HRESULT CreateCompatibleSampleGrabber(IBaseFilter **ppFilter);
static HRESULT ConnectVideo(Video *videoPtr, HWND hwnd, IVideoWindow **ppVideoWindow);
static HRESULT GetVideoSize(Video *videoPtr, long *pWidth, long *pHeight);
static void ReleasePlatformData(VideoPlatformData *pPlatformData);
static int GrabSample(Video *videoPtr, LPCSTR imageName);
static int GetDeviceList(Tcl_Interp *interp, CLSID clsidCategory);
LRESULT APIENTRY VideopWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static Tcl_Obj *Win32Error(const char * szPrefix, HRESULT hr);
static void ComputeAnchor(Tk_Anchor anchor, Tk_Window tkwin,
                          int padX, int padY, int innerWidth, int innerHeight, int *xPtr, int *yPtr);

/**
 * Windows platform specific package initialization. We only need to setup for COM
 * This call joins our interpreter thread to a single threaded apartment.
 * @return A tcl result code.
 */

int 
VideopInit(Tcl_Interp *interp)
{
    HRESULT hr = CoInitialize(0);
    return SUCCEEDED(hr) ? TCL_OK : TCL_ERROR;
}

/**
 * Windows platform specific widget initialization. This function
 * is called just after the Tk widget has been created and the core
 * data structure initialized. This is our oportunity to initialize 
 * any platform specific extension data. See VideoPlatformData
 *
 * @param videoPtr pointer to the widget instance data
 */

int
VideopCreateWidget(Video *videoPtr)
{
    VideoPlatformData *platformPtr = (VideoPlatformData *)ckalloc(sizeof(VideoPlatformData));
    videoPtr->platformData = (ClientData)platformPtr;
    if (videoPtr->platformData != NULL) {
        memset(videoPtr->platformData, 0, sizeof(VideoPlatformData));
    } else {
        Tcl_Panic("out of memory");
    }
    return TCL_OK;
}

/**
 * Windows platform specific cleanup. This function is called to permit us
 * to release any platform specific resources associated with this widget.
 * It is called after the Tk windows has been destroyed and all the core
 * memory has been released.
 * We need to free the memory allocated during VideopCreateWidget here.
 * We also have to tidy up the filter graph and de-register it from the 
 * running object table.
 *
 * @param memPtr pointer tothe widget instance data
 */

void 
VideopCleanup(char *memPtr)
{
    Video *videoPtr = (Video *)memPtr;
    if (videoPtr->platformData != NULL) {
        ReleasePlatformData((VideoPlatformData *)videoPtr->platformData);
        ckfree((char *)videoPtr->platformData);
        videoPtr->platformData = NULL;
    }
}

/**
 * Windows platform specific window destruction.
 * This function is called just before the Tk window is destroyed. For the windows
 * implementation we must unsubclass the Tk window.
 */

void
VideopDestroy(Video *videoPtr)
{
    VideoPlatformData *platformPtr = (VideoPlatformData *)videoPtr->platformData;
    ReleasePlatformData(platformPtr);
    if (platformPtr->wndproc != NULL)
    {
        HWND hwnd = Tk_GetHWND(Tk_WindowId(videoPtr->tkwin)); 
        SetWindowLong(hwnd, GWL_WNDPROC, (LONG)platformPtr->wndproc);
        platformPtr->wndproc = NULL;
    }
}

/**
 *
 */

int
InitVideoSource(Video *videoPtr)
{
    VideoPlatformData *pPlatformData = (VideoPlatformData *)videoPtr->platformData;
    HRESULT hr = S_OK;

    // Release the current graph and any pointers into it.
    ReleasePlatformData(pPlatformData);

    int nVideo = -1, nAudio = -1;
    LPCWSTR sSource = NULL;
    LPCWSTR sOutput = Tcl_GetUnicode(videoPtr->outputPtr);

    if (Tcl_GetIntFromObj(NULL, videoPtr->sourcePtr, &nVideo) != TCL_OK)
        sSource = Tcl_GetUnicode(videoPtr->sourcePtr);
    if (Tcl_GetIntFromObj(NULL, videoPtr->audioPtr, &nAudio) != TCL_OK)
        nAudio = -1;

    hr = ConstructCaptureGraph(nVideo, nAudio, sSource, sOutput, &pPlatformData->pFilterGraph);
    if (SUCCEEDED(hr))
        hr = pPlatformData->pFilterGraph->QueryInterface(&pPlatformData->pMediaControl);
    if (SUCCEEDED(hr)) {
        RegisterFilterGraph(pPlatformData->pFilterGraph, &pPlatformData->dwRegistrationId);

        HWND hwnd = Tk_GetHWND(Tk_WindowId(videoPtr->tkwin)); 
        hr = ConnectVideo(videoPtr, hwnd, &pPlatformData->pVideoWindow);
        if (SUCCEEDED(hr)) {
            // Subclass the tk window so we can recieve graph messages
            if (pPlatformData->wndproc == NULL) {
                SetProp(hwnd, TEXT("Tkvideo"), (HANDLE)videoPtr);
                pPlatformData->wndproc = (WNDPROC)SetWindowLong(hwnd, GWL_WNDPROC, (LONG)VideopWndProc);
            }

            long w = 0, h = 0;
            hr = GetVideoSize(videoPtr, &w, &h);
            videoPtr->videoHeight = h;
            videoPtr->videoWidth = w;

            if (pPlatformData->pVideoWindow)
                pPlatformData->pVideoWindow->put_BorderColor(0xffffff);
        }
    }

    if (FAILED(hr))
    {
        Tcl_SetObjResult(videoPtr->interp, Win32Error("failed to initialize video source", hr));
        return TCL_ERROR;
    }
    return TCL_OK;
}

void 
ReleasePlatformData(VideoPlatformData *pPlatformData)
{
    if (pPlatformData->pMediaEvent) {
        pPlatformData->pMediaEvent->SetNotifyWindow((OAHWND)NULL, 0, 0);
        pPlatformData->pMediaEvent->Release();
        pPlatformData->pMediaEvent = NULL;
    }
    if (pPlatformData->pMediaSeeking) {
        pPlatformData->pMediaSeeking->Release();
        pPlatformData->pMediaSeeking = NULL;
    }
    if (pPlatformData->pMediaControl) {
        pPlatformData->pMediaControl->Release();
        pPlatformData->pMediaControl = NULL;
    }
    if (pPlatformData->pVideoWindow) {
        pPlatformData->pVideoWindow->Release();
        pPlatformData->pVideoWindow = NULL;
    }
    if (pPlatformData->pFilterGraph != NULL) {
        UnregisterFilterGraph(pPlatformData->dwRegistrationId);
        pPlatformData->pFilterGraph->Release();
        pPlatformData->pFilterGraph = NULL;
    }
}

void
VideopCalculateGeometry(Video *videoPtr)
{
    VideoPlatformData *pPlatformData = (VideoPlatformData *)videoPtr->platformData;
    int width, height, x = 0, y = 0;

    if (videoPtr->stretch) {
        width = Tk_ReqWidth(videoPtr->tkwin);
        height = Tk_ReqHeight(videoPtr->tkwin);
    } else {
        width = videoPtr->videoWidth;
        height = videoPtr->videoHeight;
        ComputeAnchor(videoPtr->anchor, videoPtr->tkwin, 0, 0, width, height, &x, &y);
    }

    if (pPlatformData && pPlatformData->pVideoWindow) {
        x = (videoPtr->offset.x > 0) ? -videoPtr->offset.x : x;
        y = (videoPtr->offset.y > 0) ? -videoPtr->offset.y : y;
        pPlatformData->pVideoWindow->SetWindowPosition(x, y, width, height);
    }
}

int 
VideopWidgetObjCmd(ClientData clientData, Tcl_Interp *interp,
                   int index, int objc, Tcl_Obj *CONST objv[])
{
    Video *videoPtr = (Video *)clientData;
    VideoPlatformData *pPlatformData = (VideoPlatformData *)videoPtr->platformData;
    IGraphBuilder *pFilterGraph = NULL;
    int r = TCL_OK;

    if (pPlatformData == NULL) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj("platform data not initialized yet", -1));
        return TCL_ERROR;
    }

    switch (index) {
    case VIDEO_PROPERTYPAGE:
        if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "\"filter\" or \"pin\"");
            r = TCL_ERROR;
        } else {
            char * page = Tcl_GetString(objv[2]);
            pFilterGraph = pPlatformData->pFilterGraph;
            if (strncmp("filter", page, 6) == 0) {
                ShowCaptureFilterProperties(pFilterGraph, Tk_GetHWND(Tk_WindowId(videoPtr->tkwin)));
            } else if (strncmp("pin", page, 3) == 0) {
                ShowCapturePinProperties(pFilterGraph, Tk_GetHWND(Tk_WindowId(videoPtr->tkwin)));
            } else {
                Tcl_WrongNumArgs(interp, 2, objv, "\"filter\" or \"pin\"");
                r = TCL_ERROR;
            }
        }
        break;

    case VIDEO_DEVICES:
        {
            if (objc < 2 || objc > 3) {
                Tcl_WrongNumArgs(interp, 2, objv, "?video | audio?");
                r = TCL_ERROR;
            } else {
                CLSID clsid = CLSID_VideoInputDeviceCategory;
                if (objc == 3) {
                    const char *type = Tcl_GetString(objv[2]);
                    if (strcmp("audio", type) == 0)
                        clsid = CLSID_AudioInputDeviceCategory;
                    else if (strcmp("video", type) != 0) {
                        Tcl_WrongNumArgs(interp, 2, objv, "?video | audio?");
                        r = TCL_ERROR;
                    }
                }
                if (r == TCL_OK)
                    r = GetDeviceList(interp, clsid);
            }
        }
        break;

    case VIDEO_STOP:
    case VIDEO_START: 
    case VIDEO_PAUSE:
    {
        pFilterGraph = pPlatformData->pFilterGraph;
        if (! pFilterGraph) {
            Tcl_SetObjResult(interp, Tcl_NewStringObj("error: no video source initialized", -1));
            return TCL_ERROR;
        }

        CComPtr<IBaseFilter> pGrabberFilter;
        CComPtr<ISampleGrabber> pSampleGrabber;
        HRESULT hr = S_OK;
        if (SUCCEEDED( pFilterGraph->FindFilterByName(SAMPLE_GRABBER_NAME, &pGrabberFilter) ))
            hr = pGrabberFilter.QueryInterface(&pSampleGrabber);

        if (SUCCEEDED(hr)) {
            switch (index) {
            case VIDEO_START:
                if (SUCCEEDED(hr) && pPlatformData->pVideoWindow)
                    hr = pPlatformData->pVideoWindow->put_Visible(OATRUE);
                if (SUCCEEDED(hr) && pSampleGrabber)
                    hr = pSampleGrabber->SetBufferSamples(TRUE);
                if (SUCCEEDED(hr) && pPlatformData->pMediaControl)
                    hr = pPlatformData->pMediaControl->Run();
                break;
            case VIDEO_PAUSE:
                if (SUCCEEDED(hr) && pPlatformData->pVideoWindow)
                    hr = pPlatformData->pVideoWindow->put_Visible(OATRUE);
                if (SUCCEEDED(hr) && pSampleGrabber)
                    hr = pSampleGrabber->SetBufferSamples(TRUE);
                if (SUCCEEDED(hr) && pPlatformData->pMediaControl)
                    hr = pPlatformData->pMediaControl->Pause();
                break;

            case VIDEO_STOP:
                if (SUCCEEDED(hr) && pSampleGrabber)
                    hr = pSampleGrabber->SetBufferSamples(FALSE);
                if (SUCCEEDED(hr) && pPlatformData->pMediaControl)
                    hr = pPlatformData->pMediaControl->Stop();
                //if (SUCCEEDED(hr) && pPlatformData->pVideoWindow)
                //    hr = pPlatformData->pVideoWindow->put_Visible(OAFALSE);
                break;                
            }
        }
        if (FAILED(hr)) {
            Tcl_SetObjResult(interp, Win32Error("video command failed", hr));
            r = TCL_ERROR;
        }
        break;
    }

    case VIDEO_SEEK:
    {
        if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "position");
            r = TCL_ERROR;
        } else {
            pFilterGraph = pPlatformData->pFilterGraph;
            if (! pFilterGraph) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj("error: no video source initialized", -1));
                return TCL_ERROR;
            }

            if (!pPlatformData->pMediaSeeking) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj("seeking not supported on this source", -1));
                r = TCL_ERROR;
            } else {
                REFERENCE_TIME t = 0;
                r = Tcl_GetWideIntFromObj(interp, objv[2], &t);
                if (r == TCL_OK) {
                    t *= 10000; // convert from ms to 100ns
                    pPlatformData->pMediaSeeking->SetPositions(&t, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
                }
            }
        }
        break;
    }

    case VIDEO_TELL:
    {
        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, "");
            r = TCL_ERROR;
        } else {
            pFilterGraph = pPlatformData->pFilterGraph;
            if (! pFilterGraph) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj("error: no video source initialized", -1));
                return TCL_ERROR;
            }

            if (!pPlatformData->pMediaSeeking) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj("seeking not supported for this type of source", -1));
                r = TCL_ERROR;
            } else {
                REFERENCE_TIME tDuration, tCurrent, tStop;
                pPlatformData->pMediaSeeking->SetTimeFormat(&TIME_FORMAT_MEDIA_TIME);
                pPlatformData->pMediaSeeking->GetDuration(&tDuration);
                pPlatformData->pMediaSeeking->GetPositions(&tCurrent, &tStop);
                tDuration /= 10000; tStop /= 10000; tCurrent /= 10000; // convert units from 100ns to ms.
                Tcl_Obj *resObj = Tcl_NewListObj(0, NULL);
                r = Tcl_ListObjAppendElement(interp, resObj, Tcl_NewWideIntObj(tCurrent));
                if (r == TCL_OK)
                    r = Tcl_ListObjAppendElement(interp, resObj, Tcl_NewWideIntObj(tStop));
                if (r == TCL_OK)
                    r = Tcl_ListObjAppendElement(interp, resObj, Tcl_NewWideIntObj(tDuration));
                Tcl_SetObjResult(interp, resObj);
            }
        }
        break;
    }

    case VIDEO_PICTURE: 
    {
        if (objc < 2 || objc > 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "?imagename?");
            r = TCL_ERROR;
        } else {
            if (! pPlatformData->pFilterGraph) {
                Tcl_SetObjResult(interp, Tcl_NewStringObj("error: no video source initialized", -1));
                return TCL_ERROR;
            }
            const char *imageName = NULL;
            if (objc == 3)
                imageName = Tcl_GetString(objv[2]);
            r = GrabSample(videoPtr, imageName);
        }
        break;
    }

    default:
        Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
        r = TCL_ERROR;
    }

    return r;
}

HRESULT
ShowCaptureFilterProperties(IGraphBuilder *pFilterGraph, HWND hwnd)
{
    HRESULT hr = E_UNEXPECTED;
    if (pFilterGraph)
    {
        CComPtr<IBaseFilter> pFilter;

        hr = pFilterGraph->FindFilterByName(CAPTURE_FILTER_NAME, &pFilter);
        if (SUCCEEDED(hr))
        {
            hr = ::ShowPropertyPages(pFilter, L"Capture Filter", hwnd);

            //FIX ME: check for changed format.
        }
    }
    return hr;
}

HRESULT
ShowCapturePinProperties(IGraphBuilder *pFilterGraph, HWND hwnd)
{
    HRESULT hr = E_UNEXPECTED;
    if (pFilterGraph)
    {
        CComPtr<IBaseFilter> pFilter;
        CComPtr<IPin> pPin;

        hr = pFilterGraph->FindFilterByName(CAPTURE_FILTER_NAME, &pFilter);
        if (SUCCEEDED(hr))
            hr = FindPinByCategory(pFilter, PIN_CATEGORY_CAPTURE, &pPin);
        if (SUCCEEDED(hr))
        {
            hr = ::ShowPropertyPages(pPin, L"Capture Pin", hwnd);

            //FIX ME: check for changed format.
        }
    }
    return hr;
}

/**
 * Obtains a list of Direct Show registered devices in a specified category.
 * For instance CLSID_VideoInputDeviceCategory lists video sources and
 * or CLSID_AudioInputDeviceCategory will list audio sources.
 *
 * @param interp [in] pointer to the tcl interpreter
 * @param clsidCategory [in] identifier of the device category to list
 *
 * @return A tcl result code. The interpreter result is set to a Tcl list
 *   containing the devices names.
 */

int 
GetDeviceList(Tcl_Interp *interp, CLSID clsidCategory)
{
    CComPtr<ICreateDevEnum> pCreateDevEnum;
    CComPtr<IBindCtx> pctx;
    Tcl_Obj *listPtr = NULL;

    HRESULT hr = pCreateDevEnum.CoCreateInstance(CLSID_SystemDeviceEnum);
    if (SUCCEEDED(hr))
        hr = CreateBindCtx(0, &pctx);
    if (SUCCEEDED(hr))
    {
        CComPtr<IEnumMoniker> pEnumMoniker;
        IMoniker *pmks[12];
        ULONG nmks = 0;
        HRESULT hrLoop = S_OK;

        listPtr = Tcl_NewListObj(0, NULL);

        // bug #4992: returns S_FALSE in the event of failure and sets the pointer NULL.
        hr = pCreateDevEnum->CreateClassEnumerator(clsidCategory, &pEnumMoniker, 0);
        while (SUCCEEDED(hr) && pEnumMoniker && hrLoop == S_OK)
        {
            hr = hrLoop = pEnumMoniker->Next(12, pmks, &nmks);
            for (ULONG n = 0; SUCCEEDED(hr) && n < nmks; n++)
            {
                CComPtr<IPropertyBag> pbag;
                CComVariant vName;
                hr = pmks[n]->BindToStorage(pctx, NULL, IID_IPropertyBag, reinterpret_cast<void**>(&pbag));
                if (SUCCEEDED(hr))
                    hr = pbag->Read(L"FriendlyName", &vName, NULL);
                if (SUCCEEDED(hr))
                    Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewUnicodeObj(vName.bstrVal, -1));

                pmks[n]->Release(),  pmks[n] = 0;
            }
            if (hrLoop == S_OK)
                hr = pEnumMoniker->Reset();
        }
        
        if (SUCCEEDED(hr)) {
            Tcl_SetObjResult(interp, listPtr);
        }
    }
    return SUCCEEDED(hr) ? TCL_OK : TCL_ERROR;
}

// -----------------------------------------------------------------

static HRESULT
MediaType(LPCWSTR sPath, LPCGUID *ppMediaType, LPCGUID *ppMediaSubType)
{
    struct MediaTable { LPCWSTR wsz; LPCGUID type; LPCGUID subtype; };
    MediaTable map[] = {
        L".avi", NULL,              &MEDIASUBTYPE_Avi,
        L".wmv", &MEDIATYPE_Video,  &MEDIASUBTYPE_Asf,
        L".asf", &MEDIATYPE_Video,  &MEDIASUBTYPE_Asf,
        L".asx", &MEDIATYPE_Stream, &MEDIASUBTYPE_Asf,
        L".mov", &MEDIATYPE_Video,  &MEDIASUBTYPE_QTMovie,
    };
    LPCWSTR sExt = PathFindExtensionW(sPath);
    for (int n = 0; sExt && n < sizeof(map) / sizeof(map[0]); n++)
    {
        if (wcsnicmp(map[n].wsz, sExt, 4) == 0)
        {
            *ppMediaType = map[n].type;
            *ppMediaSubType = map[n].subtype;
            return S_OK;
        }
    }
    return E_INVALIDARG;
}

static HRESULT 
ConstructCaptureGraph(int DeviceIndex, int AudioIndex, LPCWSTR sSourcePath, LPCWSTR sOutputPath, IGraphBuilder **ppGraph)
{
    CComPtr<IGraphBuilder> pGraph;
    CComPtr<ICaptureGraphBuilder2> pBuilder;
    bool bFileSource = (sSourcePath && wcslen(sSourcePath) > 0);
    bool bRecordVideo = (sOutputPath && wcslen(sOutputPath) > 0);
    bool bRequireAudio = false;

    HRESULT hr = pGraph.CoCreateInstance(CLSID_FilterGraph);
    if (SUCCEEDED(hr))
        hr = pBuilder.CoCreateInstance(CLSID_CaptureGraphBuilder2);
    if (SUCCEEDED(hr))
        hr = pBuilder->SetFiltergraph(pGraph);

    // If we are to capture the output to a file, then add in the avimux filter
    // and set the output file destination. Returns pointers to the mux and the
    // file writer that it creates for us. Can create AVI or ASF
    CComPtr<IBaseFilter> pMux;
    CComPtr<IFileSinkFilter> pFileSink;
    if (SUCCEEDED(hr) && !bFileSource && bRecordVideo)
    {
        LPCGUID pType = NULL, pSubType = NULL;
        hr = MediaType(sOutputPath, &pType, &pSubType);
        if (SUCCEEDED(hr) && pSubType == &MEDIASUBTYPE_Asf)
            bRequireAudio = true; // the asf writer _requires_ an audio input.
        if (SUCCEEDED(hr))
            hr = pBuilder->SetOutputFileName(pSubType, sOutputPath, &pMux, &pFileSink);
#ifdef HAVE_WMF_SDK
        if (SUCCEEDED(hr) && pSubType == &MEDIASUBTYPE_Asf)
        {
            CComPtr<IConfigAsfWriter> pConfig;
            pMux.QueryInterface(&pConfig);
        }
#endif // HAVE_WMF_SDK
    }

    // Now we put in the source filter. This may be a file source, url source or
    // a capture device.
    CComPtr<IBaseFilter> pCaptureFilter;
    CComPtr<IBaseFilter> pAudioFilter;
    if (SUCCEEDED(hr))
    {
        if (bFileSource)
        {
            hr = pGraph->AddSourceFilter(sSourcePath, CAPTURE_FILTER_NAME, &pCaptureFilter);
        }
        else
        {
            CComPtr<IMoniker> pmk;
            CComPtr<IBindCtx> pBindContext;

            // Add our input device to the capture graph.
            if (SUCCEEDED(hr))
                hr = GetDeviceMoniker(CLSID_VideoInputDeviceCategory, DeviceIndex, &pmk);
            if (!pmk)
                hr = E_INVALIDARG;
            if (SUCCEEDED(hr))
                hr = CreateBindCtx(0, &pBindContext);
            if (SUCCEEDED(hr))
                hr = pmk->BindToObject(pBindContext, NULL, IID_IBaseFilter, reinterpret_cast<void**>(&pCaptureFilter));
            if (SUCCEEDED(hr))
                hr = pGraph->AddFilter(pCaptureFilter, CAPTURE_FILTER_NAME);

            pmk.Release(); pBindContext.Release();
            if (SUCCEEDED(hr) && AudioIndex != -1)
            {
                HRESULT hrA = GetDeviceMoniker(CLSID_AudioInputDeviceCategory, AudioIndex, &pmk);
                if (SUCCEEDED(hrA))
                    hrA = CreateBindCtx(0, &pBindContext);
                if (SUCCEEDED(hrA))
                    hrA = pmk->BindToObject(pBindContext, NULL, IID_IBaseFilter, reinterpret_cast<void**>(&pAudioFilter));
                if (SUCCEEDED(hrA))
                    hrA = pGraph->AddFilter(pAudioFilter, AUDIO_FILTER_NAME);
            }
        }
    }

    // Add in a Sample Grabber to the graph
    CComPtr<IBaseFilter> pGrabberFilter;
    if (SUCCEEDED(hr))
    {
        hr = CreateCompatibleSampleGrabber(&pGrabberFilter);
        if (SUCCEEDED(hr))
            hr = pGraph->AddFilter(pGrabberFilter, SAMPLE_GRABBER_NAME);
    }

    // Add a video renderer to the graph
    CComPtr<IBaseFilter> pRenderFilter;
    if (SUCCEEDED(hr))
    {
        hr = pRenderFilter.CoCreateInstance(CLSID_VideoMixingRenderer9);
        if (FAILED(hr))
            hr = pRenderFilter.CoCreateInstance(CLSID_VideoRenderer);
        if (SUCCEEDED(hr))
            hr = pGraph->AddFilter(pRenderFilter, RENDERER_FILTER_NAME);
    }

    // Could add in a compressor here.


    // Add in a preview
    if (SUCCEEDED(hr))
    {
        if (bFileSource) 
        {
            LPCGUID pType = NULL, pSubType = NULL;
            hr = MediaType(sSourcePath, &pType, &pSubType);
            hr = pBuilder->RenderStream(NULL, pType, pCaptureFilter, pGrabberFilter, pRenderFilter);
        }
        else
            hr = pBuilder->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, pCaptureFilter, pGrabberFilter, pRenderFilter);
    }

    // Now connect up the video source to the mux for saving. (The mux is already connected to the writer).
    if (SUCCEEDED(hr) && pMux)
        hr = pBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, pCaptureFilter, NULL, pMux);

    // Connect audio (if any)
    if (SUCCEEDED(hr))
    {
        if (bRequireAudio || (pAudioFilter && pMux))
        {
            CComPtr<IBaseFilter> pTeeFilter;
            pTeeFilter.CoCreateInstance(CLSID_InfTee);
            if (pTeeFilter) pGraph->AddFilter(pTeeFilter, NULL);
            if (!pAudioFilter) pAudioFilter = pCaptureFilter;
            if (pMux)
                pBuilder->RenderStream(NULL, &MEDIATYPE_Audio, pAudioFilter, pTeeFilter, pMux);
        }
        else if (pAudioFilter)
        {
            pBuilder->RenderStream(NULL, &MEDIATYPE_Audio, pAudioFilter, NULL, NULL);
        }
        else
        {
            pBuilder->RenderStream(NULL, &MEDIATYPE_Audio, pCaptureFilter, NULL, NULL);
        }
    }

    if (SUCCEEDED(hr))
        hr = pGraph.CopyTo(ppGraph);
    return hr;
}

/**
 * This function obtains the colour depth of the current desktop
 * window and configures an instance of the Sample Grabber filter
 * to be compatible with the desktop display. This means that we
 * will recover useful bitmaps from the grabber. This also ensures
 * that the filter is the penultimate filter in the graph and causes
 * the grabber to be placed @em after the AVI decompression filter.
 *
 * @param ppFilter [out] pointer is set to the newly configured
 *  sample grabber.
 */

HRESULT
CreateCompatibleSampleGrabber(IBaseFilter **ppFilter)
{
    CComPtr<IBaseFilter> pGrabberFilter;
    HRESULT hr = pGrabberFilter.CoCreateInstance(CLSID_SampleGrabber);
    if (SUCCEEDED(hr))
    {
        // Set the grabber to grab video stills.
        AM_MEDIA_TYPE mt;
        ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
        mt.majortype = MEDIATYPE_Video;
        HDC hdc = ::GetDC(HWND_DESKTOP);
        int iBitDepth = GetDeviceCaps(hdc, BITSPIXEL);

        switch (iBitDepth)
        {
        case  8: mt.subtype = MEDIASUBTYPE_RGB8;   break;
        case 24: mt.subtype = MEDIASUBTYPE_RGB24;  break;
        case 32: mt.subtype = MEDIASUBTYPE_RGB32;  break;
        case 16:
            {
                int r = 0, g = 0, b = 0;
                mt.subtype = MEDIASUBTYPE_RGB565;
                if (GetRGBBitsPerPixel(hdc, &r, &g, &b) && g == 5)
                    mt.subtype = MEDIASUBTYPE_RGB555;
            }
            break;
        default: mt.subtype = MEDIASUBTYPE_RGB24; break;
        }
        ::ReleaseDC(HWND_DESKTOP, hdc);

        CComQIPtr<ISampleGrabber> pSampleGrabber(pGrabberFilter);
        if (pSampleGrabber)
            hr = pSampleGrabber->SetMediaType(&mt);
        if (SUCCEEDED(hr))
            hr = pGrabberFilter.CopyTo(ppFilter);
    }
    return hr;
}

/**
 * Connect the capture graph to a window. This window needs
 * to adjust itself for the correct size. Note that some types of
 * graph may not have a video window object. For instance a graph
 * based on an audio-only sample.
 *
 * @param pGraph [in] interface to the current filter graph
 * @param hwnd [in] handle to the window we will display in.
 * @param ppVideoWindow [out] pointer to the IVideoWindow interface of this
 *    filter graph if available.
 * @return S_OK on success. E_INVALIDARG if the hwnd parameter is invalid.
 */

HRESULT
ConnectVideo(Video *videoPtr, HWND hwnd, IVideoWindow **ppVideoWindow)
{
    VideoPlatformData *platformPtr = (VideoPlatformData *)videoPtr->platformData;
    CComPtr<IVideoWindow> pVideoWindow;
    RECT rc;
    
    HRESULT hr = E_INVALIDARG;
    if (::IsWindow(hwnd))
    {
        ::GetClientRect(hwnd, &rc);

        // Connect our window for graph events
        if (SUCCEEDED( platformPtr->pFilterGraph->QueryInterface(&platformPtr->pMediaEvent) ))
        {
            hr = platformPtr->pMediaEvent->SetNotifyWindow((OAHWND)hwnd, WM_GRAPHNOTIFY, 0);
        }

        // Get a seeking pointer only if seeking is supported.
        if (SUCCEEDED( platformPtr->pFilterGraph->QueryInterface(&platformPtr->pMediaSeeking) ))
        {
            DWORD grfCaps = AM_SEEKING_CanSeekAbsolute | AM_SEEKING_CanGetDuration;
            if (platformPtr->pMediaSeeking->CheckCapabilities(&grfCaps) != S_OK)
            {
                platformPtr->pMediaSeeking->Release();
                platformPtr->pMediaSeeking = NULL;
            }
        }

        // Configure the overlay window.
        if (SUCCEEDED(hr))
            hr = platformPtr->pFilterGraph->QueryInterface(&pVideoWindow);
        if (SUCCEEDED(hr))
            hr = pVideoWindow->put_Owner((OAHWND)hwnd);
        if (SUCCEEDED(hr))
            hr = pVideoWindow->put_WindowStyle(WS_CHILD | WS_CLIPCHILDREN);
        if (SUCCEEDED(hr))
            hr = pVideoWindow->SetWindowPosition(rc.left, rc.top, rc.right, rc.bottom);
        if (SUCCEEDED(hr))
            hr = pVideoWindow->put_Visible(OATRUE);
        if (SUCCEEDED(hr))
            hr = pVideoWindow.CopyTo(ppVideoWindow);
    }
    return hr;
}

/**
 * Obtains the width and height in pixels of the video renderer filter
 *
 * @param pGraph [in] pointer to the current filter graph
 * @param pWidth [out] set to the width in pixels of the video image
 * @param pHeight [out] set to the height in pixels of the video image
 *
 * @return S_OK on success or S_FALSE if the graph does not support video.
 */

HRESULT
GetVideoSize(Video *videoPtr, long *pWidth, long *pHeight)
{
    VideoPlatformData *pPlatformData = (VideoPlatformData *)videoPtr->platformData;
    HRESULT hr = S_FALSE;
    if (pPlatformData->pFilterGraph)
    {
        CComPtr<IBaseFilter> pFilter;
        CComPtr<IPin> pPin;
        AM_MEDIA_TYPE mt;

        hr = pPlatformData->pFilterGraph->FindFilterByName(RENDERER_FILTER_NAME, &pFilter);
        if (SUCCEEDED(hr))
            hr = FindPinByDirection(pFilter, PINDIR_INPUT, &pPin);
        if (SUCCEEDED(hr))
            hr = pPin->ConnectionMediaType(&mt);
        if (SUCCEEDED(hr))
        {
            if (mt.formattype == FORMAT_VideoInfo)
            {
                VIDEOINFOHEADER *pvih = reinterpret_cast<VIDEOINFOHEADER *>(mt.pbFormat);
                *pWidth = pvih->bmiHeader.biWidth;
                *pHeight = pvih->bmiHeader.biHeight;                
            }

            // We have to clean up the media format properly.
            if (mt.cbFormat > 0)
                CoTaskMemFree(mt.pbFormat);
        }
    }
    return hr;
}

/**
 * This function takes an image from the sample grabber and creates a
 * Tk photo using the returned data.
 *
 * @param interp [in] pointer to the tcl interpreter
 * @param pGraph [in] pointer to the filter graph
 * @param imageName [in] optional name to use for the new Tk image
 *
 * @return TCL_OK on success. On failure TCL_ERROR and the interpreter
 *  result is set to describe the error.
 */

int
GrabSample(Video *videoPtr, LPCSTR imageName)
{
    VideoPlatformData *pPlatformData = (VideoPlatformData *)videoPtr->platformData;
    CComPtr<IBaseFilter> pGrabberFilter;
    CComPtr<ISampleGrabber> pSampleGrabber;
    Tcl_Obj *errObj = NULL;
    int r = TCL_OK;

    HRESULT hr = pPlatformData->pFilterGraph->FindFilterByName(SAMPLE_GRABBER_NAME, &pGrabberFilter);
    if (SUCCEEDED(hr))
        hr = pGrabberFilter.QueryInterface(&pSampleGrabber);

    AM_MEDIA_TYPE mt;
    ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));

    if (SUCCEEDED(hr))
        hr = pSampleGrabber->GetConnectedMediaType(&mt);

    // Copy the bitmap info from the media type structure
    VIDEOINFOHEADER *pvih = reinterpret_cast<VIDEOINFOHEADER *>(mt.pbFormat);
    BITMAPINFOHEADER bih;
    if (SUCCEEDED(hr))
    {
        ZeroMemory(&bih, sizeof(BITMAPINFOHEADER));
        CopyMemory(&bih, &pvih->bmiHeader, sizeof(BITMAPINFOHEADER));
        if (mt.cbFormat > 0)
            CoTaskMemFree(mt.pbFormat);
    }

    // Get the image data - first finding out how much space to allocate.
    // Copy the image into the buffer.
    long cbData = 0;
    LPBYTE pData = NULL;
    if (SUCCEEDED(hr))
        hr = pSampleGrabber->GetCurrentBuffer(&cbData, NULL);
    if (hr == E_INVALIDARG)
        errObj = Tcl_NewStringObj("image capture failed: no samples are being buffered", -1);
    if (SUCCEEDED(hr))
    {
        pData = new BYTE[cbData];
        hr = pSampleGrabber->GetCurrentBuffer(&cbData, reinterpret_cast<long*>(pData));
        if (SUCCEEDED(hr))
        {
            // Create a photo image.
            //image create photo -height n -width
            Tcl_Obj *objv[8];
            int      ndx = 0;
            objv[ndx++] = Tcl_NewStringObj("image", -1);
            objv[ndx++] = Tcl_NewStringObj("create", -1);
            objv[ndx++] = Tcl_NewStringObj("photo", -1);
            if (imageName != NULL)
                objv[ndx++] = Tcl_NewStringObj(imageName, -1);
            objv[ndx++] = Tcl_NewStringObj("-height", -1);
            objv[ndx++] = Tcl_NewLongObj(bih.biHeight);
            objv[ndx++] = Tcl_NewStringObj("-width", -1);
            objv[ndx++] = Tcl_NewLongObj(bih.biWidth);
            r = Tcl_EvalObjv(videoPtr->interp, ndx, objv, 0);
            if (r == TCL_OK) {
                imageName = Tcl_GetStringResult(videoPtr->interp);
                Tk_PhotoHandle img = Tk_FindPhoto(videoPtr->interp, imageName);
                Tk_PhotoImageBlock block;

                Tk_PhotoBlank(img);
                block.width = bih.biWidth;
                block.height = bih.biHeight;
                // hard coding for 24/32 bit bitmaps.
                block.pixelSize = bih.biBitCount / 8;
                block.pitch  = block.pixelSize * block.width;
                block.offset[0] = 2;  /* R */
                block.offset[1] = 1;  /* G */
                block.offset[2] = 0;  /* B */
#if TK_MINOR_VERSION >= 3
                block.offset[3] = 3;
#endif
                block.pixelPtr = pData;

#if TK_MINOR_VERSION >= 3
                // We have to fix the alpha channel. By default it is undefined and tends to
                // produce a transparent image.
                for (LPBYTE p = block.pixelPtr; p < block.pixelPtr+cbData; p += block.pixelSize)
                    *(p + block.offset[3]) = 0xff;
#endif

                // If biHeight is positive the bitmap is a bottom-up DIB.
                if (bih.biHeight > 0)
                {
                    DWORD cbRow = block.pitch;
                    LPBYTE pTmp = new BYTE[cbRow];
                    for (int i = 0; i < block.height/2; i++)
                    {
                        LPBYTE pTop = block.pixelPtr + (i * cbRow);
                        LPBYTE pBot = block.pixelPtr + ((block.height - i - 1) * cbRow);
                        CopyMemory(pTmp, pBot, cbRow);
                        CopyMemory(pBot, pTop, cbRow);
                        CopyMemory(pTop, pTmp, cbRow);
                    }
                }

                Tk_PhotoPutBlock(
#if TK_MAJOR_VERSION >= 8 && TK_MINOR_VERSION >= 5
                                 videoPtr->interp,
#endif
                                 img, &block,
                                 0, 0, bih.biWidth, bih.biHeight, TK_PHOTO_COMPOSITE_SET);
            }        
        }
    }
    if (FAILED(hr)) {
        if (errObj == NULL)
            errObj = Win32Error("failed to capture image", hr);
        Tcl_SetObjResult(videoPtr->interp, errObj);
        r = TCL_ERROR;
    }

    return r;
}

LRESULT APIENTRY
VideopWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Video *videoPtr = (Video *)GetProp(hwnd, TEXT("Tkvideo"));
    VideoPlatformData *platformPtr = (VideoPlatformData *)videoPtr->platformData;
    if (uMsg == WM_GRAPHNOTIFY && platformPtr->pMediaEvent != NULL)
    {
        long evCode = 0, lParam1 = 0, lParam2 = 0;
        while (SUCCEEDED( platformPtr->pMediaEvent->GetEvent(&evCode, &lParam1, &lParam2, 0) ))
        {
            switch (evCode)
            {
            case EC_PAUSED:
                SendVirtualEvent(videoPtr->tkwin, "VideoPaused");
                break;
            case EC_COMPLETE:
                SendVirtualEvent(videoPtr->tkwin, "VideoComplete");
                break;
            case EC_USERABORT:
                SendVirtualEvent(videoPtr->tkwin, "VideoUserAbort");
                break;
            case EC_VIDEO_SIZE_CHANGED:
                {
                    int width = LOWORD(lParam1);
                    int height = HIWORD(lParam1);
                    SendConfigureEvent(videoPtr->tkwin, 0, 0, width, height);
                }
                break;
            }
            platformPtr->pMediaEvent->FreeEventParams(evCode, lParam1, lParam2);
        }
    }
    return CallWindowProc(platformPtr->wndproc, hwnd, uMsg, wParam, lParam);
}

static Tcl_Obj *
Win32Error(const char * szPrefix, HRESULT hr)
{
    Tcl_Obj *msgObj = NULL;
    char * lpBuffer = NULL;
    DWORD  dwLen = 0;

    dwLen = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER
                          | FORMAT_MESSAGE_FROM_SYSTEM,
                          NULL, (DWORD)hr, LANG_NEUTRAL,
                          (LPTSTR)&lpBuffer, 0, NULL);
    if (dwLen < 1) {
        dwLen = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER
                              | FORMAT_MESSAGE_FROM_STRING
                              | FORMAT_MESSAGE_ARGUMENT_ARRAY,
                              "code 0x%1!08X!%n", 0, LANG_NEUTRAL,
                              (LPTSTR)&lpBuffer, 0, (va_list *)&hr);
    }

    msgObj = Tcl_NewStringObj(szPrefix, -1);
    if (dwLen > 0) {
        char *p = lpBuffer + dwLen - 1;        /* remove cr-lf at end */
        for ( ; p && *p && isspace(*p); p--)
            ;
        *++p = 0;
        Tcl_AppendToObj(msgObj, ": ", 2);
        Tcl_AppendToObj(msgObj, lpBuffer, -1);
    }
    LocalFree((HLOCAL)lpBuffer);
    return msgObj;
}

/*
 *---------------------------------------------------------------------------
 *
 * TkComputeAnchor --
 *
 *	Determine where to place a rectangle so that it will be properly
 *	anchored with respect to the given window.  Used by widgets
 *	to align a box of text inside a window.  When anchoring with
 *	respect to one of the sides, the rectangle be placed inside of
 *	the internal border of the window.
 *
 * Results:
 *	*xPtr and *yPtr set to the upper-left corner of the rectangle
 *	anchored in the window.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */
void
ComputeAnchor(Tk_Anchor anchor, Tk_Window tkwin, 
              int padX, int padY, int innerWidth, int innerHeight, int *xPtr, int *yPtr)
{
    switch (anchor) {
	case TK_ANCHOR_NW:
	case TK_ANCHOR_W:
	case TK_ANCHOR_SW:
	    *xPtr = Tk_InternalBorderLeft(tkwin) + padX;
	    break;

	case TK_ANCHOR_N:
	case TK_ANCHOR_CENTER:
	case TK_ANCHOR_S:
	    *xPtr = (Tk_Width(tkwin) - innerWidth) / 2;
	    break;

	default:
	    *xPtr = Tk_Width(tkwin) - (Tk_InternalBorderRight(tkwin) + padX)
		    - innerWidth;
	    break;
    }

    switch (anchor) {
	case TK_ANCHOR_NW:
	case TK_ANCHOR_N:
	case TK_ANCHOR_NE:
	    *yPtr = Tk_InternalBorderTop(tkwin) + padY;
	    break;

	case TK_ANCHOR_W:
	case TK_ANCHOR_CENTER:
	case TK_ANCHOR_E:
	    *yPtr = (Tk_Height(tkwin) - innerHeight) / 2;
	    break;

	default:
	    *yPtr = Tk_Height(tkwin) - Tk_InternalBorderBottom(tkwin) - padY
		    - innerHeight;
	    break;
    }
}

// -------------------------------------------------------------------------
// Local variables:
// mode: c++
// indent-tabs-mode: nil
// End:
