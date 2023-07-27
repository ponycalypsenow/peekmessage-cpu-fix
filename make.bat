tiny_impdef C:\Windows\System32\winmm.dll
tcc -shared dll.c winmm.def -o loader.dll
tcc loader.c -o loader.exe -lshell32
