#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <detours.h>
#include <string_view>

static constexpr DWORD MAX_SUPPORTED_CORES = 12;

static void(WINAPI* RealGetSystemInfo)(LPSYSTEM_INFO lpSystemInfo) = GetSystemInfo;
static BOOL(WINAPI* RealGetLogicalProcessorInformation)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION Buffer, PDWORD ReturnedLength) = GetLogicalProcessorInformation;
static FARPROC(WINAPI* RealGetProcAddress)(HMODULE hModule, LPCSTR lpProcName) = GetProcAddress;

void WINAPI GetSystemInfoDetour(LPSYSTEM_INFO info)
{
	RealGetSystemInfo(info);

	// Space Marine will crash if greater than 12 cores
	if (info->dwNumberOfProcessors > MAX_SUPPORTED_CORES)
		info->dwNumberOfProcessors = MAX_SUPPORTED_CORES;
}

BOOL WINAPI GetLogicalProcessorInformationDetour(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION Buffer, PDWORD ReturnedLength)
{
	const auto result = RealGetLogicalProcessorInformation(Buffer, ReturnedLength);
	if (result == TRUE)
	{
		DWORD entrySize = sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
		DWORD maxLength = entrySize * MAX_SUPPORTED_CORES;

		if (*ReturnedLength > maxLength)
			*ReturnedLength = maxLength;
	}
	return result;
}

FARPROC WINAPI GetProcAddressDetour(HMODULE hModule, LPCSTR lpProcName)
{
	using namespace std::literals;

	// lpProcName can be an ordinal (HIWORD == 0), skip string comparison in that case
	if (reinterpret_cast<ULONG_PTR>(lpProcName) > 0xFFFF)
	{
		if ("GetSystemInfo"sv == lpProcName)
			return reinterpret_cast<FARPROC>(&GetSystemInfoDetour);
		else if ("GetLogicalProcessorInformation"sv == lpProcName)
			return reinterpret_cast<FARPROC>(&GetLogicalProcessorInformationDetour);
	}

	return RealGetProcAddress(hModule, lpProcName);
}

// Build the path to the original DINPUT8.dll based on the Windows directory
static BOOL GetOriginalDllPath(TCHAR* path, DWORD pathSize)
{
	UINT len = GetSystemDirectory(path, pathSize);
	if (len == 0 || len + 13 >= pathSize) // 13 = length of "\\DINPUT8.dll"
		return FALSE;
	lstrcat(path, TEXT("\\DINPUT8.dll"));
	return TRUE;
}

#include "Unknwn.h"

using DirectInput8CreateFunctionType = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

static HMODULE moduleHandle = NULL;

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE hinst,
	DWORD dwVersion,
	REFIID riidltf,
	LPVOID* ppvOut,
	LPUNKNOWN punkOuter)
{
	if (!moduleHandle)
	{
		TCHAR dllPath[MAX_PATH];
		if (GetOriginalDllPath(dllPath, MAX_PATH))
		{
			HMODULE h = LoadLibrary(dllPath);
			if (h)
			{
				// If another thread loaded first, free our duplicate handle
				HMODULE expected = NULL;
				if (InterlockedCompareExchangePointer(
						reinterpret_cast<PVOID*>(&moduleHandle), h, expected) != expected)
					FreeLibrary(h);
			}
		}
	}

	if (!moduleHandle)
	{
		SetLastError(ERROR_DLL_NOT_FOUND);
		return E_FAIL;
	}

	const auto directInput8Create = reinterpret_cast<DirectInput8CreateFunctionType>(
		RealGetProcAddress(moduleHandle, "DirectInput8Create"));

	if (!directInput8Create)
	{
		SetLastError(ERROR_PROC_NOT_FOUND);
		return E_FAIL;
	}

	return directInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
}

BOOL Init(HINSTANCE hModule)
{
	DisableThreadLibraryCalls(hModule);

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)RealGetSystemInfo, GetSystemInfoDetour);
	DetourAttach(&(PVOID&)RealGetLogicalProcessorInformation, GetLogicalProcessorInformationDetour);
	DetourAttach(&(PVOID&)RealGetProcAddress, GetProcAddressDetour);
	return DetourTransactionCommit() == NO_ERROR;
}

void Cleanup()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(PVOID&)RealGetSystemInfo, GetSystemInfoDetour);
	DetourDetach(&(PVOID&)RealGetLogicalProcessorInformation, GetLogicalProcessorInformationDetour);
	DetourDetach(&(PVOID&)RealGetProcAddress, GetProcAddressDetour);
	DetourTransactionCommit();

	if (moduleHandle)
	{
		FreeLibrary(moduleHandle);
		moduleHandle = NULL;
	}
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD ul_reason_for_call,
	LPVOID)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		return Init(hModule);
	case DLL_PROCESS_DETACH:
		Cleanup();
		break;
	}
	return TRUE;
}
