/* winvideo.cpp - Copyright (c) 2003 Pat Thoyts
 *
 * This provides the Windows platform specific code for the tkvideo
 * widget. This uses the DirectX DirectShow API and objects to hook
 * up either a video input device or a file source and render this
 * to the widget window.
 *
 * @@@ Lots of tidying up to do. Much of this code is redundant.
 * @@@ 
 * 
 *
 *                 ---  THIS IS C++ ---
 *
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "tkvideo.h"
#include <tkPlatDecls.h>
#include "dshow_utils.h"

#include <qedit.h>
#pragma comment(lib, "amstrmid")
#pragma comment(lib, "gdi32")

static HRESULT ShowCaptureFilterProperties(IGraphBuilder *pFilterGraph, HWND hwnd);
static HRESULT ShowCapturePinProperties(IGraphBuilder *pFilterGraph, HWND hwnd);
static HRESULT ConstructCaptureGraph(int DeviceIndex, IGraphBuilder **ppGraphBuilder);
static HRESULT ConstructFileGraph(LPOLESTR sFilename, IGraphBuilder **ppGraphBuilder);
static HRESULT ReconnectFilterGraph(IGraphBuilder *pGraphBuilder);
static HRESULT ConnectFilterGraph(IGraphBuilder *pGraphBuilder, IBaseFilter *pCaptureFilter, IBaseFilter *pGrabberFilter, IBaseFilter *pRenderFilter);
static HRESULT ConnectVideo(IGraphBuilder *pGraphBuilder, HWND hwnd, IVideoWindow **ppVideoWindow);
static HRESULT GetVideoSize(IGraphBuilder *pFilterGraph, long *pWidth, long *pHeight);
static HRESULT DisconnectFilterGraph(IFilterGraph *pFilterGraph);
static HRESULT DisconnectPins(IBaseFilter *pFilter);
static int GetDeviceList(Tcl_Interp *interp);

typedef struct {
    IGraphBuilder *pFilterGraph;
    DWORD          dwRegistrationId;
} VideoPlatformData;

int 
VideopInit(Tcl_Interp *interp)
{
    HRESULT hr = CoInitialize(0);
    return SUCCEEDED(hr) ? TCL_OK : TCL_ERROR;
}

int
VideopCreateWidget(Video *videoPtr)
{
    videoPtr->platformData = (ClientData)ckalloc(sizeof(VideoPlatformData));
    if (videoPtr->platformData != NULL) {
	memset(videoPtr->platformData, 0, sizeof(VideoPlatformData));
    } else {
	Tcl_Panic("out of memory");
    }
    return TCL_OK;
}

void 
VideopDestroy(char *memPtr)
{
    Video *videoPtr = (Video *)memPtr;
    if (videoPtr->platformData != NULL) {
	VideoPlatformData *p = (VideoPlatformData *)videoPtr->platformData;
	if (p->pFilterGraph != NULL) {
	    UnregisterFilterGraph(p->dwRegistrationId);
	    p->pFilterGraph->Release();
	    p->pFilterGraph = NULL;
	}

	ckfree((char *)videoPtr->platformData);
	videoPtr->platformData = NULL;
    }
}

int
InitVideoSource(Video *videoPtr)
{
    VideoPlatformData *pPlatformData = (VideoPlatformData *)videoPtr->platformData;
    IVideoWindow *pVideoWindow = 0;
    HRESULT hr = S_OK;
    int device = 0;


    if (pPlatformData->pFilterGraph) {
	UnregisterFilterGraph(pPlatformData->dwRegistrationId);
	pPlatformData->pFilterGraph->Release();
	pPlatformData->pFilterGraph = NULL;
    }

    if (Tcl_GetIntFromObj(NULL, videoPtr->sourcePtr, &device) == TCL_OK) {
	hr = ConstructCaptureGraph(device, &pPlatformData->pFilterGraph);
    } else {
	hr = ConstructFileGraph(Tcl_GetUnicode(videoPtr->sourcePtr), &pPlatformData->pFilterGraph);
    }

    if (SUCCEEDED(hr)) {
	RegisterFilterGraph(pPlatformData->pFilterGraph, &pPlatformData->dwRegistrationId);

	hr = ConnectVideo(pPlatformData->pFilterGraph,
	    Tk_GetHWND(Tk_WindowId(videoPtr->tkwin)), &pVideoWindow);
	if (SUCCEEDED(hr)) {

	    if (SUCCEEDED(hr)) {
		CComPtr<IMediaControl> pMediaControl;
		hr = pPlatformData->pFilterGraph->QueryInterface(IID_IMediaControl,
		    reinterpret_cast<void**>(&pMediaControl));
		//if (SUCCEEDED(hr))
		//    hr = pMediaControl->Run();
	    }

	    long w, h;
	    hr = GetVideoSize(pPlatformData->pFilterGraph, &w, &h);

	    pVideoWindow->put_BorderColor(0xffffff);

	    // /* FIX ME */
	    //pVideoWindow->put_Width(w);
	    //pVideoWindow->put_Height(h);

	    videoPtr->videoHeight = h;
	    videoPtr->videoWidth = w;
	    pVideoWindow->Release();
	}
    }

    return TCL_OK;
}

