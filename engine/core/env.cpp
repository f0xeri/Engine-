#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS // the MSVC CRT deprecates std::getenv
#endif

#include "engine/core/env.hpp"

#include <cstdlib>
#include <cstring>

namespace Core
{

std::optional<bool> envFlag(const char* name)
{
    const char* value = std::getenv(name);
    if (!value || value[0] == '\0')
    {
        return std::nullopt;
    }

    return _stricmp(value, "1") == 0 || _stricmp(value, "true") == 0 ||
           _stricmp(value, "on") == 0 || _stricmp(value, "yes") == 0;
}

} // namespace Core
