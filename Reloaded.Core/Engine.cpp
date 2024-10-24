#include "pch.h"
#include "Engine.h"
#include <format>
#include <set>
#include <iostream>
#include <thread>
#include <chrono>
#include <timeapi.h>
#include <Windows.h>
#include "StringOperations.h"
#include "MemoryWriter.h"
#include "Input.h"
#include "GameStructs.h"
#include "Graphics.h"
#include "Debug.h"

State Engine::gameState = {};

int SetLvInEntry = 0x109ADFB3;
__declspec(naked) void SetLvIn() {
    __asm {
        mov dword ptr[Engine::gameState.lvIn], esi
        ret
    }
}

wchar_t* gameVersionIdentifier = nullptr;
__declspec(naked) void SetGameVersionIdentifier() {
    __asm {
        mov dword ptr[gameVersionIdentifier], 0x10C42D94
        ret
    }
}

bool Engine::IsListenServer() {
    if (Engine::gameState.lvIn == nullptr) {
        return false;
    }
    return Engine::gameState.lvIn->netMode() == NetMode::ListenServer;
}

static bool IsScct1_2() {
    static bool cachedResult = (gameVersionIdentifier != nullptr && wcscmp(gameVersionIdentifier, L"SC3_PC_518_5") == 0);
    return cachedResult;
}

static bool IsScctEnhanced3_4_Plus() {
    static int ScctEnhancedIdentifier = 0x10C42DA4;
    static char identifier;
    __asm {
        mov eax, dword ptr[ScctEnhancedIdentifier]
        mov al, byte ptr[eax]
        mov byte ptr[identifier], al
    }
    return identifier == '3';
}

static int ScctVersion() {
    if (IsScctEnhanced3_4_Plus()) {
        return enhanced_scct_version;
    }
    return default_scct_version;
}

int InstaFixEntry = 0x10AB8DAA;
__declspec(naked) void InstaFix() {
    static int InstaFixResume = 0x10AB8DE9;
    __asm {
        jmp dword ptr[InstaFixResume];
    }
}

typedef LONG NTSTATUS;
typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(OSVERSIONINFOEXW*);

bool IsWindows10OrGreater() {
    HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr pRtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
        if (pRtlGetVersion) {
            OSVERSIONINFOEXW osInfo = { 0 };
            osInfo.dwOSVersionInfoSize = sizeof(osInfo);
            if (pRtlGetVersion(&osInfo) == 0) {
                std::cout << "win: " << osInfo.dwMajorVersion << std::endl;
                return osInfo.dwMajorVersion >= 10;
            }
        }
    }

    return false;
}

// Enables features which reduce the risks of potential buffer overflow vulernabilities in the base game
void Engine::EnableProcessSecurity()
{
    if (Config::security_acg && IsWindows10OrGreater()) {
        PROCESS_MITIGATION_DYNAMIC_CODE_POLICY policy = {};
        policy.ProhibitDynamicCode = 1;
        if (SetProcessMitigationPolicy(ProcessDynamicCodePolicy, &policy, sizeof(policy))) {
            std::cout << "ACG enabled" << std::endl;
        }
    }

    if (Config::security_dep && SetProcessDEPPolicy(PROCESS_DEP_ENABLE)) {
        std::cout << "DEP enabled" << std::endl;
    }
}

bool binocularInitialized = false;
const long binocularUpdateInterval = 33333333;
float baz1Initial;
float baz2Initial;
float baz3Initial;


