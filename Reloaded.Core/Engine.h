#pragma once
#include "pch.h"
#include "Config.h"
#include "include/nlohmann/json.hpp"
#include "logger.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <Windows.h>
#include "GameStructs.h"

struct State {
	LvIn* lvIn = nullptr;
	bool inCameraView = false;
	bool inCoop = false;
	std::chrono::steady_clock::time_point lastJumpTime;
};

class Engine
{
private:
	
public:
	static void Initialize();
	static bool IsListenServer();
	static void EnableProcessSecurity();
	static void OncePerFrame();
	static void SetLabelOverride(const wchar_t* element, const wchar_t* page, std::wstring message);
	static State gameState;
};
