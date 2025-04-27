#include "vk_engine.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_vulkan.h>

#include <VkBootstrap.h>

#include <vk_initializers.h>
#include <vk_types.h>

#include <chrono>
#include <thread>
#include <vulkan/vulkan_core.h>

constexpr bool bUseValidationLayers = true;

VulkanEngine *loadedEngine = nullptr;

VulkanEngine &VulkanEngine::Get() { return *loadedEngine; }

void VulkanEngine::init() {
  // only one engine initialization is allowed with the application.
  assert(loadedEngine == nullptr);
  loadedEngine = this;

  // We initialize SDL and create a window with it.
  SDL_Init(SDL_INIT_VIDEO);

  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

  _window = SDL_CreateWindow("Vulkan Engine", _windowExtent.width,
                             _windowExtent.height, window_flags);

  init_vulkan();
  init_swapchain();
  init_commands();
  init_sync_structures();

  // everything went fine
  _isInitialized = true;
}

void VulkanEngine::cleanup() {
  if (_isInitialized) {
    destroy_swapchain();

    vkDestroySurfaceKHR(this->_instance, this->_surface, nullptr);
    vkDestroyDevice(this->_device, nullptr);

    vkb::destroy_debug_utils_messenger(this->_instance, this->_debug_messenger);
    vkDestroyInstance(this->_instance, nullptr);

    SDL_DestroyWindow(_window);
  }

  // clear engine pointer
  loadedEngine = nullptr;
}

void VulkanEngine::draw() {
  // nothing yet
}

void VulkanEngine::run() {
  SDL_Event e;
  bool bQuit = false;

  // main loop
  while (!bQuit) {
    // Handle events on queue
    while (SDL_PollEvent(&e) != 0) {
      // close the window when user alt-f4s or clicks the X button
      if (e.type == SDL_EVENT_QUIT)

        switch (e.type) {
        case SDL_EVENT_QUIT:
          bQuit = true;
          break;
        case SDL_EVENT_WINDOW_MINIMIZED:
          stop_rendering = true;
          break;
        case SDL_EVENT_WINDOW_RESTORED:
          stop_rendering = false;
          break;
        }
    }

    // do not draw if we are minimized
    if (stop_rendering) {
      // throttle the speed to avoid the endless spinning
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    draw();
  }
}

void VulkanEngine::init_vulkan() {
  // Init Instance
  vkb::InstanceBuilder builder;

  auto inst_ret =
      builder.set_app_name("Simulation Studio")
          .request_validation_layers()
          .set_debug_callback(
              [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                 VkDebugUtilsMessageTypeFlagsEXT messageType,
                 const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                 void *) -> VkBool32 {
                if (messageSeverity &
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                  const char *severity =
                      vkb::to_string_message_severity(messageSeverity);
                  const char *type = vkb::to_string_message_type(messageType);
                  spdlog::error("[{}: {}] {}", severity, type,
                                pCallbackData->pMessage);
                }
                // Return false to move on, but return true for validation to
                // skip passing down the call to the driver
                return VK_TRUE;
              })
          .require_api_version(1, 3, 0)
          .build();
  vkb::Instance vkb_inst = inst_ret.value();

  this->_instance = vkb_inst.instance;
  this->_debug_messenger = vkb_inst.debug_messenger;

  // Init Surface
  SDL_Vulkan_CreateSurface(this->_window, this->_instance, nullptr,
                           &this->_surface);

  // Init Device
  VkPhysicalDeviceVulkan13Features features{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  features.dynamicRendering = true;
  features.synchronization2 = true;

  VkPhysicalDeviceVulkan12Features features12{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  features12.descriptorIndexing = true;
  features12.bufferDeviceAddress = true;

  vkb::PhysicalDeviceSelector selector{vkb_inst};
  vkb::PhysicalDevice physical_device =
      selector.set_minimum_version(1, 3)
          .set_required_features_13(features)
          .set_required_features_12(features12)
          .set_surface(this->_surface)
          .select()
          .value();

  // Create Logical Device
  vkb::DeviceBuilder device_builder{physical_device};
  vkb::Device vkb_device = device_builder.build().value();

  this->_device = vkb_device.device;
  this->_chosenGPU = physical_device.physical_device;

  // Initialize Swap Chain
}
void VulkanEngine::init_swapchain() {
  create_swapchain(_windowExtent.width, _windowExtent.height);
}
void VulkanEngine::init_commands() {}
void VulkanEngine::init_sync_structures() {}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height) {
  vkb::SwapchainBuilder swapchain_builder{this->_chosenGPU, this->_device,
                                          this->_surface};
  this->_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

  vkb::Swapchain vkbSwapchain =
      swapchain_builder
          .set_desired_format(VkSurfaceFormatKHR{
              .format = this->_swapchainImageFormat,
              .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
          .set_desired_extent(width, height)
          .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
          .build()
          .value();
  this->_swapchainExtent = vkbSwapchain.extent;
  this->_swapchain = vkbSwapchain.swapchain;
  this->_swapchainImages = vkbSwapchain.get_images().value();
  this->_swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::destroy_swapchain() {
  vkDestroySwapchainKHR(this->_device, this->_swapchain, nullptr);
  for (auto &image_view : this->_swapchainImageViews) {
    vkDestroyImageView(this->_device, image_view, nullptr);
  }
}