tcc -shared dll.c -o loader.dll
tcc loader.c -o loader.exe -lshell32
