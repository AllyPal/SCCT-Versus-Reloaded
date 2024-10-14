#include "pch.h"
#include "Input.h"
#include <dinput.h>
#include <format>
#include <Windows.h>
#include "StringOperations.h"
#include "MemoryWriter.h"
#include "Config.h"

static int xMouseDelta = 0;
static int yMouseDelta = 0;

int DisableMouseInputEntry = 0x10B10CF3;
__declspec(naked) void DisableMouseInput() {
    static int DoNotProcess = 0x10B10CAE;
    static int KeepProcessing = 0x10B10CF8;
    __asm {
        cmp     eax, 0x10
        ja      doNotProcess
        cmp     eax, 0x0
        je      mouseX
        cmp     eax, 0x4
        je      mouseY

        doProcess :
        jmp     dword ptr[KeepProcessing]

            doNotProcess :
            jmp     dword ptr[DoNotProcess]

            mouseX :
            push eax
            mov eax, dword ptr[xMouseDelta]
            cmp eax, 0
            pop eax
            je doNotProcess
            jmp doProcess

            mouseY :
        push eax
            mov eax, dword ptr[yMouseDelta]
            cmp eax, 0
            pop eax
            je doNotProcess
            jmp doProcess
    }
}

float Input::aspectRatioMenuVertMouseInputMultiplier = 1.0;
static float menuPositionX = 240.0f;
static float menuPositionY = 320.0f;

void HandleMouseInput(LPDIRECTINPUTDEVICE8 device, int dd) {
    DIMOUSESTATE2 mouseState;
    HRESULT hr = device->GetDeviceState(sizeof(DIMOUSESTATE2), &mouseState);

    if (FAILED(hr)) {
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
            debug_cerr << "Device lost or not acquired. Attempting to reacquire..." << std::endl;
            hr = device->Acquire();
            if (FAILED(hr)) {
                debug_cerr << "Failed to acquire device." << std::endl;
                return;
            }
            if (SUCCEEDED(hr)) {
                debug_cerr << "Acquired device." << std::endl;
            }
            hr = device->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState);
        }

        if (FAILED(hr)) {
            debug_cerr << "Failed to get device state. Error code: " << StringOperations::toHexString(hr) << std::endl;
            return;
        }
    }
    xMouseDelta = mouseState.lX;
    yMouseDelta = mouseState.lY;

    menuPositionY += yMouseDelta * Config::sens_menu * Input::aspectRatioMenuVertMouseInputMultiplier;
    menuPositionY = std::clamp(menuPositionY, 0.0f, 480.0f);

    menuPositionX += xMouseDelta * Config::sens_menu;
    menuPositionX = std::clamp(menuPositionX, 0.0f, 640.0f);
}


int FixMouseInputEntry = 0x10B10C80;
__declspec(naked) void FixMouseInput() {
    static int DevicePtr = 0x10C91D04;
    static int dd;
    static LPDIRECTINPUTDEVICE8 device;
    __asm {
        pushad
        mov eax, DevicePtr
        cmp eax, 0
        je exitf
        mov eax, dword ptr[eax]
        je exitf
        mov dword ptr[device], eax
        mov dword ptr[dd], esi
    }
    HandleMouseInput(device, dd);
    static int Return = 0x10B10C86;
    __asm {
    exitf:
        popad
            test ebp, ebp
            mov[esp + 0x10], ebx
            jmp dword ptr[Return]
    }
}

int X_WriteMouseInputEntry = 0x10B10D06;
__declspec(naked) void X_WriteMouseInput() {
    static int Resume = 0x10B10D0D;

    __asm {
        mov eax, [esi + 0x18]
        fild dword ptr[xMouseDelta]
        jmp dword ptr[Resume]
    }
}

int Y_WriteMouseInputEntry = 0x10B10D23;
__declspec(naked) void Y_WriteMouseInput() {
    static int Resume = 0x10B10D2D;


    __asm {
        mov     eax, [esi + 0x18]
        mov     ecx, [eax + 0x28]
        mov     eax, dword ptr[yMouseDelta]
        jmp dword ptr[Resume]
    }
}

const float ten = 10;
int BaseMouseSensitivityEntry = 0x109FC177;
__declspec(naked) void BaseMouseSensitivity() {
    __asm {
        fmul dword ptr[Config::sens]
        fdiv dword ptr[ten]
        fstp dword ptr[edx + edi]
        pop edi
        pop esi
        ret 0004
    }
}

const float ScaledSpeed = 1.0;
int NegativeAccelerationEntry = 0x109FDA59;
__declspec(naked) void NegativeAcceleration() {
    static int Return = 0x109FDA5E;
    __asm {
        push    edx
        mov edx, dword ptr[ScaledSpeed]
        jmp dword ptr[Return]
    }
}

int MenuMouseSensitivityYEntry = 0x10A56B2D;
__declspec(naked) void MenuMouseSensitivityY() {
    static int Return = 0x10A56B44;
    //static float yRemainder = 0.0;
    __asm {
        /*fild dword ptr[yMouseDelta]
        fchs

        fmul dword ptr[aspectRatioMenuVertMouseInputMultiplier]
        fmul dword ptr[Config::sens_menu]
        fsubp st(1), st(0)
        fadd dword ptr[yRemainder]
        fst dword ptr[yRemainder]
        call dword ptr[RoundFpuFunction]
        mov[esi + 0x00000CD0], eax
        fld dword ptr[yRemainder]
        fisub dword ptr[esi + 0x00000CD0]
        fstp dword ptr[yRemainder]*/
        fld dword ptr[menuPositionY]
        fistp dword ptr[esi + 0x00000CD0]

        jmp dword ptr[Return]
    }
}

int MenuMouseSensitivityXEntry = 0x10A56A76;
__declspec(naked) void MenuMouseSensitivityX() {
    static int Return = 0x10A56A98;
    //static float xRemainder = 0.0;
    __asm {
        fild dword ptr[xMouseDelta]
        add esp, 0x4
        fst     dword ptr[esp + 0x1C]

        /* fmul dword ptr[Config::sens_menu]
         fiadd[esi + 0x00000CCC]
         fadd dword ptr[xRemainder]
         fst dword ptr[xRemainder]
         call dword ptr[RoundFpuFunction]
         mov[esi + 0x00000CCC], eax
         fld dword ptr[xRemainder]
         fisub dword ptr[esi + 0x00000CCC]
         fstp dword ptr[xRemainder]*/
         fld dword ptr[menuPositionX]
         fistp dword ptr[esi + 0x00000CCC]

         jmp dword ptr[Return]
    }
}

void Input::Initialize()
{
    MemoryWriter::WriteJump(DisableMouseInputEntry, DisableMouseInput);
    MemoryWriter::WriteJump(FixMouseInputEntry, FixMouseInput);
    MemoryWriter::WriteJump(X_WriteMouseInputEntry, X_WriteMouseInput);
    MemoryWriter::WriteJump(Y_WriteMouseInputEntry, Y_WriteMouseInput);
    MemoryWriter::WriteJump(MenuMouseSensitivityYEntry, MenuMouseSensitivityY);
    MemoryWriter::WriteJump(MenuMouseSensitivityXEntry, MenuMouseSensitivityX);
    MemoryWriter::WriteJump(BaseMouseSensitivityEntry, BaseMouseSensitivity);
    MemoryWriter::WriteJump(NegativeAccelerationEntry, NegativeAcceleration);
}