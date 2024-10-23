#include "pch.h"
#include "Graphics.h"
#include "include/d3d8/d3d8.h"
#include <format>
#include <set>
#include <iostream>
#include <thread>
#include <chrono>
#include <timeapi.h>
#include <Windows.h>
#include "StringOperations.h"
#include "MemoryWriter.h"
#include "Engine.h"
#include "Input.h"
#include "GameStructs.h"

static IDirect3D8* d3d;
static LPDIRECT3DDEVICE8 pDevice;
static D3DCAPS8* caps;
static int BackBufferWidth = 0;
static int BackBufferHeight = 0;
static int RenderWidth = 0;
static int RenderHeight = 0;

static std::vector<DisplayModePair> displayModePairs;
UcString* Graphics::videoSettingsDisplayModes;
UcString* Graphics::videoSettingsDisplayModesCmd;

std::wstring D3DErrorToString(HRESULT status) {
    switch (status) {
    case D3DERR_WRONGTEXTUREFORMAT: return L"D3DERR_WRONGTEXTUREFORMAT";
    case D3DERR_UNSUPPORTEDCOLOROPERATION: return L"D3DERR_UNSUPPORTEDCOLOROPERATION";
    case D3DERR_UNSUPPORTEDCOLORARG: return L"D3DERR_UNSUPPORTEDCOLORARG";
    case D3DERR_UNSUPPORTEDALPHAOPERATION: return L"D3DERR_UNSUPPORTEDALPHAOPERATION";
    case D3DERR_UNSUPPORTEDALPHAARG: return L"D3DERR_UNSUPPORTEDALPHAARG";
    case D3DERR_TOOMANYOPERATIONS: return L"D3DERR_TOOMANYOPERATIONS";
    case D3DERR_CONFLICTINGTEXTUREFILTER: return L"D3DERR_CONFLICTINGTEXTUREFILTER";
    case D3DERR_UNSUPPORTEDFACTORVALUE: return L"D3DERR_UNSUPPORTEDFACTORVALUE";
    case D3DERR_CONFLICTINGRENDERSTATE: return L"D3DERR_CONFLICTINGRENDERSTATE";
    case D3DERR_UNSUPPORTEDTEXTUREFILTER: return L"D3DERR_UNSUPPORTEDTEXTUREFILTER";
    case D3DERR_CONFLICTINGTEXTUREPALETTE: return L"D3DERR_CONFLICTINGTEXTUREPALETTE";
    case D3DERR_DRIVERINTERNALERROR: return L"D3DERR_DRIVERINTERNALERROR";
    case D3DERR_NOTFOUND: return L"D3DERR_NOTFOUND";
    case D3DERR_MOREDATA: return L"D3DERR_MOREDATA";
    case D3DERR_DEVICELOST: return L"D3DERR_DEVICELOST";
    case D3DERR_DEVICENOTRESET: return L"D3DERR_DEVICENOTRESET";
    case D3DERR_NOTAVAILABLE: return L"D3DERR_NOTAVAILABLE";
    case D3DERR_OUTOFVIDEOMEMORY: return L"D3DERR_OUTOFVIDEOMEMORY";
    case D3DERR_INVALIDDEVICE: return L"D3DERR_INVALIDDEVICE";
    case D3DERR_INVALIDCALL: return L"D3DERR_INVALIDCALL";
    case D3DERR_DRIVERINVALIDCALL: return L"D3DERR_DRIVERINVALIDCALL";
    default: return L"Unknown: " + StringOperations::toHexStringW(status);
    };
}

int D3DCreateResultEntry = 0x1095B986;
__declspec(naked) void D3DCreateResult() {
    static int Return = 0x1095B98C;
    __asm {
        mov[ebp + 0x46A0], eax
        mov[d3d], eax
        jmp dword ptr[Return]
    }
}

int GetMaxRefreshRate(UINT displayWidth, UINT displayHeight) {
    D3DDISPLAYMODE displayMode;
    UINT maxRefreshRate = 0;

    for (UINT i = 0; i < d3d->GetAdapterCount(); i++) {
        for (UINT j = 0; j < d3d->GetAdapterModeCount(i); j++) {
            if (SUCCEEDED(d3d->EnumAdapterModes(i, j, &displayMode))) {
                if (displayMode.Width == displayWidth && displayMode.Height == displayHeight) {
                    if (displayMode.RefreshRate > maxRefreshRate) {
                        maxRefreshRate = displayMode.RefreshRate;
                    }
                }
            }
        }
    }
    return maxRefreshRate;
}

void PrintParams(D3DPRESENT_PARAMETERS* d3dpp) {
    Logger::log(std::format("BackBufferWidth: {}",  d3dpp->BackBufferWidth));
    Logger::log(std::format("BackBufferHeight: {}",  d3dpp->BackBufferHeight));
    Logger::log(std::format("BackBufferFormat: {:#x}", static_cast<int>(d3dpp->BackBufferFormat)));
    Logger::log(std::format("BackBufferCount: {}",  d3dpp->BackBufferCount));
    Logger::log(std::format("MultiSampleType: {:#x}", static_cast<int>(d3dpp->MultiSampleType)));
    Logger::log(std::format("SwapEffect: {:#x}", static_cast<int>(d3dpp->SwapEffect)));
    Logger::log(std::format("Windowed: {}",  d3dpp->Windowed));
    Logger::log(std::format("EnableAutoDepthStencil: {}",  d3dpp->EnableAutoDepthStencil));
    Logger::log(std::format("AutoDepthStencilFormat: {:#x}", static_cast<int>(d3dpp->AutoDepthStencilFormat)));
    Logger::log(std::format("Flags: {:#x}",  d3dpp->Flags));
    Logger::log(std::format("FullScreen_RefreshRateInHz: {}",  d3dpp->FullScreen_RefreshRateInHz));
    Logger::log(std::format("FullScreen_PresentationInterval: {:#x}",  d3dpp->FullScreen_PresentationInterval));
}

