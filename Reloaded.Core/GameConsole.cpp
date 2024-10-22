#include "pch.h"
#include "GameConsole.h"
#include <format>
#include <set>
#include <iostream>
#include <thread>
#include <chrono>
#include <timeapi.h>
#include <Windows.h>
#include "StringOperations.h"
#include "MemoryWriter.h"
#include <functional>
#include <map>
#include <algorithm>
#include "GameStructs.h"
#include "Fonts.h"
#include "Debug.h"
#include "Engine.h"
#include <queue>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "winmm.lib")

void PrintConsoleHelp();
void PrintConsoleValues();

int getGraveAccentKeyCode() {
    HKL layout = GetKeyboardLayout(0);
    UINT vKey = MapVirtualKeyEx(consoleKeyBind, MAPVK_VSC_TO_VK, layout);
    return vKey & 0xFF;
}

static Console* console;

void OnConsoleCreated() {
    console->ConsoleKey() = getGraveAccentKeyCode();
}

static bool initialized = false;
void OnToggleConsole() {
    if (initialized) return;
    GUIFont* font = Fonts::GetFontByKeyName(L"GUIVerySmallDenFont");
    if (font->FirstFontArray() != nullptr) {
        console->MyFont() = *font->FirstFontArray();

        GameConsole::WriteGameConsole(L"======================");
        GameConsole::WriteGameConsole(console_version_message);
        GameConsole::WriteGameConsole(L"======================");
        GameConsole::WriteGameConsole(L" ");
        GameConsole::WriteGameConsole(L"Type 'help' to view the command list");

        initialized = true;
    }
}

static int ConsoleToggledEntry = 0x10C07768;
__declspec(naked) void ConsoleToggled() {
    static int Return = 0x1094A300;
    __asm {
        pushad
    }
    OnToggleConsole();
    __asm {
        popad
        jmp dword ptr[Return]
    }
}

static int ConsoleCreatedEntry = 0x109AB58B;
__declspec(naked) void ConsoleCreated() {
    __asm {
        pushad
        mov [console], esi
    }
    OnConsoleCreated();
    __asm {
        popad
        add     esp, 0x10
        ret
    }
}

static int thisConsole = 0;
int setThisConsoleEntry = 0x10B0F15E;
__declspec(naked) void setThisConsole() {
    static int Return = 0x10B0F165;
    __asm {
        lea     eax, dword ptr[esi + 0x28]
        mov[thisConsole], eax
        mov     dword ptr[esi + 0x28], 0x10BF280C
        jmp     dword ptr[Return]
    }
}

void GameConsole::WriteGameConsole(std::wstring message) {
    static int WriteConsoleFunc = 0x109117A0;
    if (!thisConsole) {
        return;
    }
    const wchar_t* messagePtr = message.c_str();
    __asm {
        pushad
        push messagePtr
        mov ebx, dword ptr[thisConsole]
        call dword ptr[WriteConsoleFunc]
        popad
    }
    return;
}

