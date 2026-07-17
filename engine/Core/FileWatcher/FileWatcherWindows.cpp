#ifdef _WIN32

#include "engine/Core/FileWatcher/FileWatcherWindows.hpp"

#include "engine/Core/Log.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <string_view>
#include <utility>

namespace Core
{

FileWatcherWindows::FileWatcherWindows()
{
    _stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (_stopEvent == nullptr)
    {
        LOG_ERROR(Core, "CreateEvent failed: {}", GetLastError());
        std::abort();
    }
}

FileWatcherWindows::~FileWatcherWindows()
{
    SetEvent(_stopEvent);
    for (std::thread& thread : _threads)
    {
        thread.join();
    }
    CloseHandle(_stopEvent);
}

void FileWatcherWindows::watch(AssetType assetType, const std::filesystem::path& directory)
{
    _threads.emplace_back(&FileWatcherWindows::watchThread, this, directory, assetType);
}

std::vector<FileEvent> FileWatcherWindows::takeEvents()
{
    std::lock_guard lock(_mutex);
    return std::exchange(_events, {});
}

void FileWatcherWindows::enqueue(FileEvent event)
{
    std::lock_guard lock(_mutex);

    const bool pending =
        std::ranges::any_of(_events, [&](const FileEvent& e) { return e.path == event.path; });
    if (!pending)
    {
        _events.push_back(std::move(event));
    }
}

void FileWatcherWindows::watchThread(std::filesystem::path root, AssetType assetType)
{
    const HANDLE dir = CreateFileW(root.c_str(),
                                   FILE_LIST_DIRECTORY,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   nullptr,
                                   OPEN_EXISTING,
                                   FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                   nullptr);
    if (dir == INVALID_HANDLE_VALUE)
    {
        LOG_ERROR(Core, "cannot watch {}: error {}", root.string(), GetLastError());
        std::abort();
    }

    const HANDLE ioEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (ioEvent == nullptr)
    {
        LOG_ERROR(Core, "CreateEvent failed: {}", GetLastError());
        std::abort();
    }

    alignas(DWORD) std::byte buffer[32 * 1024];

    for (;;)
    {
        OVERLAPPED overlapped{};
        overlapped.hEvent = ioEvent;

        if (ReadDirectoryChangesW(dir,
                                  buffer,
                                  sizeof(buffer),
                                  TRUE, // recursive
                                  FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
                                  nullptr,
                                  &overlapped,
                                  nullptr) == 0)
        {
            LOG_ERROR(
                Core, "ReadDirectoryChangesW failed on {}: {}", root.string(), GetLastError());
            break;
        }

        const HANDLE handles[] = {_stopEvent, ioEvent};
        if (WaitForMultipleObjects(2, handles, FALSE, INFINITE) == WAIT_OBJECT_0)
        {
            CancelIoEx(dir, &overlapped);
            break;
        }

        DWORD bytes = 0;
        if (GetOverlappedResult(dir, &overlapped, &bytes, TRUE) == 0)
        {
            continue;
        }
        if (bytes == 0)
        {
            LOG_WARN(Core, "file notification overflow on {}", root.string());
            continue;
        }

        const std::byte* cursor = buffer;
        for (;;)
        {
            const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(cursor);

            if (info->Action == FILE_ACTION_MODIFIED || info->Action == FILE_ACTION_ADDED ||
                info->Action == FILE_ACTION_RENAMED_NEW_NAME)
            {
                const std::wstring_view name(info->FileName, info->FileNameLength / sizeof(WCHAR));
                enqueue({.path = root / name, .assetType = assetType});
            }

            if (info->NextEntryOffset == 0)
            {
                break;
            }
            cursor += info->NextEntryOffset;
        }
    }

    CloseHandle(ioEvent);
    CloseHandle(dir);
}

} // namespace Core

#endif