std::string GetClosestAspectRatio(UINT width, UINT height) {
    const KnownAspectRatio aspectRatios[] = {
        {16.0, 9.0, "(16:9)"},
        {4.0, 3.0, "(4:3)"},
        {5.0, 4.0, "(5:4)"},
        {21.0, 9.0, "(21:9 stretched)"},
        {16.0, 10.0, "(16:10)"},
        {3.0, 2.0, "(3:2)"},
    };

    double actualRatio = static_cast<double>(width) / height;

    const KnownAspectRatio* closest = &aspectRatios[0];
    double minDifference = fabs(actualRatio - (closest->width / closest->height));

    for (const auto& ratio : aspectRatios) {
        double difference = fabs(actualRatio - (ratio.width / ratio.height));
        if (difference < minDifference) {
            minDifference = difference;
            closest = &ratio;
        }
    }

    return closest->name;
}

std::vector<DisplayModePair> SortDisplayModes(std::vector<DisplayModePair>& modes, UINT nativeWidth, UINT nativeHeight) {
    static int width = nativeWidth;
    static int height = nativeHeight;

    auto sortByHeightThenWidth = [](const DisplayModePair& a, const DisplayModePair& b) {
        
        if (a.width == width && a.height == height) {
            return true;
        }
        if (b.width == width && b.height == height) {
            return false;
        }
        if (a.width != b.width) {
            return a.width < b.width;
        }
        return a.height < b.height;
        };

    std::sort(modes.begin(), modes.end(), sortByHeightThenWidth);

    return modes;
}

std::vector<DisplayModePair> GetDisplayModesWithHighestRefreshRate() {
    std::vector<DisplayModePair> displayModes;
    std::map<std::string, ResolutionInfo> bestModes;

    UINT modeCount = d3d->GetAdapterModeCount(D3DADAPTER_DEFAULT);

    D3DDISPLAYMODE displayMode;
    for (UINT i = 0; i < modeCount; i++) {
        if (SUCCEEDED(d3d->EnumAdapterModes(D3DADAPTER_DEFAULT, i, &displayMode)) && displayMode.Width >= 640 && displayMode.Height >= 480) {
            std::string key = std::to_string(displayMode.Width) + "x" + std::to_string(displayMode.Height);

            if (bestModes.find(key) == bestModes.end() || displayMode.RefreshRate > bestModes[key].refreshRate) {
                bestModes[key] = { displayMode.Width, displayMode.Height, displayMode.RefreshRate };
            }
        }
    }

    auto maxWidth = std::max_element(bestModes.begin(), bestModes.end(),
        [](const auto& a, const auto& b) {
            return a.second.width < b.second.width;
        })->second.width;

    auto maxHeight = std::max_element(bestModes.begin(), bestModes.end(),
        [](const auto& a, const auto& b) {
            return a.second.height < b.second.height;
        })->second.height;

    auto nativeAspectRatio = GetClosestAspectRatio(maxWidth, maxHeight);

    for (const auto& entry : bestModes) {
        const ResolutionInfo& info = entry.second;

        auto aspectRatio = GetClosestAspectRatio(info.width, info.height);
        if (aspectRatio == nativeAspectRatio || aspectRatio == "(4:3)" || aspectRatio == "(16:9)" || info.height == maxHeight || info.width == maxWidth) {
            std::wstring modeWithFormat = std::to_wstring(info.width) + L"x" +
                std::to_wstring(info.height) + L"x32 F";
            displayModes.push_back({ modeWithFormat, info.width, info.height, aspectRatio });
        }
    }
    
    return SortDisplayModes(displayModes, maxWidth, maxHeight);
}

UcString* VectorToUcString(std::vector<UcString> vector) {
    auto outSize = vector.size();
    UcString* rawArray = new UcString[outSize];

    for (size_t i = 0; i < outSize; ++i) {
        rawArray[i] = vector[i];
    }

    return rawArray;
}

UcString* VideoSettingsDisplayOutput() {
    std::vector<UcString> vector;

    for (const auto& mode : displayModePairs) {
        std::wstring displayText = std::to_wstring(mode.width) + L" x " +
            std::to_wstring(mode.height) + L" " +
            StringOperations::stringToWString(mode.aspectRatio);

        UcString overrideEntry;

        overrideEntry.text = new wchar_t[displayText.size() + 1];

        std::copy(displayText.begin(), displayText.end(), overrideEntry.text);
        overrideEntry.text[displayText.size()] = L'\0';

        overrideEntry.length = static_cast<int>(displayText.size()+1);
        overrideEntry.alsoLength = static_cast<int>(displayText.size()+1);

        vector.push_back(overrideEntry);
    }

    return VectorToUcString(vector);
}

UcString* VideoSettingsDisplayCmd() {
    std::vector<UcString> vector;

    for (const auto& mode : displayModePairs) {
        std::wstring displayText = mode.modeWithFormat;

        UcString overrideEntry{};

        overrideEntry.text = new wchar_t[displayText.size() + 1];

        std::copy(displayText.begin(), displayText.end(), overrideEntry.text);
        overrideEntry.text[displayText.size()] = L'\0';

        overrideEntry.length = static_cast<int>(displayText.size() + 1);
        overrideEntry.alsoLength = static_cast<int>(displayText.size() + 1);

        vector.push_back(overrideEntry);
    }

    return VectorToUcString(vector);
}