std::map<std::wstring, CommandHandler> getCommandHandlers() {
    std::map<std::wstring, CommandHandler> commandHandlers;

    commandHandlers[L"widescreen"] = {
            L"<true/false> - Apply widescreen aspect ratio.",
            [](const std::wstring& arg) {
            if (!arg.empty()) {
                std::wstring lArg = StringOperations::toLowercase(arg);
                if (lArg == L"true") {
                    Config::widescreen = true;
                }
                else if (lArg == L"false") {
                    Config::widescreen = false;
                }
                else {
                    throw std::runtime_error("Unexpected argument");
                }
                Config::Serialize();
            }
            GameConsole::WriteGameConsole(std::format(L" > widescreen {}", Config::widescreen ? L"true" : L"false"));
            },
            std::format(L" widescreen {}", Config::widescreen ? L"true" : L"false")
    };

    commandHandlers[L"ws_fov"] = {
        L"<number> - The maximum FOV allowed in first person Merc view.",
        [](const std::wstring& arg) {
        if (!arg.empty()) {
            auto frameLimit = std::stoi(arg);
            if (frameLimit < 95) {
                frameLimit = 95;
                GameConsole::WriteGameConsole(std::format(L" > minimum setting is {}", 95));
            }
            else if (frameLimit > 112) {
                frameLimit = 112;
                GameConsole::WriteGameConsole(std::format(L" > maximum setting is {}.", 112));
            }
            Config::ws_fov = frameLimit;
            Config::Serialize();
        }
        GameConsole::WriteGameConsole(std::format(L" > ws_fov {}", Config::ws_fov));
        },
        std::format(L" ws_fov {}", Config::ws_fov)
    };

    commandHandlers[L"sens"] = {
        L"<number> - Mouse sensitivity during gameplay.",
        [](const std::wstring& arg) {
        if (!arg.empty()) {
            Config::sens = std::stof(arg);
            Config::Serialize();
        }
        GameConsole::WriteGameConsole(std::format(L" > sens {:.3f}", Config::sens));
        },
        std::format(L" sens {:.3f}", Config::sens)
    };

    commandHandlers[L"sens_menu"] = {
        L"<number> - Mouse sensitivity in menus.",
        [](const std::wstring& arg) {
        if (!arg.empty()) {
            Config::sens_menu = std::stof(arg);
        Config::Serialize();
        }
        GameConsole::WriteGameConsole(std::format(L" > sens_menu {:.3f}", Config::sens_menu));
        },
        std::format(L" sens_menu {:.3f}", Config::sens_menu)
    };

    commandHandlers[L"sens_cam"] = {
        L"<number> - Mouse sensitivity for cam network and sticky cams.",
        [](const std::wstring& arg) {
        if (!arg.empty()) {
            Config::sens_cam = std::stof(arg);
        Config::Serialize();
        }
        GameConsole::WriteGameConsole(std::format(L" > sens_cam {:.3f}", Config::sens_cam));
        },
        std::format(L" sens_cam {:.3f}", Config::sens_cam)
    };

    commandHandlers[L"fps_client"] = {
        L"<number> - FPS whilst connected to servers.",
        [](const std::wstring& arg) {
        if (!arg.empty()) {
            auto frameLimit = std::stoi(arg);
#ifndef _DEBUG
            if (frameLimit < fps_client_min) {
                frameLimit = fps_client_min;
                GameConsole::WriteGameConsole(std::format(L" > minimum setting is {} FPS", fps_client_min));
            }
            else if (frameLimit > fps_client_max) {
                frameLimit = fps_client_max;
                GameConsole::WriteGameConsole(std::format(L" > maximum setting is {} FPS.", fps_client_max));
            }
#endif
            Config::fps_client = frameLimit;
            Config::Serialize();
        }
        GameConsole::WriteGameConsole(std::format(L" > fps_client {}", Config::fps_client));
        },
        std::format(L" fps_client {}", Config::fps_client)
    };

    commandHandlers[L"fps_host"] = {
        L"<number> - FPS whilst hosting.",
        [](const std::wstring& arg) {
        if (!arg.empty()) {
            auto frameLimit = std::stoi(arg);
#ifndef _DEBUG
            if (frameLimit < fps_host_min) {
                frameLimit = fps_host_min;
                GameConsole::WriteGameConsole(std::format(L" > minimum setting is {} FPS", fps_host_min));
            } else if (frameLimit > fps_host_max) {
                frameLimit = fps_host_max;
                GameConsole::WriteGameConsole(std::format(L" > maximum setting is currently {} FPS whilst hosting.", fps_host_max));
            }
#endif
            Config::fps_host = frameLimit;
            Config::Serialize();
        }
        GameConsole::WriteGameConsole(std::format(L" > fps_host {}", Config::fps_host));
        },
        std::format(L" fps_host {}", Config::fps_host)
    };

    commandHandlers[L"quit"] = {
        L"- Exits the game.",
        [](const std::wstring& arg) {
        exit(0);
        }
    };

    commandHandlers[L"help"] = {
        L"- Display command list.",
        [](const std::wstring& arg) {
            PrintConsoleHelp();
        }
    };

    commandHandlers[L"current"] = {
        L"- Display all current settings.",
        [](const std::wstring& arg) {
            PrintConsoleValues();
        }
    };

    Debug::CommandHandlers(&commandHandlers);

    return commandHandlers;
}

