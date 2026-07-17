#pragma once

#ifdef _WIN32

#include "engine/Core/FileWatcher/FileWatcher.hpp"

#include <mutex>
#include <thread>

namespace Core
{

// One thread per watched directory root, parked in a blocking ReadDirectoryChangesW.
class FileWatcherWindows final : public FileWatcher
{
public:
    FileWatcherWindows();
    ~FileWatcherWindows() override;

    void watch(AssetType assetType, const std::filesystem::path& directory) override;
    std::vector<FileEvent> takeEvents() override;

private:
    void watchThread(std::filesystem::path root, AssetType assetType);
    void enqueue(FileEvent event);

    void* _stopEvent = nullptr;
    std::vector<std::thread> _threads;

    std::mutex _mutex;
    std::vector<FileEvent> _events;
};

} // namespace Core

#endif
