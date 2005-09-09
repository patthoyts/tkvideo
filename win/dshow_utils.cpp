#include "dshow_utils.h"
#include <wchar.h>

/**
 *  A general purpose property page display function.
 *
 *  @param Object  the object to show property pages for.
 *  @param Caption the property page title -- do not append 'Properties'
 *  @param hwnd    the hwnd for the parent window (can be NULL).
 */

HRESULT
ShowPropertyPages(LPUNKNOWN Object, LPCOLESTR Caption, HWND hwnd)
{
    CComPtr<ISpecifyPropertyPages> pPropertyPages;
    HRESULT hr = Object->QueryInterface(&pPropertyPages);
    if (SUCCEEDED(hr))
    {
        CAUUID caPages = {0};
        hr = pPropertyPages->GetPages(&caPages);
        if (SUCCEEDED(hr))
        {
            if (caPages.cElems > 0)
                hr = ::OleCreatePropertyFrame(hwnd, 0, 0, 
		    Caption, 1, &Object, caPages.cElems, caPages.pElems,
		    NULL, 0, NULL);
            ::CoTaskMemFree(caPages.pElems);
        }
    }
    return hr;
}

/**
 *  Get a device moniker from the device index.
 *  This iterates over the set of video input devices and gets the nth device
 *  returning the COM moniker.
 */

HRESULT
GetDeviceMoniker(int DeviceIndex, IMoniker **ppMoniker)
{
    CComPtr<ICreateDevEnum> pCreateDevEnum;

    if (DeviceIndex < 0)
        return E_INVALIDARG;

    HRESULT hr = pCreateDevEnum.CoCreateInstance(CLSID_SystemDeviceEnum);
    if (SUCCEEDED(hr))
    {
	CComPtr<IEnumMoniker> pEnumMoniker;
	IMoniker *pmks[12];
	ULONG nmks = 0;
	HRESULT hrLoop = S_OK;

        // bug #4992: this returns S_FALSE and sets the pointer NULL on failure.
        hr = pCreateDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnumMoniker, 0);
	while (SUCCEEDED(hr) && pEnumMoniker && hrLoop == S_OK)
	{
	    hr = hrLoop = pEnumMoniker->Next(12, pmks, &nmks);
	    for (ULONG n = 0; SUCCEEDED(hr) && n < nmks; n++)
	    {
		if (n == (ULONG)DeviceIndex) {
		    *ppMoniker = pmks[n];
		    (*ppMoniker)->AddRef();
		    hrLoop = S_FALSE;
		}
		pmks[n]->Release(),  pmks[n] = 0;
	    }
	    if (hrLoop == S_OK)
		hr = pEnumMoniker->Reset();
	}
        if (SUCCEEDED(hr))
            hr = (hrLoop == S_FALSE) ? S_OK : E_INVALIDARG;
    }
    return hr;
}

HRESULT
GetDeviceName(int DeviceIndex, BSTR *pstrName)
{
    CComPtr<IMoniker> pmk;
    CComPtr<IBindCtx> pctx;
    CComPtr<IPropertyBag> pbag;
    CComVariant v;

    HRESULT hr = GetDeviceMoniker(DeviceIndex, &pmk);
    if (SUCCEEDED(hr))
        hr = CreateBindCtx(0, &pctx);
    if (SUCCEEDED(hr))
        hr = pmk->BindToStorage(pctx, NULL, IID_IPropertyBag, reinterpret_cast<void**>(&pbag));
    if (SUCCEEDED(hr))
        hr = pbag->Read(L"FriendlyName", &v, NULL);
    if (SUCCEEDED(hr))
        *pstrName = ::SysAllocString(v.bstrVal);
    return hr;
}

