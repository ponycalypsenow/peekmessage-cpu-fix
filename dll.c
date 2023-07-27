#include <stdio.h>
#include <windows.h>
#include <psapi.h>
#include <dbghelp.h>
#include <math.h>

WINMMAPI DWORD WINAPI timeGetTime(void);

#define MODULE_NAME "loader.dll"
#define CONFIG_FILE "loader.ini"
#define LOG_FILE "loader.log"
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_DEBUG 3
#define LOG_LEVEL_TRACE 4

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

typedef enum
{
    CVT_NUMBER,
    CVT_BOOL,
    CVT_FLOAT
} ConfigValueType;

typedef struct
{
    LPCSTR lpcsSection;
    LPCSTR lpcsName;
    ConfigValueType type;
    LPVOID lpvValue;
} ConfigLink;

typedef struct
{
    // Options
    DWORD dwLogLevel;

    // FixCpuUsage
    DWORD dwMessageWaitTimeMinMs;
    DWORD dwMessageWaitTimeMaxMs;
    DWORD dwMessageWaitTimeThresholdMs;
    DWORD dwMessageProcessingTimeThresholdMs;
} Config;

Config g_config = {
    // Options
    0,
    // FixCpuUsage
    25, 50, 250, 50};

ConfigLink configLinks[] = {
    {"Options", "Log", CVT_NUMBER, &g_config.dwLogLevel},

    {"FixCpuUsage", "MessageWaitTimeMinMs", CVT_NUMBER, &g_config.dwMessageWaitTimeMinMs},
    {"FixCpuUsage", "MessageWaitTimeMaxMs", CVT_NUMBER, &g_config.dwMessageWaitTimeMaxMs},
    {"FixCpuUsage", "MessageWaitTimeThresholdMs", CVT_NUMBER, &g_config.dwMessageWaitTimeThresholdMs},
    {"FixCpuUsage", "MessageProcessingTimeThresholdMs", CVT_NUMBER, &g_config.dwMessageProcessingTimeThresholdMs}};

CRITICAL_SECTION g_criticalSection;

DOUBLE g_dTimerFrequency;
DOUBLE g_dTimerStart;
BOOL g_bTimerHighResolution = FALSE;

// Timer State
DOUBLE g_dBeginWorkTime = 0.0;
DOUBLE g_dBeginSleepTime = 0.0;

// Config
DOUBLE g_dMessageWaitTimeThreshold = 0.0;
DOUBLE g_dMessageProcessingTimeThreshold = 0.0;
DWORD g_dwMessageWaitTimeMin = 0;
DWORD g_dwMessageWaitTimeMax = 0;
DWORD g_dwMessageWaitTime = 0;
DWORD g_dwMessageWaitTimeInc = 0;

BOOL HasConfig();
BOOL ReadConfig();
BOOL WriteConfig();

BOOL InitializeTimer();
DOUBLE GetTimerCurrentTime();

