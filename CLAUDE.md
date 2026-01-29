# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

CPUCoreCountFix-r is a Windows DLL that hooks CPU core detection APIs to cap reported logical core count at 12, fixing legacy games that crash on systems with 32+ logical cores. It masquerades as `dinput8.dll` (DirectInput 8 proxy) for drop-in deployment alongside game executables.

Fork of [alexstrout/CPUCoreCountFix](https://github.com/alexstrout/CPUCoreCountFix) with bug fixes and proper cleanup.

## Build

- **Toolchain:** Visual Studio 2022 (v143 toolset), C++20
- **Solution:** `SpaceMarineCoreFix.sln`
- **Platforms:** Win32 (x86) and x64
- **External dependency:** Microsoft Detours as git submodule at `External/Detours`

Build via VS2022 or MSBuild:
```
msbuild SpaceMarineCoreFix.sln /p:Configuration=Release /p:Platform=x64
msbuild SpaceMarineCoreFix.sln /p:Configuration=Release /p:Platform=Win32
```

After cloning, init submodules: `git submodule update --init --recursive`

Output: `DINPUT8.dll` (both configurations produce this name).

## Architecture

All logic lives in a single file: `SpaceMarineCoreFix/dllmain.cpp` (~154 lines).

**Three API detours using Microsoft Detours:**
1. `GetSystemInfoDetour` — caps `dwNumberOfProcessors` to `MAX_SUPPORTED_CORES` (12)
2. `GetLogicalProcessorInformationDetour` — truncates returned buffer to 12 cores worth of entries
3. `GetProcAddressDetour` — intercepts dynamic lookups for the above two APIs, returning detoured versions

**DirectInput8 proxy:** `DirectInput8Create` export loads the real `dinput8.dll` from System32/SysWOW64 and forwards the call. Exported via `module.def`.

**Lifecycle:** `Init()` on `DLL_PROCESS_ATTACH` attaches all detours; `Cleanup()` on `DLL_PROCESS_DETACH` detaches hooks and frees the original DLL handle.