std::chrono::steady_clock::time_point nextBinocularUpdate;
void binocularFix() {
    if (!binocularInitialized) {
        if (Graphics::lastFrameTime == std::chrono::steady_clock::time_point{}) {
            // last frame not set yet
            return;
        }
        baz1Initial = Engine::gameState.lvIn->lPlC()->baz1();
        baz2Initial = Engine::gameState.lvIn->lPlC()->baz2();
        baz3Initial = Engine::gameState.lvIn->lPlC()->baz3();
        nextBinocularUpdate = Graphics::lastFrameTime;
        binocularInitialized = true;
    }
    if (nextBinocularUpdate <= Graphics::lastFrameTime) {
        Engine::gameState.lvIn->lPlC()->baz1() = baz1Initial;
        Engine::gameState.lvIn->lPlC()->baz2() = baz2Initial;
        Engine::gameState.lvIn->lPlC()->baz3() = baz3Initial;
        auto remainder = std::chrono::duration_cast<std::chrono::nanoseconds>(Graphics::lastFrameTime - nextBinocularUpdate).count();
        nextBinocularUpdate = Graphics::lastFrameTime + std::chrono::nanoseconds(binocularUpdateInterval - remainder);

    }
    else {
        Engine::gameState.lvIn->lPlC()->baz1() = 0.0f;
        Engine::gameState.lvIn->lPlC()->baz2() = 0.0f;
        Engine::gameState.lvIn->lPlC()->baz3() = 0.0f;
    }
}

static uint32_t sResOriginal;
static uint32_t sResCountOriginal;
static uint32_t sResCount2Original;

void onPageUpdate(GUIPage* page)
{
    static GUIPage* lastPage;
    if (page != lastPage && page->Title() != nullptr) {
        auto pageName = page->Parent()->Name();
        if (wcscmp(pageName, L"Controller_Settings") == 0) {
            page->Components()->GetControl(3)->Components()->GetControl(2)->flags() &= ~8;
        }
        else if (wcscmp(pageName, L"Video_Settings") == 0) {
            // Restore original pointer when we're done so it's freed
            page->sResArray().sRes() = sResOriginal;
            page->sResArray().sResCount() = sResCountOriginal;
            page->sResArray().sResCount2() = sResCount2Original;
        }
        else if (wcscmp(pageName, L"Menu_Multi") == 0) {
            page->Components()->GetControl(2)->flags() = 0;
        }
    }
    lastPage = page;
}

int GuiPageWaitUpdateEntry = 0x10C02C58;
int GuiPageWaitUpdateEntry2 = 0x10C02D08;
__declspec(naked) void GuiPageWaitUpdate() {
    static GUIPage* page;
    __asm {
        pushad
        mov [page], ecx
    }
    onPageUpdate(page);
    static int Return = 0x10A5A4C0;
    __asm {
        popad
        jmp dword ptr[Return]
    }
}

void HideConsoleWindow() {
#ifndef _DEBUG
    HWND consoleWindow = GetConsoleWindow();
    FreeConsole();
    if (consoleWindow != NULL) {
        PostMessage(consoleWindow, WM_CLOSE, 0, 0);
    }
#endif
}

void OnSPlaProCreated(SPlaPro* plaPro) {
    HideConsoleWindow();
    plaPro->sGameRes().sRes() = reinterpret_cast<uintptr_t>(Graphics::videoSettingsDisplayModesCmd);
    plaPro->sGameRes().sResCount() = Graphics::GetResolutionCount();
    plaPro->sGameRes().sResCount() = Graphics::GetResolutionCount();
}

int SPlaProCreatedEntry = 0x10AEF546;
__declspec(naked) void SPlaProCreated() {
    static SPlaPro* plaPro;
    __asm {
        je skip
        mov dword ptr[eax], 0x10BFC340
        mov dword ptr[plaPro], eax
        pushad
    }
    OnSPlaProCreated(plaPro);
    __asm {
        popad
        skip:
        ret
    }
}

void onPageLoad(GUIPage* page)
{
    static GUIPage* lastPage;
    if (page != lastPage && page->Title() != nullptr) {
        if (wcscmp(page->Parent()->Name(), L"Video_Settings") == 0) {
            sResOriginal = page->sResArray().sRes();
            sResCountOriginal = page->sResArray().sResCount();
            sResCount2Original = page->sResArray().sResCount2();

            page->sResArray().sRes() = reinterpret_cast<uintptr_t>(Graphics::videoSettingsDisplayModes);
            page->sResArray().sResCount() = Graphics::GetResolutionCount();
            page->sResArray().sResCount2() = Graphics::GetResolutionCount();
        }
    }
    lastPage = page;
}

