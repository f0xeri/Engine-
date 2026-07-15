#pragma once

#include "engine/Core/FileWatcher/FileWatcher.hpp"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Shader
{

struct SpirvBlobs
{
    std::vector<uint32_t> vertex;
    std::vector<uint32_t> fragment;
};

// called on the worker thread, for the initial compile and every successful recompile
using CompiledHandler = std::function<void(const SpirvBlobs& spirv)>;

class ShaderSystem
{
public:
    explicit ShaderSystem(std::filesystem::path shaderRoot);
    ~ShaderSystem();

    ShaderSystem(const ShaderSystem&) = delete;
    ShaderSystem& operator=(const ShaderSystem&) = delete;

    // async; initial compile failure aborts, recompile failure keeps the old artifact
    void load(std::string module, CompiledHandler onCompiled, CompiledHandler onRecompiled);

    // forwards watcher events to the worker; call once per frame
    void pump();

private:
    struct LoadRequest
    {
        std::string module;
        CompiledHandler onCompiled;
        CompiledHandler onRecompiled;
    };

    void workerLoop();

    std::filesystem::path _root;
    std::unique_ptr<Core::FileWatcher> _watcher;

    std::mutex _mutex;
    std::condition_variable _wake;
    std::deque<LoadRequest> _loads;
    bool _reloadRequested = false;
    bool _shutdown = false;

    std::thread _worker;
};

} // namespace Shader
