#pragma once

#include <format>
#include <string_view>

namespace core {

enum class LogLevel { Info, Warn, Error };

// Single source of truth for log modules: the enum and its display name are
// both generated from this list. Mirrors the architecture layers.
#define ENGINE_LOG_MODULE_LIST(X) \
    X(Core,     "Core")           \
    X(Platform, "Platform")       \
    X(Vulkan,   "Vulkan")         \
    X(Shader,   "Shader")         \
    X(Graph,    "Graph")          \
    X(Renderer, "Renderer")       \
    X(Scene,    "Scene")          \
    X(Asset,    "Asset")          \
    X(App,      "App")

enum class LogModule {
#define X(name, str) name,
    ENGINE_LOG_MODULE_LIST(X)
#undef X
};

constexpr std::string_view logModuleName(LogModule module) {
    switch (module) {
#define X(name, str) case LogModule::name: return str;
        ENGINE_LOG_MODULE_LIST(X)
#undef X
    }
    return "?";
}

// Free-form module tag for code outside the engine (sandbox, tools, demos).
void logMessage(LogLevel level, std::string_view module, std::string_view message);

inline void logMessage(LogLevel level, LogModule module, std::string_view message) {
    logMessage(level, logModuleName(module), message);
}

template <typename... Args>
void logInfo(LogModule module, std::format_string<Args...> fmt, Args&&... args) {
    logMessage(LogLevel::Info, module, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void logWarn(LogModule module, std::format_string<Args...> fmt, Args&&... args) {
    logMessage(LogLevel::Warn, module, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void logError(LogModule module, std::format_string<Args...> fmt, Args&&... args) {
    logMessage(LogLevel::Error, module, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void logInfo(std::string_view module, std::format_string<Args...> fmt, Args&&... args) {
    logMessage(LogLevel::Info, module, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void logWarn(std::string_view module, std::format_string<Args...> fmt, Args&&... args) {
    logMessage(LogLevel::Warn, module, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void logError(std::string_view module, std::format_string<Args...> fmt, Args&&... args) {
    logMessage(LogLevel::Error, module, std::format(fmt, std::forward<Args>(args)...));
}

} // namespace core

// Short-form logging. `using enum` lets the module be either a bare enumerator
// or a free-form string with the same syntax:
//   LOG_INFO(Vulkan, "instance API {}.{}.{}", major, minor, patch);
//   LOG_ERROR("sandbox", "SDL_Init failed: {}", SDL_GetError());
#define LOG_INFO(module, ...) \
    do { using enum ::core::LogModule; ::core::logInfo(module, __VA_ARGS__); } while (0)
#define LOG_WARN(module, ...) \
    do { using enum ::core::LogModule; ::core::logWarn(module, __VA_ARGS__); } while (0)
#define LOG_ERROR(module, ...) \
    do { using enum ::core::LogModule; ::core::logError(module, __VA_ARGS__); } while (0)