int GuiPageWaitLoadEntry = 0x10A66746;
__declspec(naked) void GuiPageWaitLoad() {
    static GUIPage* page;
    __asm {
        je skip
        mov dword ptr[eax], 0x10C02C40
        mov[page], eax
        pushad
    }
    onPageLoad(page);
    __asm {
        popad
        skip:
        ret
    }
}

void forceSensInternal() {
    //Engine::gameState.lvIn->lPlC()->Sens() = 4.0f;
}

void Engine::OncePerFrame() {
    binocularFix();
    forceSensInternal();
}

bool ValidateState(int newState, PlC* plc) {
    if (plc != Engine::gameState.lvIn->lPlC()) {
        return true;
    }
    bool result = true;
    __asm {
        mov eax, newState
        mov ecx, 0xF4522FF2
        xor ecx, -1
        mov ebx, 0
        mov eax, 0x1B3D94FC
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B39F643
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B39F653
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B39FCA0
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B39E4EB
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B39982A
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B398314
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B0D7F82
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B0C6D75
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B0F27D3
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B0429E8
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B042A7C
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B07D0E5
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B07D119
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B07D444
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B07C282
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B064916
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B061F52
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B060F84
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B002FE5
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B02B02B
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B1FA6AA
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B1FAF1E
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B1F1F73
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B1ED0C1
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B1EC606
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B1EC74B
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B179997
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B103B89
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B39E89B
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B3999E2
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B3984E4
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B3A45FB
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B3A4603
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B0C9F1B
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B07D0FF
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B07D520
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        mov eax, 0x1B07CE18
        xor eax, ecx
        mov eax, [eax]
        add ebx, eax
        ror ebx, 1
        cmp ebx, 0x167c46f3
        je skip
        mov byte ptr[result], 0
        skip:
    }
    return result;
}

wchar_t* StateName(int id) {
    auto basePtr = *reinterpret_cast<int**>(0x10CC9E5C);
    auto elementPtr = reinterpret_cast<int*>(basePtr[id]);
    auto stringPtr = reinterpret_cast<wchar_t*>(reinterpret_cast<int>(elementPtr) + 0xC);

    return stringPtr;
}

void OnNewState(int newState, PlC* plc) {
    if (plc != Engine::gameState.lvIn->lPlC()) {
        return;
    }
    Engine::gameState.inCameraView = false;
    Engine::gameState.inCoop = false;
    
    switch (newState) {
    case 2031:
    case 402:
        Engine::gameState.inCameraView = true;
        break;
    default:
        if (wcscmp(StateName(newState), L"s_ShortLadder_S_Jump") == 0) {
            Engine::gameState.inCoop = true;
        }
        else if (wcscmp(StateName(newState), L"s_PlayerJump") == 0) {
            Engine::gameState.lastJumpTime = Graphics::lastFrameTime;
        }
        break;
    }
}

int OnStateChangeEntry = 0x109F1FC3;
__declspec(naked) void OnStateChange() {
    static int Deny = 0x109F1FD0;
    static int Return = 0x109F1FCB;
    static int currentState;
    static int newState;
    static int all;
    static PlC* plc;
    __asm {
        pushad
        mov dword ptr[currentState], esi
        mov dword ptr[newState], eax
        mov dword ptr[plc], edi
    }
    OnNewState(newState, plc);
    if (!ValidateState(newState, plc)) {
        __asm {
            mov     eax, 0x193
            mov[esp + 0x2c], eax
            popad
            mov     edx, [edi]
            mov     eax, 0x193
            push    eax
            mov     ecx, edi
            call    dword ptr[edx + 0x30]
            jmp     dword ptr[Return]
        }
    }
    else {
        __asm {
            popad
            mov     edx, [edi]
            push    eax
            mov     ecx, edi
            call    dword ptr[edx + 0x30]
            jmp     dword ptr[Return]
        }
    }
}

