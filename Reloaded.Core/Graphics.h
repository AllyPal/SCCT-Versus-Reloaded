#pragma once
#include <Windows.h>
#include <chrono>
#include "GameStructs.h"

struct DisplayModePair {
    std::wstring modeWithFormat;
    UINT width;
    UINT height;
    std::string aspectRatio;
};

struct ResolutionInfo {
    UINT width;
    UINT height;
    UINT refreshRate;
};

struct KnownAspectRatio {
    double width;
    double height;
    std::string name;
};

class Graphics
{
public:
	static void Initialize();
	static std::chrono::steady_clock::time_point lastFrameTime;
    static UcString* videoSettingsDisplayModes;
    static UcString* videoSettingsDisplayModesCmd;
    static int GetResolutionCount();
};

#define D3DX_PI    (3.14159265358979323846)
const double originalAspectRatioWidth = 4.0;
const double originalAspectRatioHeight = 3.0;
const double originalAspectRatio = originalAspectRatioHeight / originalAspectRatioWidth;
const double degToRadConversionFactor = D3DX_PI / 180.0;
const double radToDegConversionFactor = 180.0 / D3DX_PI;