static float actualAspectRatio = 1;
static float cappedAspectRatio = 1;
static D3DPRESENT_PARAMETERS d3dppReplacement;
static D3DPRESENT_PARAMETERS* overriddenD3dpp;
void ProcessD3DPresentParameters(D3DPRESENT_PARAMETERS* d3dpp) {
    RenderWidth = d3dpp->BackBufferWidth;
    RenderHeight = d3dpp->BackBufferHeight;

    float displayHeight = static_cast<float>(RenderHeight);
    float displayWidth = static_cast<float>(RenderWidth);
    actualAspectRatio = displayWidth / displayHeight;

    displayWidth = min(displayWidth, RenderHeight * (16.0f / 9.0));
    cappedAspectRatio = displayWidth / displayHeight;
    Input::aspectRatioMenuVertMouseInputMultiplier = actualAspectRatio / (4.0 / 3.0);

    ZeroMemory(&d3dppReplacement, sizeof(d3dppReplacement));
    d3dppReplacement = *d3dpp;
    if (Config::labs_borderless_fullscreen) {
        d3dppReplacement.Windowed = TRUE;
        d3dppReplacement.SwapEffect = D3DSWAPEFFECT_DISCARD;
        d3dppReplacement.BackBufferWidth = GetSystemMetrics(SM_CXSCREEN);
        d3dppReplacement.BackBufferHeight = GetSystemMetrics(SM_CYSCREEN);
        d3dppReplacement.FullScreen_RefreshRateInHz = 0;
        d3dppReplacement.FullScreen_PresentationInterval = 0;

        auto hWnd = d3dpp->hDeviceWindow;
        LONG style = GetWindowLong(hWnd, GWL_STYLE);
        style &= ~(WS_BORDER | WS_CAPTION | WS_THICKFRAME);
        SetWindowLong(hWnd, GWL_STYLE, style);

        SetWindowPos(hWnd, HWND_TOP, 0, 0, d3dppReplacement.BackBufferWidth,
            d3dppReplacement.BackBufferHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    if (Config::force_max_refresh_rate && !Config::labs_borderless_fullscreen) {
        Logger::log(std::format("Getting max refresh rate for {} {}", d3dpp->BackBufferWidth, d3dpp->BackBufferHeight));
        auto maxRefreshRate = GetMaxRefreshRate(d3dpp->BackBufferWidth, d3dpp->BackBufferHeight);
        Logger::log(std::format("Setting refresh rate to {}", maxRefreshRate));
        d3dppReplacement.FullScreen_RefreshRateInHz = maxRefreshRate;
    }

    displayModePairs = GetDisplayModesWithHighestRefreshRate();
    Graphics::videoSettingsDisplayModes = VideoSettingsDisplayOutput();
    Graphics::videoSettingsDisplayModesCmd = VideoSettingsDisplayCmd();

    //if (caps->MaxAnisotropy != 0) {
    //    UINT qualityLevels;

    //    auto vector = d3d->CheckDeviceMultiSampleType(D3DADAPTER_DEFAULT,
    //        D3DDEVTYPE_HAL,
    //        d3dppReplacement.BackBufferFormat, // Example format
    //        d3dppReplacement.Windowed,           // Windowed mode
    //        D3DMULTISAMPLE_16_SAMPLES);
    //    if (SUCCEEDED(vector)) {
    //        d3dppReplacement.MultiSampleType = D3DMULTISAMPLE_16_SAMPLES;
    //        
    //    }
    //}

    BackBufferWidth = d3dppReplacement.BackBufferWidth;
    BackBufferHeight = d3dppReplacement.BackBufferHeight;
    overriddenD3dpp = &d3dppReplacement;
}

HRESULT SetViewport(D3DVIEWPORT8* viewport) {
    if (Config::labs_borderless_fullscreen) {
        auto widthRatio = (float)BackBufferWidth / RenderWidth;
        auto heightRatio = (float)BackBufferHeight / RenderHeight;
        if (widthRatio > heightRatio) {
            viewport->Width = RenderWidth * heightRatio;
            viewport->Height = RenderHeight * heightRatio;
        }
        else {
            viewport->Width = RenderWidth * widthRatio;
            viewport->Height = RenderHeight * widthRatio;
        }

        viewport->X = (BackBufferWidth - viewport->Width) / 2;
        viewport->Y = (BackBufferHeight - viewport->Height) / 2;
    }
    auto result = pDevice->SetViewport(viewport);
    if (FAILED(result)) {
        std::cout << "Failed to set viewport" << std::endl;
    }
    return result;
}

int OverrideSetViewportEntry = 0x10961273;
__declspec(naked) void OverrideSetViewport() {
    static D3DVIEWPORT8* viewport;
    static int result;
    __asm {
        lea     edx, [esp + 0x10]
        mov dword ptr[viewport], edx
        pushad
    }
    result = SetViewport(viewport);
    static int Return = 0x1096127F;
    __asm {
        popad
        mov eax, dword ptr[result]
        jmp dword ptr[Return]
    }
}

int OverrideSetViewport2Entry = 0x1095E1AF;
__declspec(naked) void OverrideSetViewport3D() {
    static D3DVIEWPORT8* viewport;
    static int result;
    __asm {
        lea     eax, [esp + 0x24]
        mov dword ptr[viewport], eax
        pushad
    }
    result = SetViewport(viewport);
    static int Return = 0x1095E1BB;
    __asm {
        popad
        mov eax, dword ptr[result]
        jmp dword ptr[Return]
    }
}

//void SetWorldMatrix(D3DMATRIX* matrix) {
//    pDevice->SetTransform(D3DTS_WORLD, matrix);
//}
//
//int OverrideWorldMatrixEntry = 0x1096CA4A;
//__declspec(naked) void OverrideWorldMatrix() {
//    static D3DMATRIX* matrix;
//    __asm {
//        mov [matrix], ebp
//        pushad
//    }
//    SetWorldMatrix(matrix);
//    static int Return = 0x1096CA57;
//    __asm {
//        popad
//        jmp dword ptr[Return]
//   }
//}

void PrintD3DCAPS8(D3DCAPS8 caps) {
    std::wcout << std::fixed << std::hex << "DeviceCaps" << std::endl;
    std::wcout << std::fixed << std::hex << "DeviceType: " << caps.DeviceType << std::endl;
    std::wcout << std::fixed << std::hex << "AdapterOrdinal: " << caps.AdapterOrdinal << std::endl;
    std::wcout << std::fixed << std::hex << "Caps: " << caps.Caps << std::endl;
    std::wcout << std::fixed << std::hex << "Caps2: " << caps.Caps2 << std::endl;
    std::wcout << std::fixed << std::hex << "Caps3: " << caps.Caps3 << std::endl;
    std::wcout << std::fixed << std::hex << "PresentationIntervals: " << caps.PresentationIntervals << std::endl;
    std::wcout << std::fixed << std::hex << "CursorCaps: " << caps.CursorCaps << std::endl;
    std::wcout << std::fixed << std::hex << "DevCaps: " << caps.DevCaps << std::endl;
    std::wcout << std::fixed << std::hex << "PrimitiveMiscCaps: " << caps.PrimitiveMiscCaps << std::endl;
    std::wcout << std::fixed << std::hex << "RasterCaps: " << caps.RasterCaps << std::endl;
    std::wcout << std::fixed << std::hex << "ZCmpCaps: " << caps.ZCmpCaps << std::endl;
    std::wcout << std::fixed << std::hex << "SrcBlendCaps: " << caps.SrcBlendCaps << std::endl;
    std::wcout << std::fixed << std::hex << "DestBlendCaps: " << caps.DestBlendCaps << std::endl;
    std::wcout << std::fixed << std::hex << "AlphaCmpCaps: " << caps.AlphaCmpCaps << std::endl;
    std::wcout << std::fixed << std::hex << "ShadeCaps: " << caps.ShadeCaps << std::endl;
    std::wcout << std::fixed << std::hex << "TextureCaps: " << caps.TextureCaps << std::endl;
    std::wcout << std::fixed << std::hex << "TextureFilterCaps: " << caps.TextureFilterCaps << std::endl;          // D3DPTFILTERCAPS for IDirect3DTexture8's
    std::wcout << std::fixed << std::hex << "CubeTextureFilterCaps: " << caps.CubeTextureFilterCaps << std::endl;      // D3DPTFILTERCAPS for IDirect3DCubeTexture8's
    std::wcout << std::fixed << std::hex << "VolumeTextureFilterCaps: " << caps.VolumeTextureFilterCaps << std::endl;    // D3DPTFILTERCAPS for IDirect3DVolumeTexture8's
    std::wcout << std::fixed << std::hex << "TextureAddressCaps: " << caps.TextureAddressCaps << std::endl;         // D3DPTADDRESSCAPS for IDirect3DTexture8's
    std::wcout << std::fixed << std::hex << "VolumeTextureAddressCaps: " << caps.VolumeTextureAddressCaps << std::endl;   // D3DPTADDRESSCAPS for IDirect3DVolumeTexture8's
    std::wcout << std::fixed << std::hex << "LineCaps: " << caps.LineCaps << std::endl;                   // D3DLINECAPS
    std::wcout << std::fixed << std::hex << "MaxTextureWidth: " << caps.MaxTextureWidth << std::endl;
    std::wcout << std::fixed << std::hex << "MaxTextureHeight: " << caps.MaxTextureHeight << std::endl;
    std::wcout << std::fixed << std::hex << "MaxVolumeExtent: " << caps.MaxVolumeExtent << std::endl;
    std::wcout << std::fixed << std::hex << "MaxTextureRepeat: " << caps.MaxTextureRepeat << std::endl;
    std::wcout << std::fixed << std::hex << "MaxTextureAspectRatio: " << caps.MaxTextureAspectRatio << std::endl;
    std::wcout << std::fixed << std::hex << "MaxAnisotropy: " << caps.MaxAnisotropy << std::endl;
    std::wcout << std::fixed << std::hex << "MaxVertexW: " << caps.MaxVertexW << std::endl;
    std::wcout << std::fixed << std::hex << "GuardBandLeft: " << caps.GuardBandLeft << std::endl;
    std::wcout << std::fixed << std::hex << "GuardBandTop: " << caps.GuardBandTop << std::endl;
    std::wcout << std::fixed << std::hex << "GuardBandRight: " << caps.GuardBandRight << std::endl;
    std::wcout << std::fixed << std::hex << "GuardBandBottom: " << caps.GuardBandBottom << std::endl;
    std::wcout << std::fixed << std::hex << "ExtentsAdjust: " << caps.ExtentsAdjust << std::endl;
    std::wcout << std::fixed << std::hex << "StencilCaps: " << caps.StencilCaps << std::endl;
    std::wcout << std::fixed << std::hex << "FVFCaps: " << caps.FVFCaps << std::endl;
    std::wcout << std::fixed << std::hex << "TextureOpCaps: " << caps.TextureOpCaps << std::endl;
    std::wcout << std::fixed << std::hex << "MaxTextureBlendStages: " << caps.MaxTextureBlendStages << std::endl;
    std::wcout << std::fixed << std::hex << "MaxSimultaneousTextures: " << caps.MaxSimultaneousTextures << std::endl;
    std::wcout << std::fixed << std::hex << "VertexProcessingCaps: " << caps.VertexProcessingCaps << std::endl;
    std::wcout << std::fixed << std::hex << "MaxActiveLights: " << caps.MaxActiveLights << std::endl;
    std::wcout << std::fixed << std::hex << "MaxUserClipPlanes: " << caps.MaxUserClipPlanes << std::endl;
    std::wcout << std::fixed << std::hex << "MaxVertexBlendMatrices: " << caps.MaxVertexBlendMatrices << std::endl;
    std::wcout << std::fixed << std::hex << "MaxVertexBlendMatrixIndex: " << caps.MaxVertexBlendMatrixIndex << std::endl;
    std::wcout << std::fixed << std::hex << "MaxPointSize: " << caps.MaxPointSize << std::endl;
    std::wcout << std::fixed << std::hex << "MaxPrimitiveCount: " << caps.MaxPrimitiveCount << std::endl;          // max number of primitives per DrawPrimitive call
    std::wcout << std::fixed << std::hex << "MaxVertexIndex: " << caps.MaxVertexIndex << std::endl;
    std::wcout << std::fixed << std::hex << "MaxStreams: " << caps.MaxStreams << std::endl;
    std::wcout << std::fixed << std::hex << "MaxStreamStride: " << caps.MaxStreamStride << std::endl;            // max stride for SetStreamSource
    std::wcout << std::fixed << std::hex << "VertexShaderVersion: " << caps.VertexShaderVersion << std::endl;
    std::wcout << std::fixed << std::hex << "MaxVertexShaderConst: " << caps.MaxVertexShaderConst << std::endl;       // number of vertex shader constant registers
    std::wcout << std::fixed << std::hex << "PixelShaderVersion: " << caps.PixelShaderVersion << std::endl;

    if (caps.PixelShaderVersion >= D3DPS_VERSION(1, 0))
    {
        int majorVersion = D3DSHADER_VERSION_MAJOR(caps.PixelShaderVersion);
        int minorVersion = D3DSHADER_VERSION_MINOR(caps.PixelShaderVersion);
        std::wcout << std::fixed << std::hex << "PixelShaderVersion_Major: " << majorVersion << std::endl;
        std::wcout << std::fixed << std::hex << "PixelShaderVersion_Minor: " << minorVersion << std::endl;

    }

    std::wcout << std::fixed << std::hex << "MaxPixelShaderValue: " << caps.MaxPixelShaderValue << std::endl;
}

int D3D8CapsEntry = 0x1095BA7B;
__declspec(naked) void D3D8Caps() {
    static int Return = 0x1095BA93;
    __asm {
        lea edx, [ebp + 0x0000413C]
        mov [caps], edx
        push edx
        mov edx, [ebp + 0x00004678]
        mov edi, 0x00000001
        push edi
        push edx
        push eax
        call dword ptr[ecx + 0x34]
        pushad
    }
    //PrintD3DCAPS8(*caps);
    __asm {
        popad
        jmp dword ptr[Return]
    }
}

bool ResolutionChangeAllowed(int height, int width) {
    return true;
    // should no longer be needed now we're not throwing away the device constantly
    //auto result = (width != RenderWidth || height != RenderHeight) && height != 0 && width != 0;
    //return result;
}

int CanChangeResolutionEntry = 0x10B0F9A1;
__declspec(naked) void CanChangeResolution() {
    //static int SkipResolutionChange = 0x10B0F690;
    static int SkipResolutionChange = 0x10B0F7ED;
    static int ChangeResolution = 0x10B0F9B9;
    static int Height;
    static int Width;
    __asm {
        mov     eax, [esp + 0x40]
        mov     ebx, [esp + 0x18]
        mov dword ptr[Width], eax
        mov dword ptr[Height], ebx
        pushad
    }
    if (ResolutionChangeAllowed(Height, Width)) {
        __asm {
            popad
            jmp dword ptr[ChangeResolution]
        }
    }
    else {
        __asm {
            popad
            jmp dword ptr[SkipResolutionChange]
        }
    }
    
}

int StopDeviceReleaseEntry = 0x1095C9B1;
__declspec(naked) void StopDeviceRelease() {
    static int SkipRelease = 0x1095C9D4;
    __asm {
        xor ecx,ecx
        jmp dword ptr[SkipRelease]
    }
}

bool initialized = false;
void OnDeviceCreated() {
    if (!initialized) {
        Engine::EnableProcessSecurity();
        initialized = true;
    }
}

HRESULT CreateDevice(D3DPRESENT_PARAMETERS* pPresentationParameters, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, IDirect3DDevice8** ppReturnedDeviceInterface)
{
    Logger::log("d3d->CreateDevice:");
    PrintParams(pPresentationParameters);
    auto result = d3d->CreateDevice(Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
    pDevice = *ppReturnedDeviceInterface;
    return result;
}

HRESULT ResetDevice()
{
    HRESULT result = D3DERR_NOTFOUND;

    HRESULT testCooperativeLevel;
    int maxAttempts = 100;

    for (int i = 0; i < maxAttempts; ++i) {
        testCooperativeLevel = pDevice->TestCooperativeLevel();

        if (testCooperativeLevel != D3DERR_DEVICELOST) {
            Logger::log("Device not lost.  Attempting reset.");
            break;
        }

        Logger::log("Device is lost, retrying...");
        Sleep(50);
    }

    Logger::log(std::format(L"Cooperation level: {}", D3DErrorToString(testCooperativeLevel)));
    switch (testCooperativeLevel) {
    case D3D_OK:
    case D3DERR_DEVICENOTRESET:
    {
        Logger::log("pDevice->Reset:");
        PrintParams(&d3dppReplacement);
        result = pDevice->Reset(&d3dppReplacement);
        if (!SUCCEEDED(result)) {
            auto d3dError = D3DErrorToString(result);
            Logger::log(L"Reset result: " + d3dError);
        }
        else {
            Logger::log("Reset successful.");
        }
        break;
    }
    default:
        Logger::log("Cooperation level does not allow device to be reset.");
        break;
    }

    return  result;
}

HRESULT CreateDeviceOverride(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface) {
    if (pDevice == nullptr) {
        return CreateDevice(pPresentationParameters, Adapter, DeviceType, hFocusWindow, BehaviorFlags, ppReturnedDeviceInterface);
    }
    else {
        return ResetDevice();
    }
}

int CreateDeviceEntry = 0x1095CA73;
__declspec(naked) void CreateDevice() {
    static int Return = 0x1095CAB2;
    static int Fail = 0x1095D9B3;
    static D3DPRESENT_PARAMETERS* d3dpp;
    static UINT Adapter;
    static D3DDEVTYPE DeviceType;
    static HWND hFocusWindow;
    static DWORD BehaviorFlags;
    static D3DPRESENT_PARAMETERS* pPresentationParameters;
    static IDirect3DDevice8** ppReturnedDeviceInterface;
    static HRESULT result;
    __asm {
        lea edx, [eax + 0x000046A4]
        push edx
        mov [ppReturnedDeviceInterface], edx
        add     eax, 0x46A8
        mov dword ptr[d3dpp], eax
        pushad
    }
    ProcessD3DPresentParameters(d3dpp);
    __asm {
        popad
        mov eax, dword ptr[overriddenD3dpp]
        push eax
        mov[pPresentationParameters], eax

        mov eax, [ecx]
        push edi // 0x42
        mov[BehaviorFlags], edi

        call dword ptr[eax + 0x000000B4]
        mov ecx, [esp + 0x2C]
        mov edx, [esp + 0x18]
        push eax
        mov[hFocusWindow], eax
        mov eax, [edx + 0x00004678]
        push ecx
        mov[DeviceType], ecx
        push eax
        mov[Adapter], eax
        push esi

        //call dword ptr[ebx + 0x3C]
        add esp, 0x1c
        pushad
    }
    result = CreateDeviceOverride(Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
    __asm{
        popad
        mov eax, dword ptr[result]
        test eax, eax
        je success
        jmp dword ptr[Fail]
        success:

        mov ecx, [esp + 0x0C]

        mov eax, [pDevice]
        mov [ecx + 0x46A4], eax
        mov     eax, [ecx + 0x46A4]
        mov     edx, [eax]
        pushad
    }
    OnDeviceCreated();
    __asm {
        popad
        jmp     dword ptr[Return]
    }
}

void D3DMatrixIdentity(D3DMATRIX* pOut)
{
    // Zero out the matrix
    std::memset(pOut, 0, sizeof(D3DMATRIX));

    // Set the diagonal to 1
    pOut->m[0][0] = 1.0f;
    pOut->m[1][1] = 1.0f;
    pOut->m[2][2] = 1.0f;
    pOut->m[3][3] = 1.0f;
}

D3DMATRIX* D3DXMatrixPerspectiveFovLH(D3DMATRIX* pOut, float fovy, float aspect, float zn, float zf)
{
    D3DMatrixIdentity(pOut);

    float tanHalfFovy = tanf(fovy / 2.0f);
    pOut->m[0][0] = 1.0f / (aspect * tanHalfFovy);
    pOut->m[1][1] = 1.0f / tanHalfFovy;
    pOut->m[2][2] = zf / (zf - zn);
    pOut->m[2][3] = 1.0f;
    pOut->m[3][2] = (zf * zn) / (zn - zf);
    pOut->m[3][3] = 0.0f;

    return pOut;
}

float RadToDeg(float radians) {
    return radians * radToDegConversionFactor;
}

float DegreesToRadians(float degrees)
{
    return degrees * degToRadConversionFactor;
}

static float GetOriginalFovY(float m11) {
    return 2.0f * atanf(1.0f / m11);
}

static void ApplyMatrixPerspectiveFovLH(D3DMATRIX* projMatrix, float displayWidth, float displayHeight)
{
    float fovY = GetOriginalFovY(projMatrix->m[1][1]);// DegreesToRadians(57.0f);
    /*auto ogFov = RadToDeg(fovY);
    std::cout << std::format("ogfov:  {:.2f}", ogFov) << std::endl;*/
    float aspectRatio = displayWidth / displayHeight; // Example resolution
    float nearClip = 0.1f;// 1.0f;
    float farClip = 50000.0f;

    D3DXMatrixPerspectiveFovLH(projMatrix, fovY, aspectRatio, nearClip, farClip);
}

void ApplyWidthScaling(D3DMATRIX* projMatrix, float aspectRatio)
{
    if ((projMatrix->_11 < 0.0f) ^ (projMatrix->_22 < 0.0f)) {
        projMatrix->_22 = projMatrix->_11 * aspectRatio;
        projMatrix->_22 *= -1;
    }
    else {
        projMatrix->_22 = projMatrix->_11 * aspectRatio;
    }
}

void ApplyWidthScaling(D3DMATRIX* projMatrix, float displayHeight, float displayWidth)
{
    ApplyWidthScaling(projMatrix, displayWidth / displayHeight);
}

void ApplyHeightScaling(D3DMATRIX* projMatrix, D3DDISPLAYMODE& d3dDisplayMode)
{
    if ((projMatrix->_11 < 0.0f) ^ (projMatrix->_22 < 0.0f)) {
        projMatrix->_11 = (projMatrix->_22 / d3dDisplayMode.Width) * d3dDisplayMode.Height;
        projMatrix->_11 *= -1;
    }
    else {
        projMatrix->_11 = (projMatrix->_22 / d3dDisplayMode.Width) * d3dDisplayMode.Height;
    }
}

static bool renderingHudMenu = false;
int startRenderMenuEntry = 0x10A0FDC0;
__declspec(naked) void startHudMenuRender() {
    static int Return = 0x10A0FDC6;
    __asm {
        mov byte ptr[renderingHudMenu], 1
        mov     eax, [esi + 0x28]
        mov     ecx, [eax + 0x30]
        jmp     dword ptr[Return]
    }
}
int endRenderMenuEntry = 0x10A0FE4B;
__declspec(naked) void endHudMenuRender() {
    __asm {
        mov byte ptr[renderingHudMenu], 0
        retn    4
    }
}

static void SetupProjectionMatrix(D3DMATRIX* projMatrix)
{
    if (!Config::widescreen) {
        return;
    }
    D3DDISPLAYMODE d3dDisplayMode;
    pDevice->GetDisplayMode(&d3dDisplayMode);

    float displayHeight = static_cast<float>(RenderHeight);
    float displayWidth = min(RenderWidth, RenderHeight * (16.0f / 9.0));
    //float displayAspectRatio = displayHeight/ displayWidth;
    auto renderAspectRatio = fabs(roundf((projMatrix->_11 / projMatrix->_22) * 100.0f) / 100.0f);
    const float fourByThreeAspect = 0.75f;
    if (renderAspectRatio == fourByThreeAspect) {
        /*std::string message = std::format("deg:  {:.2f}", RadToDeg(originalFov);
        std::cout << message << std::endl;*/
        if (fabs(projMatrix->_22) < 0.1f) {
            if (renderingHudMenu) {
                return;
            }

            auto newAspect = (displayWidth / displayHeight);
            ApplyWidthScaling(projMatrix, newAspect);

            projMatrix->_42 = (projMatrix->_42 / (4.0 / 3)) * newAspect;
            return;
        }

        int scalingMode = 0;
        switch (scalingMode) {
        default:
            ApplyWidthScaling(projMatrix, displayHeight, displayWidth);
            break;
        case 1:
            ApplyMatrixPerspectiveFovLH(projMatrix, displayWidth, displayHeight);
            break;
        case 2:
            ApplyHeightScaling(projMatrix, d3dDisplayMode);
            break;
        }
    }
}
int SetProjection1Entry = 0x1096CA07;
__declspec(naked) void SetProjection1() {
    static D3DMATRIX* projMatrix;
    static int Return = 0x1096CA0D;
    __asm {
        pushad
        mov     dword ptr[projMatrix], ebp
    }
    SetupProjectionMatrix(projMatrix);

    __asm {
        popad
        call    dword ptr[ecx + 0x94]
        jmp     dword ptr[Return]
    }
}

int SetProjection2Entry = 0x1097827B;
__declspec(naked) void SetProjection2() {
    static D3DMATRIX* projMatrix;
    static int Return = 0x10978281;
    __asm {
        pushad
        mov     dword ptr[projMatrix], edx
    }
    SetupProjectionMatrix(projMatrix);

    __asm {
        popad
        call    dword ptr[ecx + 0x94]
        jmp     dword ptr[Return]
    }
}

int SetProjection3Entry = 0x1096BC07;
__declspec(naked) void SetProjection3() {
    static D3DMATRIX* projMatrix;
    static int Return = 0x1096BC0D;
    __asm {
        pushad
        mov eax, dword ptr[0x10CC9560]
        mov     dword ptr[projMatrix], eax
    }
    SetupProjectionMatrix(projMatrix);

    __asm {
        popad
        call    dword ptr[ecx + 0x94]
        jmp     dword ptr[Return]
    }
}

static int removeClientFpsCapEntry = 0x1090801C;
__declspec(naked) void removeClientFpsCap() {
    static int Return = 0x10908063;
    __asm {
        jmp dword ptr[Return]
    }
}

std::chrono::steady_clock::time_point nextFrameTime;
std::chrono::steady_clock::time_point Graphics::lastFrameTime;
void UpdateLastFrameRenderedTime() {
    double frameRateLimit;
    if (Engine::IsListenServer()) {
        frameRateLimit = (double)Config::fps_host;
    }
    else {
        frameRateLimit = (double)Config::fps_client;
    }
    if (Engine::gameState.inCoop) {
        frameRateLimit = fmin(90.0, frameRateLimit);
        debug_cout << "Restricted fps" << std::endl;
    }
    double frameTimeSeconds = (double)1.0 / frameRateLimit;
    auto frameTimeNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>((double)1.0 / frameRateLimit)
    ).count();

    Graphics::lastFrameTime = nextFrameTime;
    nextFrameTime = std::chrono::high_resolution_clock::now() + std::chrono::nanoseconds(frameTimeNanoseconds);
}

void preciseSpin() {
    while (std::chrono::high_resolution_clock::now() < nextFrameTime) {
    }
}

void preciseSleep() {
    auto remainingFrameTime = nextFrameTime - std::chrono::high_resolution_clock::now();
    auto remainingMS = std::chrono::duration<double, std::milli>(remainingFrameTime).count();
    if (remainingMS - std::floor(remainingMS) >= 0.5) {
        remainingMS = std::floor(remainingMS);
    }
    else {
        remainingMS = std::floor(remainingMS) - 1;
    }
    if (remainingMS > 0) {
        timeBeginPeriod(1);
        Sleep(static_cast<DWORD>(remainingMS));
        timeEndPeriod(1);
    }

    preciseSpin();
}

void LimitFrameRate() {
    if (Config::alternate_frame_timing_mode) {
        preciseSpin();
    }
    else {
        preciseSleep();
    }

    UpdateLastFrameRenderedTime();
}

double ConvertFOV(double horizontalFOV, double newAspectRatio) {
    double horizontalFOVRad = horizontalFOV * degToRadConversionFactor;
    double verticalFOVRad = 2 * atan(tan(horizontalFOVRad / 2) * originalAspectRatio);
    double newHorizontalFOVRad = 2 * atan(tan(verticalFOVRad / 2) * newAspectRatio);
    double newHorizontalFOV = newHorizontalFOVRad * radToDegConversionFactor;
    if (Engine::gameState.inCameraView) {
        return newHorizontalFOV;
    }
    return min(newHorizontalFOV, Config::ws_fov);
}

float ScaleFov(float fov) {
    if (Config::widescreen) {
        fov = ConvertFOV(fov, cappedAspectRatio);
    }

    return fov;
}

int WidescreenFirstPersonMeshFovFix2ntry = 0x10A0B126;
__declspec(naked) void WidescreenFirstPersonMeshFovFix() {
    static float fov;
    __asm {
        mov     ecx, [eax+0x308]
        mov dword ptr[fov], ecx
        pushad
    }
    fov = ScaleFov(fov);

    static int Return = 0x10A0B12C;
    __asm {
        popad
        mov ecx, dword ptr[fov]
        jmp dword ptr[Return]
    }
}

int WidescreenAlarmFovFixEntry = 0x10AFB10C;
__declspec(naked) void WidescreenAlarmFovFix() {
    static float fov;
    __asm {
        mov edx, [ebx + 0x308]
        mov dword ptr[fov], edx
        pushad
    }
    fov = ScaleFov(fov);

    static int Return = 0x10AFB112;
    __asm {
        popad
        mov edx, dword ptr[fov]
        jmp dword ptr[Return]
    }
}

int WidescreenErFovFixEntry = 0x10AF8E07;
__declspec(naked) void WidescreenErFovFix() {
    static float fov;
    __asm {
        mov     ecx, [esi + 0x308]
        mov dword ptr[fov], ecx
        pushad
    }
    fov = ScaleFov(fov);

    static int Return = 0x10AF8E0D;
    __asm {
        popad
        mov ecx, dword ptr[fov]
        jmp dword ptr[Return]
    }
}

int WidescreenFovFixEntry = 0x109EC46B;
__declspec(naked) void WidescreenFovFix() {
    static float fov;
    __asm {
        mov     edx, [ecx + 0x308]
        mov dword ptr[fov], edx
        pushad
    }
    fov = ScaleFov(fov);
    static int Return = 0x109EC471;
    __asm {
        popad
        mov edx, dword ptr[fov]
        jmp dword ptr[Return]
    }
}

void OnPresented() {
    if (Engine::gameState.lvIn != NULL && Engine::gameState.lvIn->lPlC() != NULL && pDevice != NULL) {
        Engine::OncePerFrame();
    }
}

int presentedEntry = 0x1095E417;
int presented2Entry = 0x1095E43C;
__declspec(naked) void presented() {
    __asm {
        pushad
    }
    OnPresented();
    __asm {
        popad
        add     esp, 0x14
        retn    0x4
    }
}

int alternativeFrameModeEntry = 0x109EC82F;
__declspec(naked) void alternativeFrameMode() {
    __asm {
        pushad
    }
    LimitFrameRate();
    __asm {
        popad
        retn    0x10
    }
}


int startFrameTimerEntry = 0x1095E331;
__declspec(naked) void beforePresent() {
    static int Return = 0x1095E38D;
    __asm {
        mov edx, [esp + 00]
        jmp dword ptr[Return]
    }

}

int animatedTextureFixEntry = 0x109F2561;
__declspec(naked) void animatedTextureFix() {
    static float oneHundred = 100.00f;
    static int ToMilliseconds = 0x10B83AA0;
    __asm {
        PUSH    ESI

        MOV     ESI, 0x10CCADA0 // not precise
        FLD     QWORD PTR[ESI]

        FMUL    dword ptr[oneHundred]
        CALL    dword ptr[ToMilliseconds]
        POP     ESI

        test    eax, eax
        jz skipCheck

        // abusing this offset.  It's not used unless a FPS cap is specified on the animated texture
        cmp     dword ptr[esi + 0x0000008C], eax
        jg      skipTextureRender

        skipCheck :

        add     eax, 3 // 3 100ths of a second
            mov     dword ptr[esi + 0x0000008C], eax

            mov     edx, [esi]
            mov     ecx, esi
            call    dword ptr[edx + 0x000000A4]

            skipTextureRender:
        pop     esi
            add     esp, 0x08
            ret     0004
    }
}

int Graphics::GetResolutionCount() {
    return displayModePairs.size();
}

static int AddGuiResolutionsEntry = 0x10B0FC5E;
__declspec(naked) void AddEnhancedGuiResolutions() {
    static int MaxResolutionIndex;
    static int Return = 0x10B0FC64;
    __asm {
        pushad
    }
    MaxResolutionIndex = Graphics::GetResolutionCount() - 1;
    __asm{
        popad
        push dword ptr[MaxResolutionIndex]
        push 0x10C42C74
        jmp dword ptr[Return]
    }
}

static int lodOverrideEntry = 0x109BF1BD;
__declspec(naked) void lodOverride() {
    static int Return = 0x109BF1C3;
    __asm {
        fmul dword ptr[Config::lod]
        jmp dword ptr[Return]
    }
}

void PrintUncooperative(HRESULT coopStatus) {
    Logger::log(L"Uncooperative: " + D3DErrorToString(coopStatus));
}

static int uncooperativeEntry = 0x1095E0B1;
__declspec(naked) void uncooperative() {
    static HRESULT coopStatus;
    __asm {
        pushad
        mov dword ptr[coopStatus], eax
    }
    PrintUncooperative(coopStatus);
    static int Return = 0x1095E0B6;
    __asm {
        popad
        cmp     eax, 0x88760868
        jmp dword ptr[Return]
    }
}

void Graphics::Initialize()
{
    if (Config::animation_fix)
        MemoryWriter::WriteJump(animatedTextureFixEntry, animatedTextureFix);

    MemoryWriter::WriteJump(uncooperativeEntry, uncooperative);

    MemoryWriter::WriteJump(startFrameTimerEntry, beforePresent);
    MemoryWriter::WriteJump(alternativeFrameModeEntry, alternativeFrameMode);
    MemoryWriter::WriteJump(removeClientFpsCapEntry, removeClientFpsCap);
    MemoryWriter::WriteJump(CreateDeviceEntry, CreateDevice);
    MemoryWriter::WriteJump(D3D8CapsEntry, D3D8Caps);
    MemoryWriter::WriteJump(D3DCreateResultEntry, D3DCreateResult);
    //MemoryWriter::WriteJump(OverrideSetViewportEntry, OverrideSetViewport);
    //MemoryWriter::WriteJump(OverrideSetViewport2Entry, OverrideSetViewport3D);
    //MemoryWriter::WriteJump(OverrideWorldMatrixEntry, OverrideWorldMatrix);
    MemoryWriter::WriteJump(CanChangeResolutionEntry, CanChangeResolution);

    MemoryWriter::WriteJump(StopDeviceReleaseEntry, StopDeviceRelease);


    MemoryWriter::WriteJump(SetProjection1Entry, SetProjection1);
    MemoryWriter::WriteJump(SetProjection2Entry, SetProjection2);
    MemoryWriter::WriteJump(SetProjection3Entry, SetProjection3);
    MemoryWriter::WriteJump(WidescreenFovFixEntry, WidescreenFovFix);
    MemoryWriter::WriteJump(WidescreenFirstPersonMeshFovFix2ntry, WidescreenFirstPersonMeshFovFix);
    MemoryWriter::WriteJump(WidescreenErFovFixEntry, WidescreenErFovFix);
    MemoryWriter::WriteJump(WidescreenAlarmFovFixEntry, WidescreenAlarmFovFix);

    MemoryWriter::WriteJump(startRenderMenuEntry, startHudMenuRender);
    MemoryWriter::WriteJump(endRenderMenuEntry, endHudMenuRender);

    MemoryWriter::WriteJump(presentedEntry, presented);
    MemoryWriter::WriteJump(presented2Entry, presented);

    //MemoryWriter::WriteJump(D3DPPEntry, D3DPP);
    MemoryWriter::WriteJump(AddGuiResolutionsEntry, AddEnhancedGuiResolutions);

    if (Config::flashlight_rendering_fix && Config::flashlight_compatible_d3d8) {
        // IDirect3D8_CheckDeviceFormat D3DFMT_D24X8 & D3DFMT_D16
        uint8_t d3dTypeRef[] = { D3DDEVTYPE_REF };
        MemoryWriter::WriteBytes(0x1095BEC8, d3dTypeRef, sizeof(d3dTypeRef));
        MemoryWriter::WriteBytes(0x1095BF40, d3dTypeRef, sizeof(d3dTypeRef));
    }

    MemoryWriter::WriteJump(lodOverrideEntry, lodOverride);
}