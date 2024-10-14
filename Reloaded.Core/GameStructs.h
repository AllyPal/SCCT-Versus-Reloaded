#pragma once
#include <string>
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

struct UcString {
    wchar_t* text;
    int length;
    int alsoLength;
};

struct UcStringArray
{
    std::byte unspecified[0xc];
    uint32_t& sRes() {
        return *reinterpret_cast<uint32_t*>(unspecified + (0x0));
    }

    uint32_t& sResCount() {
        return *reinterpret_cast<uint32_t*>(unspecified + (0x4));
    }

    uint32_t& sResCount2() {
        return *reinterpret_cast<uint32_t*>(unspecified + (0x8));
    }
};

struct SLnSrvLnk {
    std::byte unspecified[0x1000];

    SOCKET Socket() {
        return *reinterpret_cast<SOCKET*>(unspecified + (0x2FC));
    }

    uint32_t& Port() {
        return *reinterpret_cast<uint32_t*>(unspecified + (0x300));
    }

    SOCKET RemoteSocket() {
        return *reinterpret_cast<SOCKET*>(unspecified + (0x304));
    }
};

struct SGaIn {
    std::byte unspecified[0x1000];

    SLnSrvLnk* IamLANServer() {
        return *reinterpret_cast<SLnSrvLnk**>(unspecified + (0x4A8));
    }

    wchar_t* InLAN() {
        return *reinterpret_cast<wchar_t**>(unspecified + (0x4B4));
    }
};

struct PlC {
    std::byte unspecified[0x1000];

    float& Defv() {
        return *reinterpret_cast<float*>(unspecified + (0x510));
    }

    float& Dfv() {
        return *reinterpret_cast<float*>(unspecified + (0xA44));
    }

    float& Sfv() {
        return *reinterpret_cast<float*>(unspecified + (0xA48));
    }

    float& Sens() {
        return *reinterpret_cast<float*>(unspecified + (0x5CC));
    }
    float& baz1() {
        return *reinterpret_cast<float*>(unspecified + (0xACC));
    }

    float& baz2() {
        return *reinterpret_cast<float*>(unspecified + (0xAD0));
    }

    float& baz3() {
        return *reinterpret_cast<float*>(unspecified + (0xAD4));
    }
};

enum NetMode {
    NotMultiplayer,
    DedicatedServer,
    ListenServer,
    Client
};

struct LvIn {
    std::byte unspecified[0x1000];

    NetMode& netMode() {
        return *reinterpret_cast<NetMode*>(unspecified + (0x4A8));
    }

    SGaIn* sGaIn() {
        return *reinterpret_cast<SGaIn**>(unspecified + (0x4DC));
    }

    PlC* lPlC() {
        return *reinterpret_cast<PlC**>(unspecified + (0x514));
    }
};

struct GUIFont {
    std::byte unspecified[0x1000];

    wchar_t* KeyName() {
        return *reinterpret_cast<wchar_t**>(unspecified + (0x2C));
    }

    uint32_t* FirstFontArray() {
        return *reinterpret_cast<uint32_t**>(unspecified + (0x48));
    }
};

struct Console {
    std::byte unspecified[0x1000];
    uint32_t& ConsoleKey() {
        return *reinterpret_cast<uint32_t*>(unspecified + (0x34));
    }

    uint32_t& MyFont() {
        return *reinterpret_cast<uint32_t*>(unspecified + (0x1E8));
    }
};

struct FragGrenade {
    std::byte unspecified[0x1000];

    float& Timer() {
        return *reinterpret_cast<float*>(unspecified + (0x1D0));
    }

    uint32_t& Flags() {
        return *reinterpret_cast<uint32_t*>(unspecified + (0x2F0));
    }
};

struct CompCol;
struct NoGuiComponent {
    std::byte unspecified[0x1000];

    uint32_t& flags() {
        return *reinterpret_cast<uint32_t*>(unspecified + (0x5C));
    }

    CompCol* Components() {
        return *reinterpret_cast<CompCol**>(unspecified + (0x190));
    }
};

struct CompCol {
    std::byte unspecified[0x1000];

    NoGuiComponent* GetControl(size_t index) {
        return *reinterpret_cast<NoGuiComponent**>(unspecified + (index - 1) * 4);
    }
};

struct GUIPageWaitLaunch {
    std::byte unspecified[0x1000];

    wchar_t* Title() {
        return *reinterpret_cast<wchar_t**>(unspecified + (0x280));
    }

    CompCol* Components() {
        return *reinterpret_cast<CompCol**>(unspecified + (0x190));
    }

    UcStringArray& sResArray() {
        return *reinterpret_cast<UcStringArray*>(unspecified + (0x2F0));
    }
};

struct SPlaPro {
    std::byte unspecified[0x1000];

    UcStringArray& sGameRes() {
        return *reinterpret_cast<UcStringArray*>(unspecified + (0xD8));
    }
};
