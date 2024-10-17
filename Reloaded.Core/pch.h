// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H
constexpr auto NOP = 0x90;
constexpr auto consoleKeyBind = 0x29;
constexpr int fps_host_min = 30;
constexpr int fps_host_max = 165;
constexpr int fps_client_min = 30;
constexpr int fps_client_max = 240;
constexpr const wchar_t console_version_message[] = L"SCCT Versus Reloaded vI";
constexpr const char config_header[] = "/*\n"
"	SCCT Versus Reloaded V1\n"
"	Website: https://github.com/AllyPal/SCCT-Versus-Reloaded\n"
"*/\n\n";

#ifdef _DEBUG
	#define debug_cout std::cout << "DEBUG: "
	#define debug_wcout std::wcout << L"DEBUG: "
	#define debug_cerr std::cerr << "DEBUG: "

#else
	#define debug_cout if (false) std::cout
	#define debug_wcout if (false) std::wcout
	#define debug_cerr if (false) std::cerr
#endif

// add headers that you want to pre-compile here
#include "framework.h"

#endif //PCH_H
