// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#if defined(_MSC_VER)
#pragma warning(disable : 4201) // disable warning C4201: nonstandard extension used: nameless struct/union; needed
                                // to get glm/detail/type_vec?.hpp without warnings
#elif defined(__GNUC__)
// don't know how to switch off that warning here
#else
// unknow compiler... just ignore the warnings for yourselves ;)
#endif

#include <vulkan/vulkan.hpp>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4127) // conditional expression is constant (glm)
#endif

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace spock
{
} // namespace spock
