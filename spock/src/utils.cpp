// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/utils.hpp"

#include <cstdint>
#include <cstdio>

// Platform detection and headers
#if defined(_WIN32) || defined(_WIN64)
#define PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#define PLATFORM_MACOS
#include <mach-o/dyld.h>
#include <os/log.h>
#else
#define PLATFORM_LINUX
#endif

namespace spock
{
    void writeLog(const std::string& message)
    {
#if defined(PLATFORM_WINDOWS)
        // Sends output directly to Visual Studio Debugger Output Window
        if (IsDebuggerPresent())
        {
            OutputDebugStringA(message.c_str());
        }
        else
        {
            std::fputs(message.c_str(), stderr);
            std::fflush(stderr);
        }
#elif defined(PLATFORM_MACOS)
        // Sends output to OS Log (viewable in Console.app)
        os_log_error(OS_LOG_DEFAULT, "%{public}s", message.c_str());
#else
        // Writes to stderr on Linux / Unix systems
        std::fputs(message.c_str(), stderr);
        std::fflush(stderr);
#endif
    }

    std::filesystem::path executableDirectory()
    {
#if defined(PLATFORM_WINDOWS)
        std::wstring path(MAX_PATH, L'\0');
        DWORD length = 0;
        // A full buffer means the path was truncated.
        while ((length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()))) == path.size())
        {
            path.resize(path.size() * 2);
        }
        if (length == 0)
        {
            throw std::runtime_error("executableDirectory: GetModuleFileNameW failed");
        }
        path.resize(length);
        return std::filesystem::canonical(path).parent_path();
#elif defined(PLATFORM_MACOS)
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        std::string path(size, '\0');
        if (_NSGetExecutablePath(path.data(), &size) != 0)
        {
            throw std::runtime_error("executableDirectory: _NSGetExecutablePath failed");
        }
        return std::filesystem::canonical(path.c_str()).parent_path();
#else
        return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
    }
} // namespace spock
