#include "pch.h"
#include "Fonts.h"
#include <string>
#include <iostream>
#include "MemoryWriter.h"

static std::unordered_map<std::wstring, GUIFont*> fontMap;

static void OnFontCreated(GUIFont* font) {
	std::wstring keyName(font->KeyName());
	fontMap[keyName] = font;
	debug_wcout << keyName << std::endl;
}

static int FontCreatedEntry = 0x10A69136;
__declspec(naked) void FontCreated() {
	static GUIFont* font;
	__asm {
		jz      short nope
		mov dword ptr[eax], 0x10C00838
		pushad
		mov [font], esi
	}
	OnFontCreated(font);
	__asm{
		popad
		nope:
		ret
	}
}

GUIFont* Fonts::GetFontByKeyName(std::wstring keyName) {
	auto it = fontMap.find(keyName);
	return it->second;
}

void Fonts::Initialize()
{
	MemoryWriter::WriteJump(FontCreatedEntry, FontCreated);
}
