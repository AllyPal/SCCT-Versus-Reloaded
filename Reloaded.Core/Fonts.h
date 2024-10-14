#pragma once
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <string>
#include "GameStructs.h"

class Fonts
{
public:
	static void Initialize();

    static GUIFont* GetFontByKeyName(std::wstring keyName);
};
