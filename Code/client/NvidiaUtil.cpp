#include "NvidiaUtil.h"
#include "immersive_launcher/launcher.h"
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
    // ENB (https://enbdev.org) works by injecting a proxy d3d11.dll into the SkyrimSE.exe
    // directory. It won't be loaded if SkyrimTogether.exe compile-time links to d3d11.dll, 
    // as it is in a different directory. To fix, launch ENB version if it is there, otherwise
    // launch via the standard search path.
    auto LC = launcher::GetLaunchContext();
    auto d3d11Path = LC->gamePath / "d3d11.dll";
    auto pModule = LoadLibraryExW(d3d11Path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);

    // If that failed, standard lookup.
    if (!pModule)
        pModule = LoadLibraryExW(d3d11Path.filename().c_str(), nullptr, 0);
    const auto pD3D11CreateDevice = reinterpret_cast<decltype(&D3D11CreateDevice)>(GetProcAddress(pModule, "D3D11CreateDevice"));

    return pD3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, appOutDevice, apOutFeatureLevel, nullptr);
}
