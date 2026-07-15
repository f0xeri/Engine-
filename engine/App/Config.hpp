#pragma once

#include <filesystem>

namespace App
{
struct Config
{
    const char* title;
    std::filesystem::path shaderRoot;
    std::filesystem::path pipelineCache;
};
} // namespace App