#include "graph.h"

#if _MSC_VER >= 100
#pragma comment(lib, "amstrmid")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "shlwapi")
#endif

static HRESULT CreateCompatibleSampleGrabber(IBaseFilter **ppFilter);
static HRESULT MediaType(LPCWSTR sPath, LPCGUID *ppMediaType, LPCGUID *ppMediaSubType);

HRESULT 
ConstructCaptureGraph(GraphSpecification *pSpec, IGraphBuilder **ppGraph)
{
    CComPtr<IGraphBuilder> pGraph;
    CComPtr<ICaptureGraphBuilder2> pBuilder;
    bool bFileSource = (wcslen(pSpec->wszSourcePath) > 0);
    bool bRecordVideo = (wcslen(pSpec->wszOutputPath) > 0);
    pSpec->bAudioRequired = FALSE;

    HRESULT hr = pGraph.CoCreateInstance(CLSID_FilterGraph);
    if (SUCCEEDED(hr))
        hr = pBuilder.CoCreateInstance(CLSID_CaptureGraphBuilder2);
    if (SUCCEEDED(hr))
        hr = pBuilder->SetFiltergraph(pGraph);

    // If we are to capture the output to a file, then add in the avimux filter
    // and set the output file destination. Returns pointers to the mux and the
    // file writer that it creates for us. Can create AVI or ASF
    CComPtr<IFileSinkFilter> pFileSink;
    if (SUCCEEDED(hr) && !bFileSource && bRecordVideo)
    {
        LPCGUID pType = NULL, pSubType = NULL;
        hr = MediaType(pSpec->wszOutputPath, &pType, &pSubType);
        if (SUCCEEDED(hr) && pSubType == &MEDIASUBTYPE_Asf)
            pSpec->bAudioRequired = TRUE; // the asf writer _requires_ an audio input.
        if (SUCCEEDED(hr))
            hr = pBuilder->SetOutputFileName(pSubType, pSpec->wszOutputPath, &pSpec->aFilters[MuxFilterIndex], &pFileSink);
#ifdef HAVE_WMF_SDK
        if (SUCCEEDED(hr) && pSubType == &MEDIASUBTYPE_Asf)
        {
            CComPtr<IConfigAsfWriter> pConfig;
            pSpec->aFilters[MuxFilterIndex]->QueryInterface(&pConfig);
        }
#endif // HAVE_WMF_SDK
    }

    // Now we put in the source filter. This may be a file source, url source or
    // a capture device.
    if (SUCCEEDED(hr))
    {
        if (bFileSource)
        {
            hr = pGraph->AddSourceFilter(pSpec->wszSourcePath, CAPTURE_FILTER_NAME, &pSpec->aFilters[CaptureFilterIndex]);
        }
        else
        {
            CComPtr<IMoniker> pmk;
            CComPtr<IBindCtx> pBindContext;

            // Add our input device to the capture graph.
            if (SUCCEEDED(hr))
                hr = GetDeviceMoniker(CLSID_VideoInputDeviceCategory, pSpec->nDeviceIndex, &pmk);
            if (!pmk)
                hr = E_INVALIDARG;
            if (SUCCEEDED(hr))
                hr = CreateBindCtx(0, &pBindContext);
            if (SUCCEEDED(hr))
                hr = pmk->BindToObject(pBindContext, NULL, IID_IBaseFilter, reinterpret_cast<void**>(&pSpec->aFilters[CaptureFilterIndex]));
            if (SUCCEEDED(hr))
                hr = pGraph->AddFilter(pSpec->aFilters[CaptureFilterIndex], CAPTURE_FILTER_NAME);

            pmk.Release(); pBindContext.Release();
            if (SUCCEEDED(hr) && pSpec->nAudioIndex != -1)
            {
                HRESULT hrA = GetDeviceMoniker(CLSID_AudioInputDeviceCategory, pSpec->nAudioIndex, &pmk);
                if (SUCCEEDED(hrA))
                    hrA = CreateBindCtx(0, &pBindContext);
                if (SUCCEEDED(hrA))
                    hrA = pmk->BindToObject(pBindContext, NULL, IID_IBaseFilter, reinterpret_cast<void**>(&pSpec->aFilters[AudioFilterIndex]));
                if (SUCCEEDED(hrA))
                    hrA = pGraph->AddFilter(pSpec->aFilters[AudioFilterIndex], AUDIO_FILTER_NAME);
            }
        }
    }

    // Add in a Sample Grabber to the graph
    if (SUCCEEDED(hr))
    {
        hr = CreateCompatibleSampleGrabber(&pSpec->aFilters[SampleGrabberIndex]);
        if (SUCCEEDED(hr))
            hr = pGraph->AddFilter(pSpec->aFilters[SampleGrabberIndex], SAMPLE_GRABBER_NAME);
    }

    // Add a video renderer to the graph
    if (SUCCEEDED(hr))
    {
        //hr = pRenderFilter.CoCreateInstance(CLSID_VideoMixingRenderer9);
        hr = CoCreateInstance(CLSID_VideoRendererDefault, NULL, CLSCTX_ALL, IID_IBaseFilter, (void **)&pSpec->aFilters[RendererFilterIndex]);
        if (FAILED(hr))
            hr = CoCreateInstance(CLSID_VideoRenderer, NULL, CLSCTX_ALL, IID_IBaseFilter, (void **)&pSpec->aFilters[RendererFilterIndex]);
        if (SUCCEEDED(hr))
            hr = pGraph->AddFilter(pSpec->aFilters[RendererFilterIndex], RENDERER_FILTER_NAME);
        if (SUCCEEDED(hr))
        {
            CComPtr<IVMRFilterConfig> pVMRFilterConfig;
            HRESULT hrx = pSpec->aFilters[RendererFilterIndex]->QueryInterface(&pVMRFilterConfig);
            if (SUCCEEDED(hrx))
                hrx = pVMRFilterConfig->SetNumberOfStreams(2);
        }
    }

    // Could add in a compressor here.

    if (SUCCEEDED(hr))
        hr = ConnectFilterGraph(pSpec, pGraph);

    if (SUCCEEDED(hr))
        hr = pGraph.CopyTo(ppGraph);
    return hr;
}

