// Usage:
//    loader -e "SimCity 4.exe" -p "-CPUCount:1 -CPUPriority:low -CustomResolution:enabled -r1600x900x32 -w -intro:off"

#include <windows.h>
#include <stdio.h>
#include <shellapi.h>

typedef BOOL(WINAPI *__WriteProcessMemory__)(HANDLE, LPVOID, LPCVOID, DWORD, LPDWORD);
typedef PVOID(WINAPI *__VirtualAllocEx__)(HANDLE, LPVOID, DWORD, DWORD, DWORD);

const CHAR DLL_NAME[] = "loader.dll";

CHAR g_exeName[MAX_PATH];
CHAR g_exeParams[MAX_PATH];
PROCESS_INFORMATION g_processInfo;
STARTUPINFO g_statusInfo;
PVOID g_memAddress = NULL;
HANDLE g_hDllInjectThread = NULL;

void clean(BOOL bTerminate)
{
  if (g_processInfo.hProcess)
  {
    if (g_hDllInjectThread)
    {
      CloseHandle(g_hDllInjectThread);
      g_hDllInjectThread = NULL;
    }

    if (g_memAddress)
    {
      VirtualFreeEx(g_processInfo.hProcess, g_memAddress, sizeof(DLL_NAME), MEM_RELEASE);
      g_memAddress = NULL;
    }

    if (bTerminate)
    {
      TerminateProcess(g_processInfo.hProcess, 1);
    }
  }
}

void ExitWithError(LPCSTR lpcsMessage)
{
  MessageBox(NULL, lpcsMessage, "Error", MB_OK | MB_ICONERROR);

  clean(TRUE);

  ExitProcess(1);
}

BOOL parseCommandLine()
{
  LPWSTR *szArgList;
  int nArgs;

  szArgList = CommandLineToArgvW(GetCommandLineW(), &nArgs);
  if (szArgList == NULL)
  {
    return FALSE;
  }
  else
  {
    for (int i = 0; i < nArgs; i++)
    {
      if (!wcscmp(L"-e", szArgList[i]))
      {
        if (++i < nArgs)
        {
          wcstombs(g_exeName, szArgList[i], sizeof(g_exeName));
        }
      }
      else if (!wcscmp(L"-p", szArgList[i]))
      {
        if (++i < nArgs)
        {
          wcstombs(g_exeParams, szArgList[i], sizeof(g_exeName));
        }
      }
    }
  }

  LocalFree(szArgList);

  if (strlen(g_exeName) == 0)
  {
    return FALSE;
  }

  return TRUE;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
  ZeroMemory(g_exeName, sizeof(g_exeName));
  ZeroMemory(g_exeParams, sizeof(g_exeParams));
  ZeroMemory(&g_processInfo, sizeof(g_processInfo));
  ZeroMemory(&g_statusInfo, sizeof(g_statusInfo));
  g_statusInfo.cb = sizeof(g_statusInfo);

  if (!parseCommandLine())
  {
    ExitWithError("Usage: loader.exe [options]\n\nOptions:\n  -e file    The target .exe file to patch\n  -p params    Additional arguments");
  }

  if (!CreateProcess(g_exeName, strlen(g_exeParams) != 0 ? g_exeParams : NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &g_statusInfo, &g_processInfo))
  {
    ExitWithError("Failed to run application.");
  }

  HMODULE hKernel = GetModuleHandle("Kernel32");

  if (!hKernel)
  {
    ExitWithError("Failed to get Kernel32 module.");
  }

  FARPROC lpfLoadLibraryAddress = GetProcAddress(hKernel, "LoadLibraryA");
  __WriteProcessMemory__ lpfWriteProcessMemory = (__WriteProcessMemory__)GetProcAddress(hKernel, "WriteProcessMemory");
  __VirtualAllocEx__ lpfVirtualAllocEx = (__VirtualAllocEx__)GetProcAddress(hKernel, "VirtualAllocEx");

  if (!lpfLoadLibraryAddress || !lpfWriteProcessMemory || !lpfVirtualAllocEx)
  {
    ExitWithError("Failed to get library functions.");
  }

  g_memAddress = lpfVirtualAllocEx(g_processInfo.hProcess, NULL, sizeof(DLL_NAME), MEM_COMMIT, PAGE_EXECUTE_READWRITE);

  if (!g_memAddress)
  {
    ExitWithError("Failed to allocate memory for DLL injection.");
  }

  if (!lpfWriteProcessMemory(g_processInfo.hProcess, g_memAddress, DLL_NAME, sizeof(DLL_NAME), NULL))
  {
    ExitWithError("Failed to write to memory for DLL injection.");
  }

  g_hDllInjectThread = CreateRemoteThread(g_processInfo.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)lpfLoadLibraryAddress, g_memAddress, 0, NULL);

  if (!g_hDllInjectThread)
  {
    ExitWithError("Failed to create DLL injection thread.");
  }

  // Wait 30 seconds.
  DWORD dwResult = WaitForSingleObject(g_hDllInjectThread, 30000);

  if (dwResult == WAIT_TIMEOUT || dwResult == WAIT_FAILED)
  {
    ExitWithError("Failed to execute DLL injection thread.");
  }

  clean(FALSE);
  ResumeThread(g_processInfo.hThread);

  return 0;
}
