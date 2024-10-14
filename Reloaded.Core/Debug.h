#pragma once
#include <string>
#include "logger.h"
#include "GameConsole.h"
#include <map>

class Debug
{
public:
    static void Initialize();
    static void CommandHandlers(std::map<std::wstring, CommandHandler>* commandHandlers);
};
