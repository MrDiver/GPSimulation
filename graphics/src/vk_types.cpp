#include "vk_types.h"


VkResult vk_check(
        VkResult result,
        std::source_location loc)
{
    if (result != VK_SUCCESS)                // compile-time evaluable branch
    {
        // This I/O/abort path only runs at **run-time**
        spdlog::error(
                   "Vulkan error {} ({}:{})",
                   string_VkResult(result),  // maps enum → text
                   loc.file_name(),
                   loc.line());              // C++20 call-site info
        std::abort();                        // bail out
    }
    return result;                           // let you chain calls
}