static int StickyCamContextMenuBlockEntry = 0x10B2D1C0;
__declspec(naked) void StickyCamContextMenuBlock() {
    __asm {
        mov eax, ecx
        mov eax, [eax + 0xB4]
        mov eax, [eax + 0x774]
        mov eax, [eax + 0xA90]
        mov [eax+0x37C], 0
        ret
    }
}


static std::map<std::pair<std::wstring, std::wstring>, std::wstring> overrideMap;
void Engine::SetLabelOverride(const wchar_t* element, const wchar_t* page, std::wstring message) {
    overrideMap[{element, page}] = message;
}

static void InitLabelOverrides() {
    Engine::SetLabelOverride(L"TitlePage", L"LAN_Seek_Games_List", L"Reloaded Server List");
    Engine::SetLabelOverride(L"TitlePage", L"LAN_Menu", L"Reloaded Session");
    Engine::SetLabelOverride(L"TitrePage.Caption", L"LAN_Menu", L"Reloaded Session");
    Engine::SetLabelOverride(L"TitlePage", L"Lobby_Create", L"Reloaded Lobby");
    Engine::SetLabelOverride(L"MainPage_LAN.Caption", L"Menu_Multi", L"Play Multiplayer");

    HKL layout = GetKeyboardLayout(0);
    UINT virtualKey = MapVirtualKeyEx(consoleKeyBind, MAPVK_VSC_TO_VK, layout);

    BYTE keyboardState[256] = { 0 };
    WCHAR buffer[3] = { 0 };
    int chars = ToUnicodeEx(virtualKey, consoleKeyBind, keyboardState, buffer, 2, 0, layout);

    if (chars > 0) {
        std::wstring message = L"Mouse Sensitivity - set with sens <value> in console (bind " + std::wstring(1, buffer[0]) + L")";
        Engine::SetLabelOverride(L"MouseSensitive.Caption", L"Controller_Settings", message);
    }
    else {
        Engine::SetLabelOverride(L"MouseSensitive.Caption", L"Controller_Settings", L"Mouse Sensitivity - set with sens <value> in console");
    }

    if (IsScct1_2()) {
        Engine::SetLabelOverride(L"NoGoodVersion", L"LAN_Seek_Games_List", L"Cannot join: Requires Enhanced SCCT Versus");
    }
    else if (IsScctEnhanced3_4_Plus()) {
        Engine::SetLabelOverride(L"NoGoodVersion", L"LAN_Seek_Games_List", L"Cannot join: Requires Stock SCCT Versus");
    }
    else {
        Engine::SetLabelOverride(L"NoGoodVersion", L"LAN_Seek_Games_List", L"Cannot join: Using Newer Versus Enhanced");
    }
}

void PrintTextEntry(wchar_t* languageName, wchar_t* controlName, wchar_t* menuName, wchar_t* current) {
    
    const wchar_t* setting = current;
    if (setting == nullptr) {
        setting = L" ";
    }
    
    std::wcout << std::format(L"Engine::SetLabelOverride(L\"{}\", L\"{}\", L\"{}\");", controlName, menuName, setting) << std::endl;
}

wchar_t* OverrideLabel(wchar_t* languageName, wchar_t* controlName, wchar_t* menuName, wchar_t* current) {
#ifdef _DEBUG
    //PrintTextEntry(languageName, controlName, menuName, current);
#endif
    std::wstring controlKey(controlName);
    std::wstring menuKey(menuName);

    std::pair<std::wstring, std::wstring> key = { controlKey, menuKey };

    if (overrideMap.find(key) != overrideMap.end()) {
        return const_cast<wchar_t*>(overrideMap[key].c_str());
    }

    return current;
}