void
VideopCalculateGeometry(Video *videoPtr)
{
    VideoPlatformData *pPlatformData = (VideoPlatformData *)videoPtr->platformData;
    int width, height;

    if (videoPtr->stretch) {
	width = Tk_ReqWidth(videoPtr->tkwin);
	height = Tk_ReqHeight(videoPtr->tkwin);
    } else {
	width = videoPtr->videoWidth;
	height = videoPtr->videoHeight;
    }

    if (pPlatformData && pPlatformData->pFilterGraph) {
	CComPtr<IVideoWindow> pVideoWindow;
	if (SUCCEEDED( pPlatformData->pFilterGraph->QueryInterface(&pVideoWindow) )) {
	    pVideoWindow->put_Width(width);
	    pVideoWindow->put_Height(height);
	    if (videoPtr->offset.x > 0)
		pVideoWindow->put_Left(-videoPtr->offset.x);
	    if (videoPtr->offset.y > 0)
		pVideoWindow->put_Top(-videoPtr->offset.y);
	}
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
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, "option");
	    r = TCL_ERROR;
	} else {
	    r = GetDeviceList(interp);
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

	CComPtr<IMediaControl> pMediaControl;
	CComPtr<IVideoWindow>  pVideoWindow;
	HRESULT hr = pFilterGraph->QueryInterface(IID_IMediaControl, reinterpret_cast<void**>(&pMediaControl));
	if (SUCCEEDED(hr))
	    hr = pFilterGraph->QueryInterface(IID_IVideoWindow, reinterpret_cast<void**>(&pVideoWindow));
	if (SUCCEEDED(hr)) {
	    switch (index) {
	    case VIDEO_START:
		hr = pMediaControl->Run();
		if (SUCCEEDED(hr))
		    hr = pVideoWindow->put_Visible(OATRUE);
		break;
	    case VIDEO_PAUSE:
		hr = pMediaControl->Pause();
		if (SUCCEEDED(hr))
		    hr = pVideoWindow->put_Visible(OATRUE);
		break;

	    case VIDEO_STOP:
		hr = pMediaControl->Stop();
		if (SUCCEEDED(hr))
		    pVideoWindow->put_Visible(OAFALSE);
		break;		
	    }
	}
	r = (long)hr;
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

static int 
GetDeviceList(Tcl_Interp *interp)
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

        hr = pCreateDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnumMoniker, 0);
	while (SUCCEEDED(hr) && hrLoop == S_OK)
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
    return SUCCEEDED(hr) ? TCL_OK : TCL_ERROR;;

}

