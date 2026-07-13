#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS // suppress warning about std::getenv
#endif

#include "engine/core/log.hpp"

#include <chrono>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#include <cstdio>

namespace Core
{

namespace
{

bool enableColor(FILE* stream)
{
    const char* forceColor = std::getenv("CLICOLOR_FORCE");

    if (forceColor && forceColor[0] == '1')
    {
#ifdef _WIN32
        HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stream)));

        DWORD mode = 0;
        if (GetConsoleMode(handle, &mode))
        {
            SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
#endif
        return true;
    }

#ifdef _WIN32
    if (!_isatty(_fileno(stream)))
    {
        return false;
    }

    HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stream)));
    DWORD mode = 0;

    if (!GetConsoleMode(handle, &mode))
    {
        return false;
    }

    return SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
#else
    return isatty(fileno(stream)) != 0;
#endif
}

constexpr const char* kReset = "\x1b[0m";
constexpr const char* kDim = "\x1b[90m";
constexpr const char* kCyan = "\x1b[36m";

} // namespace

void logMessage(LogLevel level, std::string_view module, std::string_view message)
{
    static const bool stdoutColor = enableColor(stdout);
    static const bool stderrColor = enableColor(stderr);

    const char* tag = "INFO ";
    const char* tagColor = "\x1b[32m"; // green
    switch (level)
    {
        case LogLevel::Info:
            tag = "INFO ";
            tagColor = "\x1b[32m";
            break;
        case LogLevel::Warn:
            tag = "WARN ";
            tagColor = "\x1b[33m";
            break;
        case LogLevel::Error:
            tag = "ERROR";
            tagColor = "\x1b[31m";
            break;
    }

    FILE* out = level == LogLevel::Error ? stderr : stdout;
    const bool color = level == LogLevel::Error ? stderrColor : stdoutColor;

    const auto now = std::chrono::zoned_time(
        std::chrono::current_zone(),
        std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now()));

    std::string line;
    if (color)
    {
        line = std::format("{}[{:%H:%M:%S}]{} {}[{}]{} {}[{}]{} {}\n",
                           kDim,
                           now,
                           kReset,
                           tagColor,
                           tag,
                           kReset,
                           kCyan,
                           module,
                           kReset,
                           message);
    }
    else
    {
        line = std::format("[{:%H:%M:%S}] [{}] [{}] {}\n", now, tag, module, message);
    }

    std::fputs(line.c_str(), out);
    std::fflush(out);
}

} // namespace Core
