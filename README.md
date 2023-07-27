# PeekMessage CPU Usage Fix (based on the Vinceho's Sid Meier's Alpha Centauri CPU Usage Fix)

## Description

This patch reduces the CPU usage when running older games, by hooking PeekMessageA function, and adding a configurable delay.

[Original repository](https://github.com/vinceho/smac-cpu-fix)

## Installation

1. Unpack the `loader.exe` and `loader.dll` into the game folder.

2. Create a shortcut of `loader.exe` and use the `-e` command line argument to target the game exe file.
   For passing additional command line arguments to the executable you can use `-p`.

   For example, use `-e "SimCity 4.exe" -p "-CPUCount:1 -CustomResolution:enabled -r1440x900x32 -w -intro:off -d:DirectX"` to target SimCity 4.

   Alternatively, execute the loader from the command line `loader.exe -e "SimCity 4.exe" -p "-CPUCount:1 -CustomResolution:enabled -r1440x900x32 -w -intro:off -d:DirectX"`

   For the convenience a bat file for SimCity 4 got included.

4. If the original executable requires administrator privledges, it has to be run by right clicking either the shortcut, or the bat file and choosing "Run as Administrator".

## Build instructions

1. From the [Tiny C Compiler repository](http://download.savannah.gnu.org/releases/tinycc/), download:
   - tcc-0.9.27-win32-bin.zip
   - winapi-full-for-0.9.27.zip
   - tcc-0.9.26-win64-bin.zip

2. Extract the contents of tcc-0.9.27-win32-bin
3. Overwrite the /include directory with the one included in winapi-full-for-0.9.27.zip
4. From tcc-0.9.26-win64-bin.zip copy tiny_impdef.exe into the same directory as tcc.exe
5. Copy loader.c, dll.c, and make.bat into the same directory as tcc.exe
6. Run make.bat