HRESULT
ConnectFilterGraph(GraphSpecification *pSpec, IGraphBuilder *pGraphBuilder)
{
    CComPtr<ICaptureGraphBuilder2> pBuilder;
    HRESULT hr = pBuilder.CoCreateInstance(CLSID_CaptureGraphBuilder2);
    if (SUCCEEDED(hr))
        hr = pBuilder->SetFiltergraph(pGraphBuilder);

    bool bFileSource = (wcslen(pSpec->wszSourcePath) > 0);
    bool bRecordVideo = (wcslen(pSpec->wszOutputPath) > 0);

    // Add in a preview
    if (SUCCEEDED(hr))
    {
        CComPtr<IVMRFilterConfig> pVMRFilterConfig;
        HRESULT hrx = pSpec->aFilters[RendererFilterIndex]->QueryInterface(&pVMRFilterConfig);
        if (SUCCEEDED(hrx))
            hrx = pVMRFilterConfig->SetNumberOfStreams(2);

        if (bFileSource) 
        {
            LPCGUID pType = NULL, pSubType = NULL;
            hr = MediaType(pSpec->wszSourcePath, &pType, &pSubType);
            hr = pBuilder->RenderStream(NULL, pType, pSpec->aFilters[CaptureFilterIndex],
                pSpec->aFilters[SampleGrabberIndex], pSpec->aFilters[RendererFilterIndex]);
        }
        else
        {
            hr = pBuilder->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, pSpec->aFilters[CaptureFilterIndex],
                pSpec->aFilters[SampleGrabberIndex], pSpec->aFilters[RendererFilterIndex]);
            if (hr == VFW_E_CANNOT_CONNECT && pVMRFilterConfig != NULL)
            {
                // It's possible that the graph might fail to render on the VMR filter but will be ok when the rederer is non-mixing.
                // (this is the case for the Infinity camera). So lets try without enabling mixing in this case.
                hr = pGraphBuilder->RemoveFilter(pSpec->aFilters[RendererFilterIndex]);
                pSpec->aFilters[RendererFilterIndex]->Release();
                if (SUCCEEDED(hr))
                    hr = CoCreateInstance(CLSID_VideoRendererDefault, NULL, CLSCTX_ALL, IID_IBaseFilter, (void **)&pSpec->aFilters[RendererFilterIndex]);
                if (SUCCEEDED(hr))
                    hr = pGraphBuilder->AddFilter(pSpec->aFilters[RendererFilterIndex], RENDERER_FILTER_NAME);
                if (SUCCEEDED(hr))
                    hr = pBuilder->RenderStream(NULL, NULL, pSpec->aFilters[SampleGrabberIndex], NULL, pSpec->aFilters[RendererFilterIndex]);
            }
        }
    }

    // Now connect up the video source to the mux for saving. (The mux is already connected to the writer).
    if (SUCCEEDED(hr) && pSpec->aFilters[MuxFilterIndex]) {
        hr = pBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video,
            pSpec->aFilters[CaptureFilterIndex], NULL, pSpec->aFilters[MuxFilterIndex]);
    }

    // Connect audio (if any)
    if (SUCCEEDED(hr))
    {
        if (pSpec->bAudioRequired|| (pSpec->aFilters[AudioFilterIndex] && pSpec->aFilters[MuxFilterIndex]))
        {
            CComPtr<IBaseFilter> pTeeFilter;
            pTeeFilter.CoCreateInstance(CLSID_InfTee);
            if (pTeeFilter) pGraphBuilder->AddFilter(pTeeFilter, NULL);
            IBaseFilter *pAudioFilter = pSpec->aFilters[AudioFilterIndex];
            if (!pAudioFilter) pAudioFilter = pSpec->aFilters[CaptureFilterIndex];
            if (pSpec->aFilters[MuxFilterIndex])
                pBuilder->RenderStream(NULL, &MEDIATYPE_Audio, pSpec->aFilters[AudioFilterIndex], pTeeFilter, pSpec->aFilters[MuxFilterIndex]);
        }
        else if (pSpec->aFilters[AudioFilterIndex])
        {
            pBuilder->RenderStream(NULL, &MEDIATYPE_Audio, pSpec->aFilters[AudioFilterIndex], NULL, NULL);
        }
        else
        {
            pBuilder->RenderStream(NULL, &MEDIATYPE_Audio, pSpec->aFilters[CaptureFilterIndex], NULL, NULL);
        }
    }

