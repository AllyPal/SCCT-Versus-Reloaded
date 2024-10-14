#pragma once
#include "logger.h"
#include "StringOperations.h"
class MemoryWriter
{
public:
    static bool __cdecl WriteBytes(uintptr_t targetAddress, const uint8_t* bytes, size_t length) {
        Logger::log("Writing bytes at " + StringOperations::toHexString(targetAddress));

        DWORD oldProtect;
        if (!VirtualProtect(reinterpret_cast<LPVOID>(targetAddress), length, PAGE_READWRITE, &oldProtect)) {
            Logger::log("Failed to change memory protection");
            return false;
        }

        memcpy(reinterpret_cast<void*>(targetAddress), bytes, length);
        if (!VirtualProtect(reinterpret_cast<LPVOID>(targetAddress), length, oldProtect, &oldProtect)) {
            Logger::log("Failed to restore memory protection");
            return false;
        }

        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<LPCVOID>(targetAddress), length);
        Logger::log("Finished writing bytes at " + StringOperations::toHexString(targetAddress));
        return true;
    }

    static bool __cdecl WriteFunctionPtr(uintptr_t targetAddress, void(*function)()) {
        Logger::log("Writing function pointer at " + StringOperations::toHexString(targetAddress));
        uintptr_t functionAddress = reinterpret_cast<uintptr_t>(function);
        uint32_t offset = static_cast<uint32_t>(functionAddress);

        uint8_t offsetBytes[4];
        *reinterpret_cast<uint32_t*>(offsetBytes) = static_cast<uint32_t>(offset);

        DWORD oldProtect;
        if (!VirtualProtect(reinterpret_cast<LPVOID>(targetAddress), sizeof(offsetBytes), PAGE_EXECUTE_READWRITE, &oldProtect)) {
            Logger::log("Failed to change memory protection");
            return false;
        }

        memcpy(reinterpret_cast<void*>(targetAddress), offsetBytes, sizeof(offsetBytes));
        if (!VirtualProtect(reinterpret_cast<LPVOID>(targetAddress), sizeof(offsetBytes), oldProtect, &oldProtect)) {
            Logger::log("Failed to restore memory protection");
            return false;
        }

        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<LPCVOID>(targetAddress), sizeof(offsetBytes));
        Logger::log("Finished writing function pointer at " + StringOperations::toHexString(targetAddress));
        return true;
    }

    static bool __cdecl WriteJump(uintptr_t targetAddress, void(*function)()) {
        Logger::log("Writing jump at " + StringOperations::toHexString(targetAddress));
        uintptr_t functionAddress = reinterpret_cast<uintptr_t>(function);
        uintptr_t relativeAddress = (functionAddress - targetAddress - 5);
        uint8_t jump[5];
        jump[0] = 0xE9; // JMP opcode
        *reinterpret_cast<uint32_t*>(jump + 1) = static_cast<uint32_t>(relativeAddress);

        DWORD oldProtect;
        if (!VirtualProtect(reinterpret_cast<LPVOID>(targetAddress), sizeof(jump), PAGE_EXECUTE_READWRITE, &oldProtect)) {
            Logger::log("Failed to change memory protection");
            return false;
        }

        memcpy(reinterpret_cast<void*>(targetAddress), jump, sizeof(jump));
        if (!VirtualProtect(reinterpret_cast<LPVOID>(targetAddress), sizeof(jump), oldProtect, &oldProtect)) {
            Logger::log("Failed to restore memory protection");
            return false;
        }

        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<LPCVOID>(targetAddress), sizeof(jump));
        Logger::log("Finished writing jump at " + StringOperations::toHexString(targetAddress));
        return true;
    }
};

