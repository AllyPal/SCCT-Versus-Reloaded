#pragma once
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
class Input
{
public:
	static void Initialize();
	static float aspectRatioMenuVertMouseInputMultiplier;
};

