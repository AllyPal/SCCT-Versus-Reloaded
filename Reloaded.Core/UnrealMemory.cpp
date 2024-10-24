#include "pch.h"
#include "UnrealMemory.h"
#include <iostream>

typedef void* (*malloc_t)(size_t);

void* UnrealMemory::malloc(size_t size) {
    return (*reinterpret_cast<malloc_t*>(0x10BDF2C4))(size);
}
