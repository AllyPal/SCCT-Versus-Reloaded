#pragma once

#include "pch.h"
#include "logger.h"
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <Windows.h>
#include <thread>
#include <chrono>

class Logger
{
public:
	static void log(const std::string& message);
	static void log(const std::wstring& message);

	static void Initialize(const std::wstring& dllPath);
};
