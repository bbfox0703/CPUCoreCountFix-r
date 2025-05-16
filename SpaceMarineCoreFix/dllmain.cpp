#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
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

	// Check if one of our detoured functions is being requested dynamically
	if ("GetSystemInfo"sv == lpProcName)
		return reinterpret_cast<FARPROC>(&GetSystemInfoDetour);
	else if ("GetLogicalProcessorInformation"sv == lpProcName)
		return reinterpret_cast<FARPROC>(&GetLogicalProcessorInformationDetour);
	else
		return RealGetProcAddress(hModule, lpProcName);
}

// Export function to impersonate DirectInput8.dll
#ifdef _WIN64
#define ORIGINAL_DLL_PATH_64BIT_SYSTEM "C:\\Windows\\System32\\DINPUT8.dll"
#else
#define ORIGINAL_DLL_PATH_64BIT_SYSTEM "C:\\Windows\\SysWOW64\\DINPUT8.dll"
#define ORIGINAL_DLL_PATH_32BIT_SYSTEM "C:\\Windows\\System32\\DINPUT8.dll"

static BOOL Is64BitOS()
{
	using IsWow64ProcessFunctionType = BOOL(WINAPI*)(HANDLE, PBOOL);
	const auto isWow64Process = reinterpret_cast<IsWow64ProcessFunctionType>(
		RealGetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process"));

	BOOL is64Bit = FALSE;
	if (isWow64Process != NULL)
		isWow64Process(GetCurrentProcess(), &is64Bit);

	return is64Bit;
}

static const BOOL IS_64BIT_OS = Is64BitOS();
#endif

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
		moduleHandle = LoadLibrary(
#ifdef _WIN64
			TEXT(ORIGINAL_DLL_PATH_64BIT_SYSTEM)
#else
			IS_64BIT_OS ? TEXT(ORIGINAL_DLL_PATH_64BIT_SYSTEM) : TEXT(ORIGINAL_DLL_PATH_32BIT_SYSTEM)
#endif
		);
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
