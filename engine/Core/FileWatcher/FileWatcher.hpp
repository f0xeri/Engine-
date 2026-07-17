#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace Core
{

enum class AssetType : std::uint8_t
{
    Shader
};

struct FileEvent
{
    std::filesystem::path path;
    AssetType assetType;
};

class FileWatcher
{
public:
    virtual ~FileWatcher() = default;

    virtual void watch(AssetType assetType, const std::filesystem::path& directory) = 0;
    virtual std::vector<FileEvent> takeEvents() = 0;

    static std::unique_ptr<FileWatcher> create();
};

} // namespace Core