BOOL WINAPI PeekMessageEx(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
BOOL PatchCpuUsage(HANDLE);

BOOL InitializeConfig()
{
    if (HasConfig())
    {
        return ReadConfig();
    }

    return WriteConfig();
}

DWORD GetMinMax(DWORD dwValue, DWORD dwMin, DWORD dwMax)
{
    if (dwValue < dwMin)
    {
        return dwMin;
    }
    else if (dwValue > dwMax)
    {
        return dwMax;
    }

    return dwValue;
}

FLOAT GetMinMaxFloat(FLOAT fValue, FLOAT fMin, FLOAT fMax)
{
    if (fValue < fMin)
    {
        return fMin;
    }
    else if (fValue > fMax)
    {
        return fMax;
    }

    return fValue;
}

BOOL ValidateConfig()
{
    g_config.dwLogLevel = GetMinMax(0, g_config.dwLogLevel, 4);

    g_config.dwMessageWaitTimeMinMs = GetMinMax(0, g_config.dwMessageWaitTimeMinMs, 5000);
    g_config.dwMessageWaitTimeMaxMs = GetMinMax(g_config.dwMessageWaitTimeMinMs, g_config.dwMessageWaitTimeMaxMs, 5000);
    g_config.dwMessageWaitTimeThresholdMs = GetMinMax(0, g_config.dwMessageWaitTimeThresholdMs, 3600000);
    g_config.dwMessageProcessingTimeThresholdMs = GetMinMax(0, g_config.dwMessageProcessingTimeThresholdMs, 3600000);

    return TRUE;
}

BOOL HasConfig()
{
    FILE *file = file = fopen(CONFIG_FILE, "r");

    if (file)
    {
        fclose(file);

        return TRUE;
    }

    return FALSE;
}

BOOL ReadConfig()
{
    DWORD dwSize = (sizeof(configLinks) / sizeof(ConfigLink));
    FILE *file = fopen(CONFIG_FILE, "r");
    DWORD dwBufferSize = 64;
    char buffer[dwBufferSize], name[dwBufferSize], value[dwBufferSize];

    // Ignore INI sections when reading for simplicity.
    if (file)
    {
        while (TRUE)
        {
            if (fgets(buffer, dwBufferSize, file) == NULL)
            {
                break;
            }

            if (sscanf(buffer, " %[^= ] = %s ", name, value) == 2)
            {
                for (DWORD i = 0; i < dwSize; i++)
                {
                    LPCSTR lpcsName = configLinks[i].lpcsName;

                    if (!strcmp(lpcsName, name))
                    {
                        switch (configLinks[i].type)
                        {
                        case CVT_BOOL:
                            *(BOOL *)(configLinks[i].lpvValue) = (BOOL)atoi(value);
                            break;
                        case CVT_NUMBER:
                            *(DWORD *)(configLinks[i].lpvValue) = (DWORD)atoi(value);
                            break;
                        case CVT_FLOAT:
                            *(FLOAT *)(configLinks[i].lpvValue) = (FLOAT)atof(value);
                            break;
                        }

                        break;
                    }
                }
            }

            if (feof(file))
            {
                break;
            }
        }

        fclose(file);
    }

    return ValidateConfig();
}

BOOL WriteConfig()
{
    DWORD dwSize = (sizeof(configLinks) / sizeof(ConfigLink));
    FILE *file = fopen(CONFIG_FILE, "w");
    LPCSTR lpcsLastSection = NULL;

    for (DWORD i = 0; i < dwSize; i++)
    {
        LPCSTR lpcsCurrentSection = configLinks[i].lpcsSection;

        if (lpcsLastSection == NULL || strcmp(lpcsLastSection, lpcsCurrentSection))
        {
            if (lpcsLastSection != NULL)
            {
                fprintf(file, "\n");
            }

            fprintf(file, "[%s]\n", lpcsCurrentSection);
        }

        lpcsLastSection = lpcsCurrentSection;

        switch (configLinks[i].type)
        {
        case CVT_BOOL:
        case CVT_NUMBER:
            fprintf(file, "%s=%d\n", configLinks[i].lpcsName, *(DWORD *)(configLinks[i].lpvValue));
            break;
        case CVT_FLOAT:
            fprintf(file, "%s=%f\n", configLinks[i].lpcsName, *(FLOAT *)(configLinks[i].lpvValue));
            break;
        }
    }

    fclose(file);

    return TRUE;
}

BOOL IsLogEnabled()
{
    return (g_config.dwLogLevel > 0);
}

DWORD GetLogLevel()
{
    return g_config.dwLogLevel;
}

DWORD GetMessageWaitTimeMinMs()
{
    return g_config.dwMessageWaitTimeMinMs;
}

DWORD GetMessageWaitTimeMaxMs()
{
    return g_config.dwMessageWaitTimeMaxMs;
}

DWORD GetMessageWaitTimeThresholdMs()
{
    return g_config.dwMessageWaitTimeThresholdMs;
}

DWORD GetMessageProcessingTimeThresholdMs()
{
    return g_config.dwMessageProcessingTimeThresholdMs;
}

void InitializeLog()
{
    if (IsLogEnabled())
    {
        InitializeCriticalSection(&g_criticalSection);

        // Purge log file.
        fclose(fopen(LOG_FILE, "w"));
    }
}

void ShutdownLog()
{
    if (IsLogEnabled())
    {
        DeleteCriticalSection(&g_criticalSection);
    }
}

void Log(LPCSTR lpcsFormat, LPCSTR lpcsLevel, va_list args)
{
    EnterCriticalSection(&g_criticalSection);

    FILE *file = fopen(LOG_FILE, "a");

    fprintf(file, "[%.0lf] [%d] %s: ", GetTimerCurrentTime(), GetCurrentThreadId(), lpcsLevel);
    vfprintf(file, lpcsFormat, args);
    fprintf(file, "\n");

    fclose(file);

    LeaveCriticalSection(&g_criticalSection);
}

void LogInfo(LPCSTR lpcsFormat, ...)
{
    if (GetLogLevel() >= LOG_LEVEL_INFO)
    {
        va_list args;
        va_start(args, lpcsFormat);
        Log(lpcsFormat, "INFO", args);
        va_end(args);
    }
}

void LogError(LPCSTR lpcsFormat, ...)
{
    if (GetLogLevel() >= LOG_LEVEL_ERROR)
    {
        va_list args;
        va_start(args, lpcsFormat);
        Log(lpcsFormat, "ERROR", args);
        va_end(args);
    }
}

void LogDebug(LPCSTR lpcsFormat, ...)
{
    if (GetLogLevel() >= LOG_LEVEL_DEBUG)
    {
        va_list args;
        va_start(args, lpcsFormat);
        Log(lpcsFormat, "DEBUG", args);
        va_end(args);
    }
}

void LogTrace(LPCSTR lpcsFormat, ...)
{
    if (GetLogLevel() >= LOG_LEVEL_TRACE)
    {
        va_list args;
        va_start(args, lpcsFormat);
        Log(lpcsFormat, "TRACE", args);
        va_end(args);
    }
}

BOOL InitializeTimer()
{
    LARGE_INTEGER timerFrequency;

    if (!QueryPerformanceFrequency(&timerFrequency))
    {
        LogDebug("High precision timer not supported.");

        g_dTimerStart = round((DOUBLE)timeGetTime() * 1000.0);
    }
    else
    {
        LARGE_INTEGER timerCounter;

        g_bTimerHighResolution = TRUE;
        g_dTimerFrequency = (DOUBLE)timerFrequency.QuadPart / 1000000.0;
        QueryPerformanceCounter(&timerCounter);

        g_dTimerStart = (DOUBLE)timerCounter.QuadPart / g_dTimerFrequency;
    }

    return TRUE;
}

DOUBLE GetTimerCurrentTime()
{
    DOUBLE dNow = 0.0;

    if (!g_bTimerHighResolution)
    {
        timeBeginPeriod(1);
        dNow = round((DOUBLE)timeGetTime() * 1000.0);
        timeEndPeriod(1);
    }
    else
    {
        LARGE_INTEGER timerCounter;

        QueryPerformanceCounter(&timerCounter);
        dNow = (DOUBLE)timerCounter.QuadPart / g_dTimerFrequency;
    }

    // Overflow check.
    if (g_dTimerStart > dNow)
    {
        g_dTimerStart = dNow;
    }

    return round(dNow - g_dTimerStart);
}

LPDWORD SearchIat(LPCSTR functionName)
{
    LPVOID lpMapping = GetModuleHandle(NULL);

    if (lpMapping == NULL)
    {
        LogError("Cannot get current module handle.");
        return NULL;
    }

    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpMapping;

    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
    {
        LogError("Invalid PE DOS header.");
        return NULL;
    }

    PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)((LPBYTE)pDosHeader + pDosHeader->e_lfanew);

    if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE)
    {
        LogError("Invalid PE NT header.");
        return NULL;
    }

    PIMAGE_DATA_DIRECTORY pDataDirectory = &pNtHeaders->OptionalHeader.DataDirectory[1];
    PIMAGE_IMPORT_DESCRIPTOR pImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)((LPBYTE)pDosHeader + pDataDirectory->VirtualAddress);
    PIMAGE_THUNK_DATA32 pOriginalFirstThunk = (PIMAGE_THUNK_DATA32)((LPBYTE)pDosHeader + pImportDescriptor->OriginalFirstThunk);

    while (pOriginalFirstThunk != 0)
    {
        pOriginalFirstThunk = (PIMAGE_THUNK_DATA32)((LPBYTE)pDosHeader + pImportDescriptor->OriginalFirstThunk);

        PIMAGE_THUNK_DATA32 pFirstThunk = (PIMAGE_THUNK_DATA32)((LPBYTE)pDosHeader + pImportDescriptor->FirstThunk);

        while (pOriginalFirstThunk->u1.AddressOfData != 0)
        {
            PIMAGE_IMPORT_BY_NAME pImport = (PIMAGE_IMPORT_BY_NAME)((LPBYTE)pDosHeader + (DWORD)pOriginalFirstThunk->u1.AddressOfData);

            if (!((DWORD)pOriginalFirstThunk->u1.Function & (DWORD)IMAGE_ORDINAL_FLAG32))
            {
                if (strcmp(functionName, (LPCSTR)pImport->Name) == 0)
                {
                    return (LPDWORD) & (pFirstThunk->u1.Function);
                }
            }

            pOriginalFirstThunk++;
            pFirstThunk++;
        }
        pImportDescriptor++;
    }

    return NULL;
}