#ifdef USE_STILL_PIN
    // If the capture device has a still pin then hook it up.
    {
        CComPtr<IPin> pStillPin;
        HRESULT hr = FindPinByCategory(pCaptureFilter, PIN_CATEGORY_STILL, &pStillPin);
        if (SUCCEEDED(hr) && pStillPin)
        {
            CComPtr<IBaseFilter> pStillGrabber;
            CComPtr<IBaseFilter> pStillRenderer;
            hr = pStillGrabber.CoCreateInstance(CLSID_SampleGrabber);
            if (SUCCEEDED(hr))
                hr = pStillRenderer.CoCreateInstance(CLSID_NullRenderer);
            if (SUCCEEDED(hr))
                hr = pGraph->AddFilter(pStillGrabber, STILL_GRABBER_NAME);
            if (SUCCEEDED(hr))
                hr = pGraph->AddFilter(pStillRenderer, STILL_RENDERER_NAME);
            if (SUCCEEDED(hr))
            {
                CComQIPtr<ISampleGrabber> pSampleGrabber(pStillGrabber);
                if (pSampleGrabber)
                {
                    hr = pSampleGrabber->SetOneShot(FALSE);
                    hr = pSampleGrabber->SetBufferSamples(TRUE);

                    AM_MEDIA_TYPE *pmt = NULL;
                    hr = GetCaptureMediaFormat(pGraph, -1, &pmt);
                    if (SUCCEEDED(hr))
                    {
                        hr = pSampleGrabber->SetMediaType(pmt);
                        if (SUCCEEDED(hr))
                            hr = pBuilder->RenderStream(&PIN_CATEGORY_STILL, &MEDIATYPE_Video,
                                        pCaptureFilter, pStillGrabber, pStillRenderer);
                        FreeMediaType(pmt);
                    }
                }
            }
        }
    }
