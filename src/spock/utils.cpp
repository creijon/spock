// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "utils.hpp"

// Platform detection and headers
#if defined(_WIN32) || defined(_WIN64)
#define PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#define PLATFORM_MACOS
#include <os/log.h>
#else
#define PLATFORM_LINUX
#include <cstdio>
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
} // namespace spock
