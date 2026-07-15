#include "engine/Core/FileWatcher/FileWatcher.hpp"

#ifdef _WIN32
#include "engine/Core/FileWatcher/FileWatcherWindows.hpp"
#endif

namespace Core
{

std::unique_ptr<FileWatcher> FileWatcher::create()
{
#ifdef _WIN32
    return std::make_unique<FileWatcherWindows>();
#else
#error "no FileWatcher implementation for this platform"
#endif
}

} // namespace Core