HRESULT
GetDeviceID(int DeviceIndex, BSTR *pstrName)
{
    CComPtr<IMoniker> pmk;
    CComPtr<IBindCtx> pctx;

    HRESULT hr = GetDeviceMoniker(DeviceIndex, &pmk);
    if (SUCCEEDED(hr))
        hr = CreateBindCtx(0, &pctx);
    if (SUCCEEDED(hr))
    {
        LPOLESTR ocsz, pstr;
        hr = pmk->GetDisplayName(pctx, NULL, &ocsz);
        if (SUCCEEDED(hr))
        {
            // Get the serial number from the moniker.
            pstr = wcstok(ocsz, L"#");
            for (int n = 0; pstr && n < 2; n++)
                pstr = wcstok(NULL, L"#");
            if (pstr)
                pstr = wcstok(pstr, L"&");
            if (pstr)
                pstr = wcstok(NULL, L"&");
            if (pstr)
                *pstrName = ::SysAllocString(pstr);
            ::CoTaskMemFree(ocsz), ocsz = 0;
        }

        // Under Win9x the above yields a NULL string so lets checksum it.
        if (SUCCEEDED(hr) && pstr == 0)
        {
            hr = pmk->GetDisplayName(pctx, NULL, &ocsz);
            if (SUCCEEDED(hr))
            {
                // Perform a 32 bit checksum on the moniker display name.
                unsigned long sum = 0x67452301;
                LPWSTR psrc = ocsz;
                while (psrc && *psrc)
                {
                    sum = (sum & 1) ? (sum >> 1) + 0x80000000 : (sum >> 1);
                    sum += (unsigned short)(*psrc++);
                }
                _snwprintf(ocsz, wcslen(ocsz), L"%08lx", sum);
                *pstrName = ::SysAllocString(ocsz);
                ::CoTaskMemFree(ocsz);
            }
        }

    }
    return hr;
}


/**
 *  Returns the first pin on the given filter that matches the given 
 *  pin ID or name. ID's are more explicit than names, and IDs should be
 *  locale independent.
 */

HRESULT
FindPinByName(IBaseFilter *pFilter, LPCWSTR sID, LPCWSTR sName, IPin **ppPin)
{
    ATLASSERT(sID || sName);

    CComPtr<IPin> pPin;
    CComPtr<IEnumPins> pEnumPins;
    HRESULT hr = pFilter->EnumPins(&pEnumPins);
    while (SUCCEEDED(hr) && pEnumPins->Next(1, &pPin, 0) == S_OK)
    {
        LPWSTR wstr = 0;
        PIN_INFO info;
        BOOL bMatch = FALSE;

        hr = pPin->QueryId(&wstr);
        if (SUCCEEDED(hr))
        {
            hr = pPin->QueryPinInfo(&info);
            if (SUCCEEDED(hr))
            {
                if (sID)
                    bMatch = wcscmp(sID, wstr) == 0;

                if (! bMatch && sName)
                    bMatch = wcscmp(sName, info.achName) == 0;

                if (info.pFilter)
                    info.pFilter->Release();
            }
            CoTaskMemFree(wstr);
        }

        if (bMatch)
            break;
        pPin.Release();
    }
    if (SUCCEEDED(hr) && pPin)
        hr = pPin.CopyTo(ppPin);
    return hr;
}

/**
 *  Returns the first pin on the filter which has the given category.
 *  Useful for finding the capture and preview pins on a Capture filter.
 */

HRESULT
FindPinByCategory(IBaseFilter *pFilter, REFGUID Category, IPin **ppPin)
{
    CComPtr<IPin> pPin;
    CComPtr<IEnumPins> pEnumPins;
    HRESULT hr = pFilter->EnumPins(&pEnumPins);
    while (SUCCEEDED(hr) && pEnumPins->Next(1, &pPin, 0) == S_OK)
    {
        CComPtr<IKsPropertySet> pKsPropertySet;
        hr = pPin.QueryInterface(&pKsPropertySet);
        if (SUCCEEDED(hr))
        {
            DWORD cbBytes;
            GUID guid = GUID_NULL;
            hr = pKsPropertySet->Get(AMPROPSETID_Pin, AMPROPERTY_PIN_CATEGORY, NULL, 0, &guid, sizeof(GUID), &cbBytes);
            if (IsEqualGUID(guid, Category))
                break;
        }
        pPin.Release();
    }
    if (SUCCEEDED(hr) && pPin)
        hr = pPin.CopyTo(ppPin);
    return hr;
}

/**
 *  Returns the first pin on the filter with the given direction.
 */

HRESULT
FindPinByDirection(IBaseFilter *pFilter, PIN_DIRECTION eDirection, IPin **ppPin)
{
    CComPtr<IPin> pPin;
    CComPtr<IEnumPins> pEnumPins;
    HRESULT hr = pFilter->EnumPins(&pEnumPins);
    while (SUCCEEDED(hr) && pEnumPins->Next(1, &pPin, 0) == S_OK)
    {
        PIN_DIRECTION eDir;
        hr = pPin->QueryDirection(&eDir);
        if (SUCCEEDED(hr) && eDir == eDirection)
            break;
        pPin.Release();
    }
    if (SUCCEEDED(hr) && pPin)
        hr = pPin.CopyTo(ppPin);
    return hr;
}

/**
 *  Find an interface in the graph. This uses the ICaptureGraphBuilder2 object
 *  to search all the filters and pins in the graph for the first one to support
 *  the given interface. This implementation begins at the named filter.
 */

