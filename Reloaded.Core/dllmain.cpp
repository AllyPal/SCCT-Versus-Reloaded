// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <Windows.h>
#include "logger.h"
#include "Engine.h"
#include "Config.h"
#include "Input.h"
#include "GameConsole.h"
#include "Graphics.h"
#include "Networking.h"
#include "Debug.h"
#include "Fonts.h"

INIT_ONCE g_InitOnce = INIT_ONCE_STATIC_INIT;

std::wstring GetDllPath(HINSTANCE hModule) {
    std::vector<wchar_t> pathBuffer(MAX_PATH);
    const DWORD result = GetModuleFileName(hModule, pathBuffer.data(), pathBuffer.size());
    if (result == 0) {
        return L"";
    }

    pathBuffer.resize(result);
    return std::wstring(pathBuffer.begin(), pathBuffer.end());
}

static std::wstring GetExecutableDirectory(std::wstring executablePath) {
    std::wstring::size_type pos = std::wstring(executablePath).find_last_of(L"\\/");
    return std::wstring(executablePath).substr(0, pos);
}

void RedirectToConsole()
{
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONIN$", "r", stdin);
    freopen_s(&fp, "CONOUT$", "w", stderr);

    std::cout << "SCCT Versus Revived injected successfully" << std::endl;
}

const int BaseAddress = 0x10900000;
BOOL CALLBACK InitFunction(PINIT_ONCE InitOnce, PVOID Parameter, PVOID* Context) {
    HMODULE hModule = static_cast<HMODULE>(Parameter);
    auto dllPath = GetDllPath(hModule);
    auto directoryPath = GetExecutableDirectory(dllPath);

    RedirectToConsole();

    Logger::Initialize(dllPath);
    auto configPath = directoryPath + L"\\SCCT_Versus.config";
    Logger::log("");
    Logger::log(L"Loading " + configPath);
    Config::Initialize(configPath);
    Logger::log(L"animation_fix: " + std::to_wstring(Config::animation_fix));
    Logger::log(L"fps_host:" + std::to_wstring(Config::fps_host));
    Logger::log(L"fps_client:" + std::to_wstring(Config::fps_client));
    
    Networking::Initialize();
    Graphics::Initialize();
    GameConsole::Initialize();
    Input::Initialize();
    Engine::Initialize();
    Fonts::Initialize();

#ifdef _DEBUG
    Debug::Initialize();
#endif

    return TRUE;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            InitOnceExecuteOnce(&g_InitOnce, InitFunction, hModule, NULL);
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

