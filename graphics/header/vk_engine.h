#pragma once

#include "vk_types.h"

constexpr unsigned int FRAME_OVERLAP = 2;

struct AllocatedImage {
	VkImage image;
	VkImageView imageView;
	VmaAllocation allocation;
	VkExtent3D imageExtent;
	VkFormat imageFormat;
};

struct DeletionQueue {
	std::deque<std::function<void()>> _deletionQueue;

	void add(std::function<void()> function);
	void flush(VkDevice device);
};

struct FrameData {
	VkCommandPool _pool;
	VkCommandBuffer _buffer;
	VkFence _renderFence;
	VkSemaphore _swapchainSemaphore, _renderSemaphore;
	DeletionQueue _deletionQueue;
};

class VulkanEngine {
public:
	VmaAllocator _allocator; // Vulkan Memory Allocator

	//draw resources
	AllocatedImage _drawImage;
	VkExtent2D _drawExtent;

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

  // Frame Related Things
  FrameData _frames[FRAME_OVERLAP];
  FrameData& get_current_frame();
  VkQueue _graphicsQueue;
  uint32_t _graphicsQueueFamily;

  bool _isInitialized{false};
  int _frameNumber{0};
  bool stop_rendering{false};
  VkExtent2D _windowExtent{1700, 900};

  struct SDL_Window *_window{nullptr};

  static VulkanEngine &Get();

  void init();
  void cleanup();
  void draw();
  void draw_background(VkCommandBuffer cmd);
  void run();

private:
	DeletionQueue _mainDeletionQueue;
  void init_vulkan();
  void init_swapchain();
  void init_commands();
  void init_sync_structures();

  void create_swapchain(uint32_t width, uint32_t height);
  void destroy_swapchain();
};