HRESULT
FindGraphInterface(IGraphBuilder *pGraphBuilder, LPCWSTR pstrFilter, REFIID riid, void **ppif)
{
    HRESULT hr = E_POINTER;
    if (ppif)
    {
        CComPtr<ICaptureGraphBuilder2> pCaptureGraphBuilder2;
        CComPtr<IBaseFilter> pCaptureFilter;
        CComPtr<IAMVfwCaptureDialogs>  pCaptureDialogs;

        hr = pCaptureGraphBuilder2.CoCreateInstance(CLSID_CaptureGraphBuilder2);
        if (SUCCEEDED(hr))
            hr = pCaptureGraphBuilder2->SetFiltergraph(pGraphBuilder);
        if (SUCCEEDED(hr))
            hr = pGraphBuilder->FindFilterByName(pstrFilter, &pCaptureFilter);
        if (SUCCEEDED(hr))
            hr = pCaptureGraphBuilder2->FindInterface(NULL, &MEDIATYPE_Video, pCaptureFilter, riid, ppif);
    }
    return hr;
}

/**
 *  Register the capture graph with the ROT so that the GraphEditor app can view
 *  the filter graph.
 */

HRESULT
RegisterFilterGraph(IFilterGraph *pFilterGraph, DWORD *lpdwCookie) 
{
    CComPtr<IRunningObjectTable> pROT;
    CComPtr<IMoniker> pmk;
    WCHAR wsz[128];

    HRESULT hr = ::GetRunningObjectTable(0, &pROT);
    if (SUCCEEDED(hr))
    {
        wsprintfW(wsz, L"FilterGraph %08x pid %08x\0", (DWORD_PTR)pFilterGraph, GetCurrentProcessId());

        hr = CreateItemMoniker(L"!", wsz, &pmk);
        if (SUCCEEDED(hr))
            hr = pROT->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE, pFilterGraph, pmk, lpdwCookie);
    }

    return hr;
}

/**
 *  Unregister an interface from the ROT using the registration cookie.
 */

HRESULT
UnregisterFilterGraph(DWORD dwCookie)
{
    CComPtr<IRunningObjectTable> pROT;
    HRESULT hr = ::GetRunningObjectTable(0, &pROT);
    if (SUCCEEDED(hr))
        hr = pROT->Revoke(dwCookie);
    return hr;
}

/**
 *  Save a filter graph to a .GRF format storage file.
 */

HRESULT
SaveFilterGraph(IFilterGraph *pFilterGraph, BSTR sFilename)
{
    HRESULT hr = E_INVALIDARG;
    if (pFilterGraph)
    {
        CComPtr<IPersistStream> pPersistStream;
        CComPtr<IStorage> pStg;
        CComPtr<IStream> pStm;

        hr = pFilterGraph->QueryInterface(&pPersistStream);
        if (SUCCEEDED(hr))
            hr = StgCreateDocfile(sFilename, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_TRANSACTED, 0, &pStg);
        if (SUCCEEDED(hr))
            hr = pStg->CreateStream(L"ActiveMovieGraph", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &pStm);
        if (SUCCEEDED(hr))
        {
            hr = pPersistStream->Save(pStm, TRUE);
            if (SUCCEEDED(hr))
                hr = pStg->Commit(STGC_DEFAULT);
        }
    }
    return hr;
}

/**
 *  Create a new filter graph and load it from a persisted .GRF file.
 *  Once loaded, you should acquire the video and get it running again.
 */

HRESULT
LoadFilterGraph(BSTR sFilename, IFilterGraph **ppFilterGraph)
{
    CComPtr<IFilterGraph> pFilterGraph;
    CComPtr<IPersistStream> pPersistStream;
    CComPtr<IStorage> pStg;
    CComPtr<IStream> pStm;

    HRESULT hr = pFilterGraph.CoCreateInstance(CLSID_FilterGraph);
    if (SUCCEEDED(hr))
        hr = pFilterGraph->QueryInterface(&pPersistStream);
    if (SUCCEEDED(hr))
        hr = StgOpenStorage(sFilename, NULL, STGM_READ | STGM_SHARE_DENY_WRITE | STGM_TRANSACTED, NULL, 0, &pStg);
    if (SUCCEEDED(hr))
        hr = pStg->OpenStream(L"ActiveMovieGraph", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &pStm);
    if (SUCCEEDED(hr))
        hr = pPersistStream->Load(pStm);
    if (SUCCEEDED(hr))
        hr = pFilterGraph.CopyTo(ppFilterGraph);
    return hr;
}


