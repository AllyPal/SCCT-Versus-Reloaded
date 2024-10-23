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
#include <ctime>

static std::wstring logFilePath;

std::string TimePrefix() {
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime;

    // Use localtime_s for thread safety
    localtime_s(&localTime, &currentTime);

    std::ostringstream oss;
    oss << std::put_time(&localTime, "[%H:%M:%S] ");
    return oss.str();
}

std::wstring TimePrefixW() {
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime;

    // Use localtime_s for thread safety
    localtime_s(&localTime, &currentTime);

    std::wostringstream woss;
    woss << std::put_time(&localTime, L"[%H:%M:%S] ");
    return woss.str();
}

void Logger::log(const std::string& message) {
    std::ofstream logFile(logFilePath, std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Error: Could not open the file for logging." << std::endl;
        return;
    }
    
    logFile << TimePrefix() << message << std::endl;
    logFile.close();
}

void Logger::log(const std::wstring& message) {
    std::wofstream logFile(logFilePath, std::ios::app);
    if (!logFile.is_open()) {
        std::wcerr << L"Error: Could not open the file for logging." << std::endl;
        return;
    }

    logFile << TimePrefixW() << message << std::endl;
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
