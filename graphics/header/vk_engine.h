#pragma once

#include "vk_types.h"

class VulkanEngine {
public:
  VkInstance _instance;                      // Vulkan library handle
  VkDebugUtilsMessengerEXT _debug_messenger; // Vulkan debug output handle
  VkPhysicalDevice _chosenGPU;               // GPU chosen as the default device
  VkDevice _device;                          // Vulkan device for commands
  VkSurfaceKHR _surface;                     // Vulkan window surface

  VkSwapchainKHR _swapchain;
  VkFormat _swapchainImageFormat;

  std::vector<VkImage> _swapchainImages;
  std::vector<VkImageView> _swapchainImageViews;
  VkExtent2D _swapchainExtent;

  bool _isInitialized{false};
  int _frameNumber{0};
  bool stop_rendering{false};
  VkExtent2D _windowExtent{1700, 900};

  struct SDL_Window *_window{nullptr};

  static VulkanEngine &Get();

  void init();
  void cleanup();
  void draw();
  void run();

private:
  void init_vulkan();
  void init_swapchain();
  void init_commands();
  void init_sync_structures();

  void create_swapchain(uint32_t width, uint32_t height);
  void destroy_swapchain();
};