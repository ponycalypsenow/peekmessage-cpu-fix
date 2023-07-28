# PeekMessage CPU Usage Fix (based on the Vinceho's Sid Meier's Alpha Centauri CPU Usage Fix)

## Description

This patch reduces the CPU usage when running older games, by hooking PeekMessageA function, and adding a configurable delay.

[Original repository](https://github.com/vinceho/smac-cpu-fix)

## Installation

1. Unpack the `loader.exe` and `loader.dll` into the game folder (same directory as the entrypoint exe).

2. Create a shortcut of `loader.exe` and use the `-e` command line argument to target the game exe file.
   For passing additional command line arguments to the executable you can use `-p`.

   For example, use `-e "SimCity 4.exe" -p "-CPUCount:1 -CustomResolution:enabled -r1440x900x32 -w -intro:off -d:DirectX"` to target SimCity 4.

   Alternatively, execute the loader from the command line `loader.exe -e "SimCity 4.exe" -p "-CPUCount:1 -CustomResolution:enabled -r1440x900x32 -w -intro:off -d:DirectX"`

   For the convenience a bat file for SimCity 4 got included.

4. If the original executable requires administrator privledges, the shortcut has to be right-clicked, and run with "Run as Administrator" option.

   Similarly, the command prompt should be started with the administrator rights.

## Building instructions
### Tiny C Compiler

1. From the [Tiny C Compiler repository](http://download.savannah.gnu.org/releases/tinycc/), download:
   - tcc-0.9.27-win32-bin.zip
   - winapi-full-for-0.9.27.zip

2. Extract the contents of tcc-0.9.27-win32-bin
3. Overwrite the /include directory with the one included in winapi-full-for-0.9.27.zip
4. Copy loader.c, dll.c, and make_tcc.bat into the same directory as tcc.exe
5. Run make_tcc.bat, or manually execute:
      ```tcc -shared dll.c -o loader.dll
      tcc loader.c -o loader.exe -lshell32```

### Digital Mars C/C++ Compiler

1. From the [Digital Mars C/C++ Compiler website](https://digitalmars.com/download/freecompiler.html), download:
      - dm857c.zip
2. Extract the contents of dm857c.zip
3. Copy loader.c, dll.c, and make_dmc.bat into the same directory as dmc.exe (it's in the /bin subfolder)
4. Inside of the /bin subfolder, run make_dmc.bat, or manually execute:
      ```dmc loader.c shell32.lib
      dmc dll.c kernel32.lib -WD -oloader.dll```