BOOL HookFunction(LPCSTR functionName, LPDWORD newFunction)
{
    LPDWORD pOldFunction = SearchIat(functionName);
    DWORD accessProtectionValue, dummyAccessProtectionValue;

    int vProtect = VirtualProtect(pOldFunction, sizeof(LPDWORD), PAGE_EXECUTE_READWRITE, &accessProtectionValue);

    if (!vProtect)
    {
        LogError("Failed to access virtual address space.");
        return FALSE;
    }

    *pOldFunction = (DWORD)newFunction;

    vProtect = VirtualProtect(pOldFunction, sizeof(LPDWORD), accessProtectionValue, &dummyAccessProtectionValue);

    if (!vProtect)
    {
        LogError("Failed to lock virtual address space.");
        return FALSE;
    }

    return TRUE;
}

BOOL PatchApplication(HANDLE hProcess)
{
    g_dMessageWaitTimeThreshold = GetMessageWaitTimeThresholdMs() * 1000.0;
    g_dMessageProcessingTimeThreshold = GetMessageProcessingTimeThresholdMs() * 1000.0;
    g_dwMessageWaitTimeMin = GetMessageWaitTimeMinMs();
    ;
    g_dwMessageWaitTimeMax = GetMessageWaitTimeMaxMs();
    g_dwMessageWaitTime = g_dwMessageWaitTimeMin;
    g_dwMessageWaitTimeInc = MAX(1, (g_dwMessageWaitTimeMax - g_dwMessageWaitTimeMin) / 10);

    return PatchCpuUsage(hProcess);
}