static int LoadTextEntry = 0x10912900;
__declspec(naked) void LoadText() {
    static int Return = 0x1091290C;
    static wchar_t* _eax;
    static wchar_t* _edi;
    static wchar_t* _ebp;
    static wchar_t* current;
    static wchar_t* textOverride;
    __asm {
        lea     eax, [esp + 0x8]
        push eax
        mov[_eax], eax
        push edi
        mov[_edi], edi
        push 0
        push ebp
        mov[_ebp], ebp
        call    dword ptr[edx + 0xC]
        mov[current],eax
        pushad
    } 
    textOverride = OverrideLabel(_eax, _edi, _ebp, current);
    __asm{
        popad
        mov eax,[textOverride]
        jmp dword ptr[Return]
    }
}

static void FragCreated(FragGrenade* frag) {
    if (frag->Timer() == 0.0f) {
        frag->Flags() &= ~0x1000;
    }
    else {
        frag->Flags() |= 0x1000;
    }
}

static int FragCreatedEntry = 0x10AF0443;
__declspec(naked) void FragCreated() {
    static FragGrenade* frag;
    __asm {
        pushad
        mov [frag], eax
    }
    FragCreated(frag);
    __asm {
        popad
        ret
    }
}

static int FragUpdatedEntry = 0x10BFAC14;
__declspec(naked) void FragUpdated() {
    static int Return = 0x10AB8990;
    static FragGrenade* frag;
    __asm {
        mov [frag], ecx
        pushad
    }
    FragCreated(frag);
    __asm {
        popad
        jmp dword ptr[Return]
    }
}

const float MaxRightTurn = 2666.666f;
const float MaxLeftTurn = -2666.666f;
static float minMovement = 151.0f;
float camYawDelta;
float camPitchDelta;
static std::pair<float, float> inputPair;

const float camSens = 0.4f;
void BufferInput(float yawInput, float pitchInput) {
    float originalYawInput = yawInput;
    float originalPitchInput = pitchInput;
    if (Engine::gameState.inCameraView) {
        Engine::gameState.lvIn->lPlC()->Sens() = camSens;
        yawInput *= Config::sens_cam * 10.0f;
        //camYawDelta += yawInput;
        //float quotient = camYawDelta / minMovement;
        //yawInput = minMovement * static_cast<int>(quotient);
        //camYawDelta -= yawInput;

        pitchInput *= Config::sens_cam * 10.0f;
        //camPitchDelta += pitchInput;
        //quotient = camPitchDelta / minMovement;
        //pitchInput = minMovement * static_cast<int>(quotient);
        //camPitchDelta -= pitchInput;
    }
    else {
        Engine::gameState.lvIn->lPlC()->Sens() = 4.0f;
        camYawDelta = 0;
        camPitchDelta = 0;
    }

    if (yawInput < MaxLeftTurn || yawInput > MaxRightTurn || pitchInput < MaxLeftTurn || pitchInput > MaxRightTurn) {
        float sensRatio;
        if (fabs(yawInput) > fabs(pitchInput)) {
            sensRatio = fabs(yawInput) / MaxRightTurn;
            yawInput = std::clamp(yawInput, MaxLeftTurn, MaxRightTurn);
            pitchInput /= sensRatio;
        }
        else {
            sensRatio = fabs(pitchInput) / MaxRightTurn;
            pitchInput = std::clamp(pitchInput, MaxLeftTurn, MaxRightTurn);
            yawInput /= sensRatio;
        }

        Engine::gameState.lvIn->lPlC()->Sens() *= sensRatio;
    }

    inputPair = std::make_pair(yawInput, pitchInput);
}

static int PitchYawInputEntry = 0x10AAAA32;
__declspec(naked) void PitchYawInput() {
    static float yawInput;
    static float pitchInput;
    __asm {
        mov eax, dword ptr[esi + 0x4E0]
        mov dword ptr[yawInput], eax
        mov eax, dword ptr[esi + 0x4EC]
        mov dword ptr[pitchInput], eax
    }
    BufferInput(yawInput, pitchInput);
    yawInput = inputPair.first;
    pitchInput = inputPair.second;

    static int Return = 0x10AAAA38;
    __asm {
        mov eax, dword ptr[pitchInput]
        mov dword ptr[esi + 0x4EC], eax

        mov eax, dword ptr[yawInput]
        mov dword ptr[esi + 0x4E0], eax
        fld     dword ptr[esi + 0x4E0]
        jmp dword ptr[Return]
    }
}