void PrintConsoleValues() {
    auto commandHandlers = getCommandHandlers();
    for (const auto& [key, handler] : commandHandlers) {
        if (!handler.displayValue.empty()) {
            GameConsole::WriteGameConsole(handler.displayValue);
        }
    }
}

void PrintConsoleHelp() {
    auto commandHandlers = getCommandHandlers();

    for (const auto& [key, handler] : commandHandlers) {
        GameConsole::WriteGameConsole(std::format(L" {} {}", key, handler.description));
    }
}

void ProcessConsole(uintptr_t inputPtr) {
    wchar_t* input = reinterpret_cast<wchar_t*>(inputPtr);
    std::wcout << L"Input: " << input << std::endl;

    std::wistringstream iss(input);
    std::wstring command, arg;
    iss >> command >> arg;
    std::transform(command.begin(), command.end(), command.begin(), ::towlower);
    auto commandHandlers = getCommandHandlers();
    if (commandHandlers.find(command) != commandHandlers.end()) {
        try {
            commandHandlers[command].handler(arg);
        }
        catch (...) {
            GameConsole::WriteGameConsole(L"Unexpected input format.");
        }
    }
}

int ConsoleInputEntry = 0x10B0F639;
__declspec(naked) void ConsoleInput() {
    static int Return = 0x10B0F63E;
    static uintptr_t inputPtr = 0;
    __asm {
        pushad
        mov[inputPtr], esi
    }
    ProcessConsole(inputPtr);
    __asm {
        popad
        push esi
        lea eax, [edi - 0x2C]
        push eax
        jmp dword ptr[Return]
    }
}

static std::queue<std::wstring> messageQueue;

void ClearMessageQueue()
{
    while (!messageQueue.empty()) {
        messageQueue.pop();
    }
}

void GameConsole::WriteChatBox(std::wstring displayText) {
    messageQueue.push(cb_prefix + displayText);
}

void OnCbCreated() {
    ClearMessageQueue();
    if (Engine::IsListenServer()) {
        GameConsole::WriteChatBox(L"Starting Reloaded v1.0 lobby");
    }
}

static int CbCreatedEntry = 0x10A66D88;
__declspec(naked) void CbCreated() {
    __asm {
        mov     dword ptr[eax], 0x10C02800
        pushad
    }
    OnCbCreated();
    __asm {
        popad
        ret
    }
}

static void OnCbUpdated(GUICb* cb) {
    if (messageQueue.empty() || cb->Input().text != nullptr) {
        return;
    }

    auto& displayText = messageQueue.front();
    cb->Input().text = new wchar_t[displayText.size() + 1];
    std::copy(displayText.begin(), displayText.end(), cb->Input().text);
    cb->Input().text[displayText.size()] = L'\0';
    cb->Input().length = static_cast<int>(displayText.size() + 1);
    cb->Input().alsoLength = static_cast<int>(displayText.size() + 1);
    messageQueue.pop();
}

static int CbUpdatedEntry = 0x10C02858;
__declspec(naked) void CbUpdated() {
    static GUICb* cb;
    __asm {
        mov dword ptr[cb], ecx
        pushad
    }
    OnCbUpdated(cb);
    static int Return = 0x10A61B20;
    __asm {
        popad
        jmp dword ptr[Return]
    }
}

void GameConsole::Initialize()
{
    MemoryWriter::WriteJump(CbCreatedEntry, CbCreated);
    MemoryWriter::WriteFunctionPtr(CbUpdatedEntry, CbUpdated);
    MemoryWriter::WriteJump(ConsoleInputEntry, ConsoleInput);
    MemoryWriter::WriteJump(setThisConsoleEntry, setThisConsole);
    MemoryWriter::WriteJump(ConsoleCreatedEntry, ConsoleCreated);
    MemoryWriter::WriteFunctionPtr(ConsoleToggledEntry, ConsoleToggled);
}
