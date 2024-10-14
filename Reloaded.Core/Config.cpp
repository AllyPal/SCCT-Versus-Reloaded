#include "pch.h"
#include "Config.h"
#include <fstream>
#include <iostream>
#include "include/nlohmann/json.hpp"
#include <vector>

static std::wstring configFilePathRef;
int Config::fps_client;
int Config::fps_host;
bool Config::alternate_frame_timing_mode;
bool Config::animation_fix;
bool Config::flashlight_rendering_fix;
bool Config::widescreen;
float Config::fov_cap;
bool Config::force_max_refresh_rate;
bool Config::labs_borderless_fullscreen;

bool Config::useDirectConnect;
std::wstring Config::directConnectIp;
std::wstring Config::directConnectPort;

std::string Config::master_server_dns;

float Config::sens_menu;
float Config::sens_camera;
float Config::sens;
bool Config::security_acg;
bool Config::security_dep;
bool Config::sticky_camera_fix;

float Config::lod;

std::vector<std::string> Config::server_list;

void Config::Initialize(std::wstring& configFilePath) {
    configFilePathRef = configFilePath;
    fps_client = 60;
    fps_host = 60;
    alternate_frame_timing_mode = false;
    animation_fix = true;
    flashlight_rendering_fix = true;
    widescreen = true;
    fov_cap = 105.0;
    force_max_refresh_rate = true;
    labs_borderless_fullscreen = false;
    useDirectConnect = false;
    directConnectIp = L"";
    directConnectPort = L"";
    master_server_dns = "scct-reloaded.duckdns.org:11000";
    sens_menu = 0.75;
    sens_camera = 1.0;
    sens = 1.0;
    security_acg = false;
    security_dep = true;
    sticky_camera_fix = true;
    lod = 10.0;

    std::ifstream configFile(configFilePathRef);

    if (configFile.is_open()) {
        try {
            std::string jsonString((std::istreambuf_iterator<char>(configFile)), std::istreambuf_iterator<char>());
            nlohmann::json jsonConfig = nlohmann::json::parse(jsonString, nullptr, true, true);

            fps_client = jsonConfig.value("fps_client", 60);
#ifndef _DEBUG
            fps_client = std::clamp(fps_client, fps_client_min, fps_client_max);
#endif
            fps_host = jsonConfig.value("fps_host", 60);
#ifndef _DEBUG
            fps_host = std::clamp(fps_host, fps_host_min, fps_host_max);
#endif
            alternate_frame_timing_mode = jsonConfig.value("alternate_frame_timing_mode", false);
            animation_fix = jsonConfig.value("animation_fix", true);
            flashlight_rendering_fix = jsonConfig.value("flashlight_rendering_fix", true);
            widescreen = jsonConfig.value("widescreen", true);
            fov_cap = jsonConfig.value("fov_cap", 105.0);
            force_max_refresh_rate = jsonConfig.value("force_max_refresh_rate", true);
            labs_borderless_fullscreen = jsonConfig.value("labs_borderless_fullscreen", false);
            sens_menu = jsonConfig.value("sens_menu", 0.75);
            sens_camera = jsonConfig.value("sens_camera", 1.0);
            sens = jsonConfig.value("sens", 1.0);
            server_list = jsonConfig.value("server_list", std::vector<std::string> {});
            security_acg = jsonConfig.value("security_acg", false);
            security_dep = jsonConfig.value("security_dep", true);
            sticky_camera_fix = jsonConfig.value("sticky_camera_fix", true);
            master_server_dns = jsonConfig.value("master_server_dns", "scct-reloaded.duckdns.org:11000");
            lod = jsonConfig.value("lod", 10.0);
            if (lod < 1.0) {
                lod = 1.0;
            }

            // Update the file with any new fields
            Serialize();
        }
        catch (const std::exception& e) {
            std::cerr << "Error reading config file: " << e.what() << std::endl;
        }
    }
    else {
        std::cerr << "Could not open config file. Using default values." << std::endl;
        Serialize(); // Write defaults to the file if it doesn't exist
    }
    ProcessCommandLine();
}

void Config::ProcessCommandLine()
{
    std::wstring commandLine = GetCommandLineW();
    std::wstring connectFlag = L"-join ";

    size_t pos = commandLine.find(connectFlag);

    if (pos != std::wstring::npos) {
        useDirectConnect = true;
        size_t start = pos + connectFlag.length();
        size_t colonPos = commandLine.find(L':', start);
        if (colonPos != std::wstring::npos) {
            directConnectIp = commandLine.substr(start, colonPos - start);
            directConnectPort = commandLine.substr(colonPos + 1);
        }
        else {
            directConnectIp = commandLine.substr(start);
        }

        if (directConnectIp.length() > 15 || directConnectIp.length() <= 7) {
            std::wcout << L"Invalid IP address format." << std::endl;
            useDirectConnect = false;
            directConnectIp.clear();
        }
    }

    if (useDirectConnect) {
        std::wcout << L"UseDirectConnect: true" << std::endl;
        std::wcout << L"IP: " << directConnectIp << std::endl;
        std::wcout << L"Port: " << directConnectPort << std::endl;
    }
    else {
        std::wcout << L"UseDirectConnect: false" << std::endl;
    }
}