static int PatchActorEntry = 0x1099B222;
__declspec(naked) void PatchActor() {
    static int Return = 0x1099B228;
    __asm {
        mov edx, dword ptr[ebx + 0x110]
        cmp edx,0
        jne notZero
        mov dword ptr[ebx + 0x110], 0x3C23D70A

        notZero:
        fld dword ptr[ebx + 0x110]
        jmp dword ptr[Return]
    }
}

static float ScreenShakeAccumulator(float timeDelta, float& accumulator, std::chrono::steady_clock::time_point& lastUpdate, int id) {
    auto alternateDeltaMs = std::chrono::duration<float>(Graphics::lastFrameTime - lastUpdate).count();
    lastUpdate = Graphics::lastFrameTime;
    if (alternateDeltaMs < 1.0 && timeDelta < alternateDeltaMs) {
        timeDelta = alternateDeltaMs;
    }

    float result = 0;
    accumulator += timeDelta;
    if (accumulator > 0.0333333333333333) {
        result = accumulator;
        accumulator = 0;
    }

    return result;
}

static int ScreenShake1Entry = 0x10A96327;
__declspec(naked) void ScreenShake1() {
    static int Return = 0x10A9632D;
    static float TimeDelta = 0.0;
    static float Accumulator = 0.0;
    static std::chrono::steady_clock::time_point LastUpdate;
    __asm {
        mov     edx, [ecx + 0x54C]
        mov dword ptr[TimeDelta], edx
        pushad
    }
    TimeDelta = ScreenShakeAccumulator(TimeDelta, Accumulator, LastUpdate, 1);
    __asm {
        popad
        mov     edx, dword ptr[TimeDelta]
        jmp dword ptr[Return]
    }
}

static int ScreenShake2Entry = 0x10A963E3;
__declspec(naked) void ScreenShake2() {
    static int Return = 0x10A963E9;
    static float TimeDelta = 0.0;
    static float Accumulator = 0.0;
    static std::chrono::steady_clock::time_point LastUpdate;
    __asm {
        mov     edx, [ecx + 0x54C]
        mov dword ptr[TimeDelta], edx
        pushad
    }
    TimeDelta = ScreenShakeAccumulator(TimeDelta, Accumulator, LastUpdate, 2);
    __asm {
        popad
        mov     edx, dword ptr[TimeDelta]
        jmp dword ptr[Return]
    }
}

//static int ScreenShake3Entry = 0x10A9641C;
//__declspec(naked) void ScreenShake3() {
//    static int Return = 0x10A96422;
//    static float TimeDelta = 0.0;
//    static float Accumulator = 0.0;
//    static std::chrono::steady_clock::time_point LastUpdate;
//    __asm {
//        mov     eax, dword ptr[eax + 0x54C]
//        mov dword ptr[TimeDelta], eax
//        pushad
//    }
//    TimeDelta = ScreenShakeAccumulator(TimeDelta, Accumulator, LastUpdate, 3);
//    __asm {
//        popad
//        fmul dword ptr[TimeDelta]
//        jmp dword ptr[Return]
//    }
//}

bool CanWaistLedge() {
    auto timeSinceJump = std::chrono::duration<float>(Graphics::lastFrameTime - Engine::gameState.lastJumpTime).count();
    const float minimumTime = 1.0f / 64;
    return timeSinceJump >= minimumTime;
}

int WaistLedgeEntry = 0x10AB2DF0;
__declspec(naked) void WaistLedge() {
    static int Return = 0x10AB2DF7;
    static bool allowed = false;
    __asm {
        pushad
    }
    allowed = CanWaistLedge();
    __asm {
        popad
        cmp byte ptr[allowed], 1
        je canGrab
        ret

        canGrab:
        push    ecx
        push    0x0
        lea     ecx, [esp + 0x4]
        jmp dword ptr[Return]
    }
}