#endif /* USE_STILL_PIN */

    return hr;
}
    
/**
 *  Disconnect all the connected pins for each filter.
 */

HRESULT
DisconnectFilterGraph(IGraphBuilder *pGraphBuilder)
{
    CComPtr<IEnumFilters> pEnumFilters;
    HRESULT hr = pGraphBuilder->EnumFilters(&pEnumFilters);
    if (SUCCEEDED(hr))
    {
        IBaseFilter *pFilter = NULL;
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
        // For capture to Tk we want to force RGB32. The image conversion code
        // can deal with RGB24 and RGB32 but not the other formats.
        mt.subtype = MEDIASUBTYPE_RGB32;
        CComQIPtr<ISampleGrabber> pSampleGrabber(pGrabberFilter);
        if (pSampleGrabber)
            hr = pSampleGrabber->SetMediaType(&mt);
        if (SUCCEEDED(hr))
            hr = pGrabberFilter.CopyTo(ppFilter);
    }
    return hr;
}

HRESULT
GetCaptureMediaFormat(IGraphBuilder *pGraph, int index, AM_MEDIA_TYPE **ppmt)
{
    CComPtr<IBaseFilter> pGrabberFilter;
    CComPtr<IAMStreamConfig> pConfig;

    HRESULT hr = FindGraphInterface(pGraph, CAPTURE_FILTER_NAME, IID_IAMStreamConfig, (void**)&pConfig);
    if (SUCCEEDED(hr))
    {
        int nCount = 0, nSize = 0;
        hr = pConfig->GetNumberOfCapabilities(&nCount, &nSize);

        // If we were passed an index then return that media type. Otherwise we search for
        // the best (ie: biggest) one.
        if (index != -1)
        {
            VIDEO_STREAM_CONFIG_CAPS caps;
            hr = pConfig->GetStreamCaps(index, ppmt, (LPBYTE)&caps);
        }
        else
        {
            AM_MEDIA_TYPE *pMediaType = 0;
            ULONG nVideoSize = 0;

            for (int n = 0; SUCCEEDED(hr) && n < nCount; n++)
            {
                VIDEO_STREAM_CONFIG_CAPS caps;
                AM_MEDIA_TYPE *pmt = 0;
                ULONG nSize = 0;
                hr = pConfig->GetStreamCaps(n, &pmt, (LPBYTE)&caps);
                if (SUCCEEDED(hr))
                {
                    //ATLTRACE(_T("caps: %d cx: %d cy: %d\n"), n, caps.MaxOutputSize.cx, caps.MaxOutputSize.cy);
                    if (pmt->formattype == FORMAT_VideoInfo && pmt->cbFormat > 0)
                    {
                        VIDEOINFOHEADER *pvih = (VIDEOINFOHEADER *)pmt->pbFormat;
                        ATLTRACE(_T("pmt : %d cx: %d cy:%d bitcnt: %d\n"),
                            n, pvih->bmiHeader.biWidth, pvih->bmiHeader.biHeight, pvih->bmiHeader.biBitCount);
                        nSize = pvih->bmiHeader.biWidth * pvih->bmiHeader.biHeight;
                    }
                    if (nSize > nVideoSize)
                    {
                        ATLTRACE(_T("use index %d for return\n"), n);
                        FreeMediaType(pMediaType);
                        pMediaType = pmt;
                        nVideoSize = nSize;
                    }
                    else 
                    {
                        FreeMediaType(pmt);
                        if (nSize == nVideoSize && nSize != 0)
                            break;
                    }
                }
            }
            *ppmt = pMediaType;
        }
    }

    return hr;
}

void
FreeMediaType(AM_MEDIA_TYPE *pmt)
{
    if (pmt)
    {
        if (pmt->cbFormat)
            CoTaskMemFree(pmt->pbFormat);
        CoTaskMemFree(pmt);
    }
}

HRESULT
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