void WriteJsonWithComments(const nlohmann::json& jsonData, const std::map<std::string, std::string>& comments, std::ofstream& configFile) {
    configFile << "{\n";

    bool first = true;
    for (auto& [key, value] : jsonData.items()) {
        if (!first) {
            configFile << ",\n";
        }
        first = false;

        // Check if a comment exists for the field and write it if available
        if (comments.find(key) != comments.end()) {
            configFile << "\n" << "  // " << comments.at(key) << "\n";
        }

        // Write the key-value pair in JSON format
        configFile << "  \"" << key << "\": " << value.dump();
    }

    configFile << "\n}\n";
}

bool Config::Serialize() {
    std::ofstream configFile(configFilePathRef);

    if (configFile) {
        try {
            nlohmann::json jsonConfig;

            std::map<std::string, std::string> comments;

            comments["fps_client"] = "FPS limit whilst connected to servers.";
            jsonConfig["fps_client"] = fps_client;

            comments["fps_host"] = "FPS limit whilst hosting.  It is not recommended to use higher than 60 with people who aren't using this patch.";
            jsonConfig["fps_host"] = fps_host;

            comments["alternate_frame_timing_mode"] = "May improve frame pacing on some systems, at the expense of increased CPU usage.\n  // Most people want this off.";
            jsonConfig["alternate_frame_timing_mode"] = alternate_frame_timing_mode;

            comments["animation_fix"] = "Fixes animated textures running too fast and looking like they're flickering at high FPS.";
            jsonConfig["animation_fix"] = animation_fix;

            comments["flashlight_rendering_fix"] = "Fixes rendering issues with lighting (e.g. the Merc flashlight)";
            jsonConfig["flashlight_rendering_fix"] = flashlight_rendering_fix;

            comments["sens_menu"] = "Mouse sensitivity in menus";
            jsonConfig["sens_menu"] = sens_menu;

            comments["sens_camera"] = "Mouse sensitivity for camera network and sticky cameras";
            jsonConfig["sens_camera"] = sens_camera;

            comments["sens"] = "Mouse sensitivity during gameplay";
            jsonConfig["sens"] = sens;

            comments["widescreen"] = "Use widescreen aspect ratio. With this off, your image will be stretched horizontally at widescreen resolutions.";
            jsonConfig["widescreen"] = widescreen;

            comments["fov_cap"] = "Caps field of view for people who are sensitive to motion sickness.\n  // The default is 105.0 which gives a similar experience to many modern FPS games, but settings up to 112 will increase Merc FOV.";
            jsonConfig["fov_cap"] = fov_cap;

            comments["force_max_refresh_rate"] = "Forces your game to run at your monitor's maximum resolution.";
            jsonConfig["force_max_refresh_rate"] = force_max_refresh_rate;

            comments["labs_borderless_fullscreen"] = "Experimental feature.  Your game will be stuck in the top left corner\n  // if you don't use your monitor's native resolution.";
            jsonConfig["labs_borderless_fullscreen"] = labs_borderless_fullscreen;

            comments["server_list"] = "";
            jsonConfig["server_list"] = server_list;

            comments["security_acg"] = "Security feature which may be incompatible with certain software like OBS, so should normally be kept off.";
            jsonConfig["security_acg"] = security_acg;

            comments["security_dep"] = "Recommended security feature to improve safety whilst playing online.";
            jsonConfig["security_dep"] = security_dep;

            comments["sticky_camera_fix"] = "Stops viewing previous sticky cam/exiting when attempting to fire a new camera.\n  // This entirely removes the context menu, so you will need to use the previous camera bind.";
            jsonConfig["sticky_camera_fix"] = sticky_camera_fix;

            jsonConfig["master_server_dns"] = master_server_dns;

            comments["lod"] = "Determines how far away models become lower quality.  The stock game uses 1.0";
            jsonConfig["lod"] = lod;

            WriteJsonWithComments(jsonConfig, comments, configFile);
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Error writing config file: " << e.what() << std::endl;
        }
    }
    else {
        std::cerr << "Could not open config file for writing. Check path and permissions." << std::endl;
    }
    return false;
}