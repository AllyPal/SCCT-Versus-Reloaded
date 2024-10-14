#include <iostream>
#include <windows.h>
#include <cstdio>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <chrono>
#include <filesystem>
#include "Inject.h"

static std::wstring GetExecutablePath() {
    wchar_t buffer[MAX_PATH];
    GetModuleFileName(nullptr, buffer, MAX_PATH);
    return std::wstring(buffer);
}

static std::wstring GetExecutableDirectory() {
    std::wstring executablePath = GetExecutablePath();
    std::wstring::size_type pos = std::wstring(executablePath).find_last_of(L"\\/");
    return std::wstring(executablePath).substr(0, pos);
}

static std::wstring FindExecutable(const std::wstring& exeName) {
    std::wstring currentPath = GetExecutableDirectory() + L"\\" + exeName;
    if (GetFileAttributes(currentPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return currentPath;
    }

    return L"";
}

static std::wstring FindScctVersusExecutable() {
    // Enhanced file name
    std::wstring enhancedExecutableName = L"scct versus";
    auto result = FindExecutable(enhancedExecutableName);
    if (!result.empty()) {
        return result;
    }
    
    result = FindExecutable(L"system\\" + enhancedExecutableName);
    if (!result.empty()) {
        return result;
    }

    // Default file name
    std::wstring execuatableName = L"scct_versus.ex";
    result = FindExecutable(execuatableName);
    if (!result.empty()) {
        return result;
    }

    return FindExecutable(L"system\\" + execuatableName);
}

std::wstring getCurrentDateTime() {
    std::time_t now = std::time(nullptr);

    std::tm localTime;

    localtime_s(&localTime, &now);

    std::wstringstream wss;
    wss << std::put_time(&localTime, L"%Y-%m-%d %H:%M:%S");

    return wss.str();
}

LPWSTR ConvertToWideString(LPSTR lpStr)
{
    int nChars = MultiByteToWideChar(CP_ACP, 0, lpStr, -1, NULL, 0);
    if (nChars == 0)
    {
        return NULL;
    }

    LPWSTR lpWideStr = new WCHAR[nChars];
    MultiByteToWideChar(CP_ACP, 0, lpStr, -1, lpWideStr, nChars);
    return lpWideStr;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    std::wofstream file(GetExecutablePath() + L".log");
    if (!file.is_open()) {
        std::wcerr << L"Failed to open log file." << std::endl;
        return 1;
    }
    // Redirect std::wcout and std::wcerr to the file stream
    std::wcout.rdbuf(file.rdbuf());
    std::wcerr.rdbuf(file.rdbuf());

    std::wcout << L"Starting at " << getCurrentDateTime() << std::endl;

    auto exePath = FindScctVersusExecutable();
    if (exePath.empty()) {
        // TODO: Check where it goes for full copies of the game
        std::wcerr << L"SCCT_Versus.ex not found. Make sure the application is placed in the root or /System folder of your game" << std::endl;
        std::cin.get();
        return 1;
    }
    
    std::wcout << L"Found executable at " << exePath << std::endl << std::endl;

    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    if (!CreateProcess(exePath.c_str(), ConvertToWideString(lpCmdLine), NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        std::wcerr << L"Failed to start process." << std::endl;
        std::cin.get();
        return 1;
    }

    std::wcout << L"Suspended process created successfully." << std::endl;

    std::wstring dllPath = GetExecutableDirectory() + L"\\Reloaded.Core.dll";
    if (!Inject::InjectDLL(pi.hProcess, dllPath)) {
        std::wcerr << L"Failed to inject DLL. Terminating SCCT_Versus.exe" << std::endl;
        TerminateProcess(pi.hProcess, 1);
        std::cin.get();
        return 1;
    }
    std::wcout << L"Code injected successfully." << std::endl;
    ResumeThread(pi.hThread);
    std::wcout << L"Process resumed." << std::endl;

    /*std::this_thread::sleep_for(std::chrono::seconds(1));
    TerminateProcess(pi.hProcess, 1);*/

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    file.close();

    return 0;
}