BOOL PatchCpuUsage(HANDLE hProcess)
{
    return HookFunction("PeekMessageA", (LPDWORD)&PeekMessageEx);
}

BOOL WINAPI PeekMessageEx(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
    DOUBLE dBeginTime = GetTimerCurrentTime();
    DWORD dwMsgWaitResult = MsgWaitForMultipleObjectsEx(0, 0, g_dwMessageWaitTime, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    DOUBLE dNow = GetTimerCurrentTime();

    if (dwMsgWaitResult == WAIT_TIMEOUT)
    {
        // Idle again, reset work timer.
        g_dBeginWorkTime = 0.0;

        if ((g_dBeginSleepTime <= 0.0) || (dNow < g_dBeginSleepTime))
        {
            g_dBeginSleepTime = dNow;
        }

        if ((dNow - g_dBeginSleepTime) >= g_dMessageWaitTimeThreshold)
        {
            g_dwMessageWaitTime = MIN(g_dwMessageWaitTimeMax, g_dwMessageWaitTime + g_dwMessageWaitTimeInc);
            g_dBeginSleepTime = 0.0;
        }
    }
    else
    {
        // Messages available, reset sleep timer.
        g_dBeginSleepTime = 0.0;

        if ((g_dBeginWorkTime <= 0.0) || (dNow < g_dBeginWorkTime))
        {
            g_dBeginWorkTime = dNow;
        }

        if ((dNow - g_dBeginWorkTime) >= g_dMessageProcessingTimeThreshold)
        {
            g_dwMessageWaitTime = 0;
        }
        else
        {
            g_dwMessageWaitTime = g_dwMessageWaitTimeMin;
        }
    }

    return PeekMessage(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpvReserved)
{
    HANDLE hProcess = NULL;
    BOOL bSuccess = FALSE;

    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        InitializeConfig();
        InitializeLog();
        InitializeTimer();

        hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());

        if (hProcess)
        {
            bSuccess = PatchApplication(hProcess);
            CloseHandle(hProcess);
        }
        else
        {
            LogError("Failed to open process.");
        }
        break;
    case DLL_PROCESS_DETACH:
        ShutdownLog();
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    }

    return bSuccess;
}
