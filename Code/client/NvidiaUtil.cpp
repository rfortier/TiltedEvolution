#include "NvidiaUtil.h"
#include <d3d11.h>

bool IsNvidiaOverlayLoaded()
{
    return GetModuleHandleW(L"nvspcap64.dll");
}

// This makes the Nvidia overlay happy.
// The call to D3D11CreateDevice probably causes some of their
// internal hooks to be called and do the required init work before the game window opens.
HRESULT CreateEarlyDxDevice(ID3D11Device** appOutDevice, D3D_FEATURE_LEVEL* apOutFeatureLevel)
{
    // Turn this off for now because it breaks ENBs. Just disable nvidia overlay.
    *apOutFeatureLevel = D3D_FEATURE_LEVEL_11_0;
    return S_OK;
    //return D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, appOutDevice,
    //                         apOutFeatureLevel, nullptr);
}
