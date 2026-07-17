#pragma once

#include <optional>

namespace Core
{
// Reads a boolean environment flag.
// None: variable is unset
// True: "1", "true", "on", "yes" (case-insensitive)
// False: anything else
std::optional<bool> envFlag(const char* name);

} // namespace Core
