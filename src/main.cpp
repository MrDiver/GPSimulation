#include <spdlog/spdlog.h>
#include "vk_engine.h"

int main(){
    spdlog::info("Starting Engine");
    VulkanEngine engine;
    engine.init();
    engine.run();
    engine.cleanup();
    return 0;
}