// -----------------------------------------------------------------
static HRESULT 
ConstructFileGraph(LPOLESTR sFilename, IGraphBuilder **ppGraphBuilder)
{
    CComPtr<IGraphBuilder> pGraphBuilder;
    HRESULT hr = pGraphBuilder.CoCreateInstance(CLSID_FilterGraph);

    // Add in a Sample Grabber to the graph
    CComPtr<IBaseFilter> pGrabberFilter;
    if (SUCCEEDED(hr))
        hr = pGrabberFilter.CoCreateInstance(CLSID_SampleGrabber);
    if (SUCCEEDED(hr))
        hr = pGraphBuilder->AddFilter(pGrabberFilter, SAMPLE_GRABBER_NAME);
    if (SUCCEEDED(hr))
    {
	// Set the grabber to grab video stills.
	AM_MEDIA_TYPE mt;
	ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
	mt.majortype = MEDIATYPE_Video;
	int iBitDepth = GetDeviceCaps(::GetDC(HWND_DESKTOP), BITSPIXEL);
	switch (iBitDepth) {
	case  8: mt.subtype = MEDIASUBTYPE_RGB8;   break;
	case 24: mt.subtype = MEDIASUBTYPE_RGB24;  break;
	case 32: mt.subtype = MEDIASUBTYPE_RGB32;  break;
	default: mt.subtype = MEDIASUBTYPE_RGB555; break;
	}

	CComQIPtr<ISampleGrabber> pSampleGrabber(pGrabberFilter);
	if (pSampleGrabber)
	    hr = pSampleGrabber->SetMediaType(&mt);
    }

    // Add a video renderer to the graph
    CComPtr<IBaseFilter> pRenderFilter;
    if (SUCCEEDED(hr))
        hr = pRenderFilter.CoCreateInstance(CLSID_VideoRenderer);
    if (SUCCEEDED(hr))
        hr = pGraphBuilder->AddFilter(pRenderFilter, RENDERER_FILTER_NAME);
    
    // begin rendering our file.
    if (SUCCEEDED(hr))
	hr = pGraphBuilder->RenderFile(sFilename, NULL);
    if (SUCCEEDED(hr))
	hr = pGraphBuilder.CopyTo(ppGraphBuilder);
    return hr;
}

/**
 *  Build a DirectX Video Capture graph for the given video input device.
 *  The graph will contain a Sample Grabber and a VideoRenderer.
 */
static HRESULT
ConstructCaptureGraph(int DeviceIndex, IGraphBuilder **ppGraphBuilder)
{
    CComPtr<IGraphBuilder> pGraphBuilder;
    CComPtr<IMediaControl> pMediaControl;
    CComPtr<IMoniker> pmk;
    CComPtr<IBindCtx> pBindContext;
    CComPtr<IBaseFilter> pCaptureFilter;

    HRESULT hr = pGraphBuilder.CoCreateInstance(CLSID_FilterGraph);

    // Add our input device to the capture graph.
    if (SUCCEEDED(hr))
        hr = GetDeviceMoniker(DeviceIndex, &pmk);
    if (SUCCEEDED(hr))
        hr = CreateBindCtx(0, &pBindContext);
    if (SUCCEEDED(hr))
        hr = pmk->BindToObject(pBindContext, NULL, IID_IBaseFilter, reinterpret_cast<void**>(&pCaptureFilter));
    if (SUCCEEDED(hr))
    {
        CComPtr<IPropertyBag> pCaptureBag;
        CComVariant vCaptureName;
        if (SUCCEEDED( pmk->BindToStorage(pBindContext, NULL, IID_IPropertyBag, reinterpret_cast<void**>(&pCaptureBag)) ))
            pCaptureBag->Read(L"FriendlyName", &vCaptureName, 0);

        hr = pGraphBuilder->AddFilter(pCaptureFilter, CAPTURE_FILTER_NAME);
    }

    // Add a video renderer to the graph
    CComPtr<IBaseFilter> pRenderFilter;
    if (SUCCEEDED(hr))
        hr = pRenderFilter.CoCreateInstance(CLSID_VideoRenderer);
    if (SUCCEEDED(hr))
        hr = pGraphBuilder->AddFilter(pRenderFilter, RENDERER_FILTER_NAME);

    // Add in a Sample Grabber to the graph
    CComPtr<IBaseFilter> pGrabberFilter;
    if (SUCCEEDED(hr))
        hr = pGrabberFilter.CoCreateInstance(CLSID_SampleGrabber);
    if (SUCCEEDED(hr))
        hr = pGraphBuilder->AddFilter(pGrabberFilter, SAMPLE_GRABBER_NAME);

    if (SUCCEEDED(hr))
        hr = ConnectFilterGraph(pGraphBuilder, pCaptureFilter, pGrabberFilter, pRenderFilter);

    // Return the completed capture graph
    if (SUCCEEDED(hr))
        hr = pGraphBuilder.CopyTo(ppGraphBuilder);

    return hr;
}