/*
 * The following is from MSDN knowledge-base article KB230492
 * http://support.microsoft.com/default.aspx?scid=kb;EN-US;230492
 */

/**********************************************************

GetOptimalDIBFormat

  Purpose:
   Retrieves the optimal DIB format for a display 
   device. The optimal DIB format is the format that 
   exactly matches the format of the target device. 
   Obtaining this is very important when dealing with 
   16bpp modes because you need to know what bitfields 
   value to use (555 or 565 for example).

   You normally use this function to get the best
   format to pass to CreateDIBSection() in order to
   maximize blt'ing performance.

  Input:
   hdc - Device to get the optimal format for.
   pbi - Pointer to a BITMAPINFO + color table
         (room for 256 colors is assumed).

  Output:
   pbi - Contains the optimal DIB format. In the 
         <= 8bpp case, the color table will contain the 
         system palette. In the >=16bpp case, the "color 
         table" will contain the correct bit fields (see 
         BI_BITFIELDS in the Platform SDK documentation 
         for more information).

  Notes:
   If you are going to use this function on a 8bpp device
   you should make sure the color table contains a identity
   palette for optimal blt'ing.

**********************************************************/ 
static BOOL 
GetOptimalDIBFormat(HDC hdc, BITMAPINFOHEADER *pbi)
{
    HBITMAP hbm;
    BOOL bRet = TRUE;
    
    // Create a memory bitmap that is compatible with the
    // format of the target device.
    hbm = CreateCompatibleBitmap(hdc, 1, 1);
    if (!hbm)
        return FALSE;
    
    // Initialize the header.
    ZeroMemory(pbi, sizeof(BITMAPINFOHEADER));
    pbi->biSize = sizeof(BITMAPINFOHEADER);

    // First call to GetDIBits will fill in the optimal biBitCount.
    bRet = GetDIBits(hdc, hbm, 0, 1, NULL, (BITMAPINFO*)pbi, DIB_RGB_COLORS);
    
    // Second call to GetDIBits will get the optimal color table, o
    // or the optimal bitfields values.
    if (bRet)
        bRet = GetDIBits(hdc, hbm, 0, 1, NULL, (BITMAPINFO*)pbi, DIB_RGB_COLORS);
    
    // Clean up.
    DeleteObject(hbm);

    return bRet;
}

// Counts the number of set bits in a DWORD.
static BYTE 
CountBits(DWORD dw)
{
    int iBits = 0;
    
    while (dw) {
        iBits += (dw & 1);
        dw >>= 1;
    }
    
    return iBits;
}

/**********************************************************

GetRGBBitsPerPixel

  Purpose:
   Retrieves the number of bits of color resolution for each 
   color channel of a specified.

  Input:
   hdc - Device to get the color information for.
   
  Output:
   pRed   - Number of distinct red levels the device can display.
   pGreen - Number of distinct green levels the device can display.
   pBlue  - Number of distinct blue levels the device can display.

  Notes:
   This function does not return any meaningful information for
   palette-based devices.

**********************************************************/ 
BOOL 
GetRGBBitsPerPixel(HDC hdc, PINT pRed, PINT pGreen, PINT pBlue)
{
    BITMAPINFOHEADER *pbi;
    LPDWORD lpdw;
    BOOL bRet = TRUE;

    // If the target device is palette-based, then bail because there is no
    // meaningful way to determine separate RGB bits per pixel.
    if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE) 
        return FALSE;  

    // Shortcut for handling 24bpp cases.
    if (GetDeviceCaps(hdc, PLANES) * GetDeviceCaps(hdc, BITSPIXEL) == 24) {
        *pRed = *pGreen = *pBlue = 8;
        return TRUE;
    }
   
    // Allocate room for a header and a color table.
    pbi = (BITMAPINFOHEADER *)GlobalAlloc(GPTR, sizeof(BITMAPINFOHEADER) + 
                                                sizeof(RGBQUAD)*256);
    if (!pbi)
        return FALSE;

    // Retrieve a description of the device surface.
    if (GetOptimalDIBFormat(hdc, pbi)) {
        // Get a pointer to the bitfields.
        lpdw = (LPDWORD)((LPBYTE)pbi + sizeof(BITMAPINFOHEADER));

        *pRed   = CountBits(lpdw[0]);
        *pGreen = CountBits(lpdw[1]);
        *pBlue  = CountBits(lpdw[2]);
    } else
        bRet = FALSE;
    
    // Clean up.
    GlobalFree(pbi);

    return bRet;
}
