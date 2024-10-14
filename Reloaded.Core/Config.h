#pragma once

#include <string>
#include <vector>

class Config {
public:
    static int fps_client;
    static int fps_host;
    static bool alternate_frame_timing_mode;
    static bool animation_fix;
    static bool flashlight_rendering_fix;
    static bool widescreen;
    static float fov_cap;
    static bool force_max_refresh_rate;
    static bool labs_borderless_fullscreen;
    static std::vector<std::string> server_list;
    static bool security_acg;
    static bool security_dep;

    static bool sticky_camera_fix;

    static bool useDirectConnect;
    static std::wstring directConnectIp;
    static std::wstring directConnectPort;

    static std::string master_server_dns;

    static float sens;
    static float sens_menu;
    static float sens_camera;

    static float lod;

    // Constructor to initialize the config from a wide string path
    static void Initialize(std::wstring& configFilePath);
    static void ProcessCommandLine();
    static bool Serialize();
};