/**
 *  Reconnect the filter graph by keeping a reference to our filters and throwing away 
 *  the current graph. We then create a new graph with out filters and connect it.
 */

HRESULT
ReconnectFilterGraph(IGraphBuilder *pGraphBuilder)
{
    ATLASSERT(pGraphBuilder);
    HRESULT hr = S_OK;

    CComPtr<ICaptureGraphBuilder2> pCaptureGraphBuilder2;
    CComPtr<IBaseFilter> pCaptureFilter, pGrabberFilter, pRenderFilter;

    if (SUCCEEDED(hr))
        hr = pGraphBuilder->FindFilterByName(CAPTURE_FILTER_NAME, &pCaptureFilter);
    if (SUCCEEDED(hr))
        hr = pGraphBuilder->FindFilterByName(SAMPLE_GRABBER_NAME, &pGrabberFilter);
    if (SUCCEEDED(hr))
        hr = pGraphBuilder->FindFilterByName(RENDERER_FILTER_NAME, &pRenderFilter);

    // Remove all filters from the graph.
    if (SUCCEEDED(hr))
    {
        CComPtr<IEnumFilters> pEnumFilters;
        IBaseFilter *pFilters[12];
        ULONG nFilters = 0;
        HRESULT hrLoop = S_OK;
        hr = pGraphBuilder->EnumFilters(&pEnumFilters);
        while (SUCCEEDED(hr) && hrLoop == S_OK)
        {
            hr = hrLoop = pEnumFilters->Next(12, pFilters, &nFilters);
            for (unsigned long n = 0; SUCCEEDED(hr) && n < nFilters; n++)
            {
                hr = pGraphBuilder->RemoveFilter(pFilters[n]);
                pFilters[n]->Release(), pFilters[n] = 0;
            }
            if (hrLoop == S_OK)
                hr = pEnumFilters->Reset();
        }
    }
    
    // Re-add our filters.
    if (SUCCEEDED(hr))
        hr = pGraphBuilder->AddFilter(pCaptureFilter, CAPTURE_FILTER_NAME);
    if (SUCCEEDED(hr))
        hr = pGraphBuilder->AddFilter(pGrabberFilter, SAMPLE_GRABBER_NAME);
    if (SUCCEEDED(hr))
        hr = pGraphBuilder->AddFilter(pRenderFilter, RENDERER_FILTER_NAME);

    if (SUCCEEDED(hr))
        hr = ConnectFilterGraph(pGraphBuilder, pCaptureFilter, pGrabberFilter, pRenderFilter);

    return hr;
}

