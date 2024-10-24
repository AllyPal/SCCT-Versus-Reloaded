#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <Windows.h>
#include <thread>
#include <chrono>
#include "logger.h"
#include "Config.h"
#include <functional>

class GameConsole
{
public:
    static void Initialize();
    static void WriteGameConsole(std::wstring message);
    static void WriteChatBox(std::wstring displayText);
};

struct CommandHandler {
    std::wstring description;
    std::function<void(const std::wstring&)> handler;
    std::wstring displayValue;
};

const std::wstring cb_prefix = L" > ";