static int ServerInfoVEntry = 0x10AB4015;
__declspec(naked) void ServerInfoV() {
    static int version;
    _asm {
        pushad
    }
    version = ScctVersion();
    static int Return = 0x10AB4020;
    __asm {
        popad
        mov eax, dword ptr[version]
        mov dword ptr[esp + 0xDC], eax
        jmp dword ptr[Return]
    }
}

static int ServerBrowserVEntry = 0x10AB35E7;
__declspec(naked) void ServerBrowserV() {
    static int version;
    _asm {
        pushad
    }
    version = ScctVersion();
    static int Return = 0x10AB35ED;
    __asm {
        popad
        cmp     esi, dword ptr[version]
        jmp dword ptr[Return]
    }
}

static const wchar_t* KnownEnhancedMap = L"AquaD";
static const wchar_t* KnownDefaultMap = L"AQU03";
const wchar_t* VerifyMapName(const wchar_t* mapName, int version) {
    if (ScctVersion() == version) {
        return mapName;
    }
    else if (IsScct1_2()) {
        return KnownDefaultMap;
    }
    return KnownEnhancedMap;
}

static int DisableMissingMapVersionErrorEntry = 0x10AB3B9F;
__declspec(naked) void DisableMissingMapVersionError() {
    static int Return = 0x10AB3BA7;
    static const wchar_t* MapName;
    static int Version;
    __asm {
        lea     ecx, [ebx + 0x38]
        mov dword ptr[MapName], ecx
        mov     ecx, [ebx + 0x0A8]
        mov dword ptr[Version], ecx
        pushad
    }
    MapName = VerifyMapName(MapName, Version);
    __asm {
        popad
        mov ecx, dword ptr[MapName]
        push    ecx
        lea     eax, [edi + edx + 0x14]
        jmp dword ptr[Return]
    }
}

void Engine::Initialize()
{
    SetGameVersionIdentifier();
    InitLabelOverrides();

    MemoryWriter::WriteJump(InstaFixEntry, InstaFix);
    MemoryWriter::WriteJump(FragCreatedEntry, FragCreated);
    MemoryWriter::WriteFunctionPtr(FragUpdatedEntry, FragUpdated);

    MemoryWriter::WriteJump(SetLvInEntry, SetLvIn);
    MemoryWriter::WriteJump(OnStateChangeEntry, OnStateChange);
    MemoryWriter::WriteJump(LoadTextEntry, LoadText);

    MemoryWriter::WriteFunctionPtr(GuiPageWaitUpdateEntry, GuiPageWaitUpdate);
    MemoryWriter::WriteFunctionPtr(GuiPageWaitUpdateEntry2, GuiPageWaitUpdate);
    MemoryWriter::WriteJump(GuiPageWaitLoadEntry, GuiPageWaitLoad);
    MemoryWriter::WriteJump(SPlaProCreatedEntry, SPlaProCreated);
    MemoryWriter::WriteJump(PitchYawInputEntry, PitchYawInput);
    MemoryWriter::WriteJump(PatchActorEntry, PatchActor);

    MemoryWriter::WriteJump(ScreenShake1Entry, ScreenShake1);
    MemoryWriter::WriteJump(ScreenShake2Entry, ScreenShake2);

    MemoryWriter::WriteJump(WaistLedgeEntry, WaistLedge);

    MemoryWriter::WriteJump(ServerInfoVEntry, ServerInfoV);
    MemoryWriter::WriteJump(ServerBrowserVEntry, ServerBrowserV);
    MemoryWriter::WriteJump(DisableMissingMapVersionErrorEntry, DisableMissingMapVersionError);

    if (Config::sticky_camera_fix) {
        MemoryWriter::WriteJump(StickyCamContextMenuBlockEntry, StickyCamContextMenuBlock);
        
        uint8_t shortJump[] = { 0xEB };
        MemoryWriter::WriteBytes(0x10B2CE1B, shortJump, sizeof(shortJump));
    }
}