static HRESULT
ConnectFilterGraph(IGraphBuilder *pGraphBuilder,
                   IBaseFilter *pCaptureFilter, IBaseFilter *pGrabberFilter, IBaseFilter *pRenderFilter)
{
    HRESULT hr = S_OK;

    AM_MEDIA_TYPE mt;
    ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
    mt.majortype = MEDIATYPE_Video;
    HDC hdc = ::GetDC(HWND_DESKTOP);
    int iBitDepth = GetDeviceCaps(hdc, BITSPIXEL);
    ::ReleaseDC(HWND_DESKTOP, hdc);

    switch (iBitDepth) {
    case  8: mt.subtype = MEDIASUBTYPE_RGB8;   break;
    case 24: mt.subtype = MEDIASUBTYPE_RGB24;  break;
    case 32: mt.subtype = MEDIASUBTYPE_RGB32;  break;
    default: mt.subtype = MEDIASUBTYPE_RGB565; break;
    }

    CComPtr<IPin> pCapturePin, pRenderPin, pGrabIn, pGrabOut;
    if (SUCCEEDED(hr))
        hr = FindPinByCategory(pCaptureFilter, PIN_CATEGORY_CAPTURE, &pCapturePin);
    if (SUCCEEDED(hr))
        hr = FindPinByDirection(pRenderFilter, PINDIR_INPUT, &pRenderPin);
    if (SUCCEEDED(hr))
        hr = FindPinByDirection(pGrabberFilter, PINDIR_INPUT, &pGrabIn);
    if (SUCCEEDED(hr))
        hr = FindPinByDirection(pGrabberFilter, PINDIR_OUTPUT, &pGrabOut);

    CComQIPtr<ISampleGrabber> pSampleGrabber(pGrabberFilter);
    if (pSampleGrabber)
        hr = pSampleGrabber->SetMediaType(&mt);

    if (SUCCEEDED(hr))
        hr = pGraphBuilder->Connect(pCapturePin, pGrabIn);
    if (SUCCEEDED(hr))
        hr = pGraphBuilder->Connect(pGrabOut, pRenderPin);

    if (SUCCEEDED(hr))
    {
        // The point of this section is to get any upstream filters placed into 
        // the graph for us. We don't especially care if the capture filter supports
        // compression, because the smart connect will handle this for us. However,
        // we do need the CaptureGraphBuilder to build the upstream part of the graph
        // otherwise it won't give us the crossbar filter later on.

        CComPtr<ICaptureGraphBuilder2> pCaptureGraphBuilder2;
        CComPtr<IBaseFilter> pFilter;
        CComPtr<IAMVideoCompression> pCompression;

        hr = pCaptureGraphBuilder2.CoCreateInstance(CLSID_CaptureGraphBuilder2);
        if (SUCCEEDED(hr))
            hr = pCaptureGraphBuilder2->SetFiltergraph(pGraphBuilder);
        if (SUCCEEDED(hr))
            pCaptureGraphBuilder2->FindInterface(&PIN_CATEGORY_CAPTURE,
                                                 &MEDIATYPE_Video,
                                                 pCaptureFilter,
                                                 IID_IAMVideoCompression,
                                                 reinterpret_cast<void**>(&pCompression));
    }

    ATLASSERT(SUCCEEDED(hr) && _T("Unable to connect capture pin in ConnectFilterGraph"));
    return hr;
}

/**
 * Connect the capture graph to a window. This window needs
 * to adjust itself for the correct size.
 */
HRESULT
ConnectVideo(IGraphBuilder *pGraphBuilder, HWND hwnd, IVideoWindow **ppVideoWindow)
{
    CComPtr<IVideoWindow> pVideoWindow;
    CComPtr<IMediaEventEx> pMediaEvent;
    RECT rc;
    
    HRESULT hr = E_INVALIDARG;
    if (::IsWindow(hwnd))
    {
        ::GetClientRect(hwnd, &rc);

        hr = pGraphBuilder->QueryInterface(&pMediaEvent);
        if (SUCCEEDED(hr))
            hr = pGraphBuilder->QueryInterface(&pVideoWindow);
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


HRESULT
GetVideoSize(IGraphBuilder *pFilterGraph, long *pWidth, long *pHeight)
{
    HRESULT hr = S_FALSE;
    if (pFilterGraph)
    {
        CComPtr<IBaseFilter> pFilter;
        CComPtr<IPin> pPin;
        AM_MEDIA_TYPE mt;

        hr = pFilterGraph->FindFilterByName(RENDERER_FILTER_NAME, &pFilter);
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
 *  Disconnect all the connected pins for each filter.
 */

HRESULT
DisconnectFilterGraph(IFilterGraph *pFilterGraph)
{
    CComPtr<IEnumFilters> pEnumFilters;
    HRESULT hr = pFilterGraph->EnumFilters(&pEnumFilters);
    if (SUCCEEDED(hr))
    {
        IBaseFilter *pFilter;
        while ((hr = pEnumFilters->Next(1, &pFilter, NULL)) == S_OK)
        {
            hr = DisconnectPins(pFilter);
            pFilter->Release();
        }
    }
    return hr;
}

/**
 *  Disconnect all pins on the filter.
 */

HRESULT
DisconnectPins(IBaseFilter *pFilter)
{
    CComPtr<IEnumPins> pEnumPins;
    HRESULT hr = pFilter->EnumPins(&pEnumPins);
    if (SUCCEEDED(hr))
    {
        IPin *pPin;
        while ((hr = pEnumPins->Next(1, &pPin, NULL)) == S_OK)
        {
            hr = pPin->Disconnect();
            pPin->Release();
        }
    }
    return hr;
}
