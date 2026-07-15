#include "engine/Shader/ShaderSystem.hpp"

#include "engine/Core/Log.hpp"

#include <slang.h>
#include <slang-com-ptr.h>

#include <cstdlib>
#include <optional>
#include <utility>

namespace Shader
{

namespace
{

std::vector<uint32_t> copyBlob(slang::IBlob* blob)
{
    const auto* words = static_cast<const uint32_t*>(blob->getBufferPointer());
    return {words, words + (blob->getBufferSize() / sizeof(uint32_t))};
}

void logDiagnostics(slang::IBlob* diagnostics)
{
    if (diagnostics && diagnostics->getBufferSize() > 0)
    {
        LOG_WARN(Shader, "{}", static_cast<const char*>(diagnostics->getBufferPointer()));
    }
}

// fresh session per compile
std::optional<SpirvBlobs> compile(slang::IGlobalSession* globalSession,
                                  const std::filesystem::path& root,
                                  const std::string& module)
{
    slang::TargetDesc target;
    target.format = SLANG_SPIRV;
    target.profile = globalSession->findProfile("spirv_1_5");
    // make shader structs stay byte-identical to their packed C++ mirrors
    target.forceGLSLScalarBufferLayout = true;

    const std::string rootStr = root.string();
    const char* searchPath = rootStr.c_str();

    slang::SessionDesc desc;
    desc.targets = &target;
    desc.targetCount = 1;
    desc.searchPaths = &searchPath;
    desc.searchPathCount = 1;

    Slang::ComPtr<slang::ISession> session;
    if (SLANG_FAILED(globalSession->createSession(desc, session.writeRef())))
    {
        LOG_ERROR(Shader, "createSession failed");
        return std::nullopt;
    }

    Slang::ComPtr<slang::IBlob> diagnostics;
    slang::IModule* slangModule = session->loadModule(module.c_str(), diagnostics.writeRef());
    logDiagnostics(diagnostics);
    if (!slangModule)
    {
        return std::nullopt;
    }

    Slang::ComPtr<slang::IEntryPoint> vsEntry;
    Slang::ComPtr<slang::IEntryPoint> fsEntry;
    if (SLANG_FAILED(slangModule->findEntryPointByName("vsMain", vsEntry.writeRef())) ||
        SLANG_FAILED(slangModule->findEntryPointByName("fsMain", fsEntry.writeRef())))
    {
        LOG_ERROR(Shader, "{}: missing vsMain/fsMain entry point", module);
        return std::nullopt;
    }

    slang::IComponentType* components[] = {slangModule, vsEntry, fsEntry};
    Slang::ComPtr<slang::IComponentType> composed;
    if (SLANG_FAILED(session->createCompositeComponentType(
            components, 3, composed.writeRef(), diagnostics.writeRef())))
    {
        logDiagnostics(diagnostics);
        return std::nullopt;
    }

    Slang::ComPtr<slang::IComponentType> linked;
    if (SLANG_FAILED(composed->link(linked.writeRef(), diagnostics.writeRef())))
    {
        logDiagnostics(diagnostics);
        return std::nullopt;
    }

    // Entry point indices follow the order they were added to the composite.
    Slang::ComPtr<slang::IBlob> vsCode;
    Slang::ComPtr<slang::IBlob> fsCode;
    if (SLANG_FAILED(linked->getEntryPointCode(0, 0, vsCode.writeRef(), diagnostics.writeRef())))
    {
        logDiagnostics(diagnostics);
        return std::nullopt;
    }
    if (SLANG_FAILED(linked->getEntryPointCode(1, 0, fsCode.writeRef(), diagnostics.writeRef())))
    {
        logDiagnostics(diagnostics);
        return std::nullopt;
    }

    return SpirvBlobs{.vertex = copyBlob(vsCode), .fragment = copyBlob(fsCode)};
}

} // namespace

ShaderSystem::ShaderSystem(std::filesystem::path shaderRoot)
    : _root(std::move(shaderRoot))
{
    _watcher = Core::FileWatcher::create();
    _watcher->watch(Core::AssetType::Shader, _root);
    _worker = std::thread(&ShaderSystem::workerLoop, this);
}

ShaderSystem::~ShaderSystem()
{
    {
        std::lock_guard lock(_mutex);
        _shutdown = true;
    }
    _wake.notify_one();
    _worker.join();
}

void ShaderSystem::load(std::string module,
                        CompiledHandler onCompiled,
                        CompiledHandler onRecompiled)
{
    {
        std::lock_guard lock(_mutex);
        _loads.push_back({std::move(module), std::move(onCompiled), std::move(onRecompiled)});
    }
    _wake.notify_one();
}

void ShaderSystem::pump()
{
    bool reload = false;
    for (const Core::FileEvent& event : _watcher->takeEvents())
    {
        if (event.path.extension() == ".slang")
        {
            LOG_INFO(Shader, "changed: {}", event.path.filename().string());
            reload = true;
        }
    }

    if (reload)
    {
        {
            std::lock_guard lock(_mutex);
            _reloadRequested = true;
        }
        _wake.notify_one();
    }
}

void ShaderSystem::workerLoop()
{
    Slang::ComPtr<slang::IGlobalSession> globalSession;
    if (SLANG_FAILED(slang::createGlobalSession(globalSession.writeRef())))
    {
        LOG_ERROR(Shader, "cannot create Slang global session");
        std::abort();
    }

    std::vector<LoadRequest> registry;

    for (;;)
    {
        std::unique_lock lock(_mutex);
        _wake.wait(lock, [&] { return _shutdown || !_loads.empty() || _reloadRequested; });

        if (_shutdown)
        {
            return;
        }

        // load path
        if (!_loads.empty())
        {
            LoadRequest request = std::move(_loads.front());
            _loads.pop_front();
            lock.unlock();

            const auto spirv = compile(globalSession, _root, request.module);
            if (!spirv)
            {
                LOG_ERROR(Shader, "initial compile of {} failed", request.module);
                std::abort();
            }

            request.onCompiled(*spirv);
            LOG_INFO(Shader, "compiled {}", request.module);
            registry.emplace_back(std::move(request));
            continue;
        }

        // hot reload path
        _reloadRequested = false;
        lock.unlock();

        // rebuild ALL. might be slow, consider checking dependencies of updated shaders
        for (const auto& [module, _, onRecompiled] : registry)
        {
            const auto spirv = compile(globalSession, _root, module);
            if (!spirv)
            {
                continue; // error already logged, old artifact alive
            }

            onRecompiled(*spirv);
            LOG_INFO(Shader, "recompiled {}", module);
        }
    }
}

} // namespace Shader
