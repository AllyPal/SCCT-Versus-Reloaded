# SCCT Versus Reloaded

An unofficial community patch that brings enhancements and fixes for Splinter Cell Chaos Theory (SCCT) Versus.

## Features:
* Allows players to connect to other players over the internet (without a VPN)
* Adds a master server
* Fixes the flashlight rendering bug without the need to use 3DAnalyze or dgvoodoo
* Allows users to set a frame limit (no longer capped at 64 FPS on Windows with SCCT Frame Rate Editor/Framer)
* Allows users to specify a different frame rate limit when hosting.  Currently hosting above 90 fps is not recommended (up from 31 on the stock game)
* Fixes issue where grenades (frag, chaff, smoke etc) intermittently instantly explode on impact with the floor
* Replaces the mouse input system - now works above 125hz, removes negative acceleration and is no longer affected by frame rate
* Fixes animated textures/shaders (e.g. the water effect on Aquarius) that run too fast at higher frame rates.  Textures/shaders which already specify a limit will retain their original cap
* Adds widescreen support up to 16:9 aspect ratio without making the 3d image look stretched
* Fixes stretched icons on Enhanced Reality with widescreen
* FOV dynamically adjusts to give a consistent vertical view at all aspect ratios
* Improved frame timing for more consistent frame rate
* The config file has a "serverList" section, which allows users to enter a list of IP addresses.  This means people can play even if the new master server goes away
* Fixes bug where holding forwards or backwards whilst attempting to fire a sticky cam would sometimes view the previous camera or do nothing
* Adds numerous commands to the console to configure your game (type 'help' in the console to see the full list)
* Fixes bug where the game runs much faster and causes severe lag when the host is alt-tabbed

## Installation
* Unzip files into the root directory of SCCT Versus
* Launch the game with SCCT_Launcher.exe
* After the first run, SCCT_config.json will be created.  You can set your desired frame rate here.
* It is not advised to run above 60 FPS whilst hosting

## Beta Builds
Main: https://nightly.link/AllyPal/SCCT-Versus-Reloaded/workflows/msbuild/main/Beta.zip
