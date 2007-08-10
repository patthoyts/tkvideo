#ifndef _GRAPH_H_INCLUDE
#define _GRAPH_H_INCLUDE

#define WIN32_LEAN_AND_MEAN
#define OEMRESOURCE 
#include <windows.h>
#include <qedit.h>

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
    StillRendererIndex
} EFilterIndices;

typedef struct GraphSpecification {
    int nDeviceIndex;
    int nAudioIndex;
    BOOL bAudioRequired;
    IBaseFilter *aFilters[8];
    WCHAR wszSourcePath[MAX_PATH];
    WCHAR wszOutputPath[MAX_PATH];
} GraphSpecification;

HRESULT ConstructCaptureGraph(GraphSpecification *pSpec, IGraphBuilder **ppGraphBuilder);
HRESULT ConnectFilterGraph(GraphSpecification *pSpec, IGraphBuilder *pGraphBuilder);
HRESULT DisconnectFilterGraph(IGraphBuilder *pGraphBuilder);
HRESULT DisconnectPins(IBaseFilter *pFilter);
HRESULT GetCaptureMediaFormat(IGraphBuilder *pGraph, int index, AM_MEDIA_TYPE **ppmt);
void FreeMediaType(AM_MEDIA_TYPE *pmt);

#endif /* _GRAPH_H_INCLUDE */
