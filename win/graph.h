#ifndef _GRAPH_H_INCLUDE
#define _GRAPH_H_INCLUDE

#define WIN32_LEAN_AND_MEAN
#define OEMRESOURCE 
#include <windows.h>

// The following section allows us to compile using Visual Studio 2008 and
// beyond without requiring the installation of DX9.
#if _MSC_VER < 1500
#include <qedit.h>
#else
#pragma include_alias("dxtrans.h","qedit.h")
#define __IDxtCompositor_INTERFACE_DEFINED__
#define __IDxtAlphaSetter_INTERFACE_DEFINED__
#define __IDxtJpeg_INTERFACE_DEFINED__
#define __IDxtKey_INTERFACE_DEFINED__
#include "dx/qedit.h"
#endif

#ifdef HAVE_WMF_SDK
#include <dshowasf.h>
#endif // HAVE_WMF_SDK

#include "dshow_utils.h"

typedef enum EFilterIndices {
    CaptureFilterIndex,
    AudioFilterIndex,
    SampleGrabberIndex,
    MuxFilterIndex,
    FileSinkIndex,
    RendererFilterIndex,
    StillGrabberIndex,
    StillRendererIndex,
    CustomFilterIndex,
} EFilterIndices;

typedef struct GraphSpecification {
    int nDeviceIndex;
    int nAudioIndex;
    BOOL bAudioRequired;
    IBaseFilter *aFilters[9];
    WCHAR wszSourcePath[MAX_PATH];
    WCHAR wszOutputPath[MAX_PATH];
} GraphSpecification;

HRESULT ConstructCaptureGraph(GraphSpecification *pSpec, IGraphBuilder **ppGraphBuilder);
HRESULT ConnectFilterGraph(GraphSpecification *pSpec, IGraphBuilder *pGraphBuilder);
HRESULT ReconnectFilterGraph(GraphSpecification *pSpec, IGraphBuilder *pGraphBuilder);
HRESULT RemoveFiltersFromGraph(IFilterGraph *pFilterGraph);
HRESULT DisconnectFilterGraph(IGraphBuilder *pGraphBuilder);
HRESULT DisconnectPins(IBaseFilter *pFilter);
HRESULT GetCaptureMediaFormat(IGraphBuilder *pGraph, int index, AM_MEDIA_TYPE **ppmt);
void FreeMediaType(AM_MEDIA_TYPE *pmt);

#endif /* _GRAPH_H_INCLUDE */
