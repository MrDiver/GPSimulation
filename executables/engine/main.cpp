#include "vk_engine.h"
#include <spdlog/spdlog.h>

int main() {
  spdlog::info("Starting Engine");
  VulkanEngine engine;
  engine.init();
  engine.run();
  engine.cleanup();
  return 0;
}