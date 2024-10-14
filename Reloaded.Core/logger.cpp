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
#include <memory>
#include <chrono>

static std::wstring logFilePath;

void Logger::log(const std::string& message) {
    std::ofstream logFile(logFilePath, std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Error: Could not open the file for logging." << std::endl;
        return;
    }

    logFile << message << std::endl;
    logFile.close();
}

void Logger::log(const std::wstring& message) {
    std::wofstream logFile(logFilePath, std::ios::app);
    if (!logFile.is_open()) {
        std::wcerr << L"Error: Could not open the file for logging." << std::endl;
        return;
    }

    logFile << message << std::endl;
    logFile.close();
}

void Logger::Initialize(const std::wstring& dllPath)
{
    logFilePath = dllPath + L".log";
    std::wofstream logFile(logFilePath, std::ios::trunc);
    if (!logFile.is_open()) {
        std::wcerr << L"Error: Could not open the file for clearing." << std::endl;
        return;
    }
    logFile.close();
}
