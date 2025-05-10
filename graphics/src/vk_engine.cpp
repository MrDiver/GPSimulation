#include "vk_engine.h"
#include "vk_pipelines.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_vulkan.h>

#include <VkBootstrap.h>

#include <chrono>
#include <thread>
#include <vulkan/vulkan_core.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "vk_initializers.h"
#include "vk_images.h"
#include "vk_types.h"

void DeletionQueue::add(std::function<void()> function) {
	_deletionQueue.push_back(function);
}

void DeletionQueue::flush(VkDevice device) {
    // Iterate in reverse
	for (auto it = _deletionQueue.rbegin(); it != _deletionQueue.rend(); ++it) {
		(*it)();
	}
	_deletionQueue.clear();
}


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
  init_descriptors();
  init_pipelines();

  // everything went fine
  _isInitialized = true;
}

void VulkanEngine::cleanup() {
  if (_isInitialized) {
	  vkDeviceWaitIdle(this->_device);
      for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
          //already written from before
          vkDestroyCommandPool(_device, _frames[i]._pool, nullptr);

          //destroy sync objects
          vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
          vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
          vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);
		  _frames[i]._deletionQueue.flush(this->_device);
      }
	_mainDeletionQueue.flush(this->_device);
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
	vk_check(vkWaitForFences(this->_device, 1, &get_current_frame()._renderFence, true, 1000000000));
	get_current_frame()._deletionQueue.flush(this->_device);
	vk_check(vkResetFences(this->_device, 1, &get_current_frame()._renderFence));

    uint32_t swapchainImageIndex;
    vk_check(vkAcquireNextImageKHR(this->_device, this->_swapchain, 1000000000, get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex));


	VkCommandBuffer cmd = get_current_frame()._buffer;
	vk_check(vkResetCommandBuffer(cmd, 0));

	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	// set up the command buffer for rendering
    _drawExtent.width = _drawImage.imageExtent.width;
    _drawExtent.height = _drawImage.imageExtent.height;

    vk_check(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // transition our main draw image into general layout so we can write into it
    // we will overwrite it all so we dont care about what was the older layout
    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    draw_background(cmd);

    //transition the draw image and the swapchain image into their correct transfer layouts
    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // execute a copy from the draw image into the swapchain
    vkutil::copy_image_to_image(cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);

    // set swapchain image layout to Present so we can show it on the screen
    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    //finalize the command buffer (we can no longer add commands, but it can now be executed)
    vk_check(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame()._swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame()._renderSemaphore);

    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    vk_check(vkQueueSubmit2(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    vk_check(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

    //increase the number of frames drawn
    _frameNumber++;
}

void VulkanEngine::draw_background(VkCommandBuffer cmd) {
	VkClearColorValue clearColor = { 0.0f, 0.5f, 0.0f, 1.0f };
	VkImageSubresourceRange range = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
	vkCmdClearColorImage(cmd, this->_drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &range);

        // bind the gradient drawing compute pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          _gradientPipeline);

        // bind the descriptor set containing the draw image for the compute
        // pipeline
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                _gradientPipelineLayout, 0, 1,
                                &_drawImageDescriptors, 0, nullptr);

        // execute the compute pipeline dispatch. We are using 16x16 workgroup
        // size so we need to divide by it
        vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0),
                      std::ceil(_drawExtent.height / 16.0), 1);
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

  _graphicsQueue = vkb_device.get_queue(vkb::QueueType::graphics).value();
  _graphicsQueueFamily =
	  vkb_device.get_queue_index(vkb::QueueType::graphics).value();

  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.physicalDevice = this->_chosenGPU;
  allocatorInfo.device = this->_device;
  allocatorInfo.instance = this->_instance;
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  vmaCreateAllocator(&allocatorInfo, &_allocator);
  _mainDeletionQueue.add([=, this]() { vmaDestroyAllocator(_allocator); });
}

void VulkanEngine::init_swapchain() {
    create_swapchain(_windowExtent.width, _windowExtent.height);
    VkExtent3D drawImageExtent = {
        .width = _windowExtent.width,
        .height = _windowExtent.height,
        .depth = 1,
    };
    _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages = {};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info = vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

    VmaAllocationCreateInfo rimg_alloc_info = {};
    rimg_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_alloc_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vmaCreateImage(_allocator, &rimg_info, &rimg_alloc_info, &_drawImage.image, &_drawImage.allocation, nullptr);

	VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

	vk_check(vkCreateImageView(this->_device, &rview_info, nullptr, &_drawImage.imageView));
        _mainDeletionQueue.add([=, this]() {
          vkDestroyImageView(this->_device, _drawImage.imageView, nullptr);
          vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
        });
}

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

void VulkanEngine::init_commands() {
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
		vk_check(vkCreateCommandPool(this->_device, &commandPoolInfo, nullptr, &_frames[i]._pool));
		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._pool, 1);
		vk_check(vkAllocateCommandBuffers(this->_device, &cmdAllocInfo, &_frames[i]._buffer));
    }
}

void VulkanEngine::init_sync_structures() {
	VkFenceCreateInfo fenceInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreInfo = vkinit::semaphore_create_info();

	for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
		vk_check(vkCreateFence(this->_device, &fenceInfo, nullptr, &_frames[i]._renderFence));
		vk_check(vkCreateSemaphore(this->_device, &semaphoreInfo, nullptr, &_frames[i]._swapchainSemaphore));
		vk_check(vkCreateSemaphore(this->_device, &semaphoreInfo, nullptr, &_frames[i]._renderSemaphore));
	}
}

void VulkanEngine::init_descriptors() {
  std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}};

  globalDescriptorAllocator.init_pool(_device, 10, sizes);
  // make the descriptor set layout for our compute draw
  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    _drawImageDescriptorLayout =
        builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  // allocate a descriptor set for our draw image
  _drawImageDescriptors =
      globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

  VkDescriptorImageInfo imgInfo{};
  imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  imgInfo.imageView = _drawImage.imageView;

  VkWriteDescriptorSet drawImageWrite = {};
  drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  drawImageWrite.pNext = nullptr;

  drawImageWrite.dstBinding = 0;
  drawImageWrite.dstSet = _drawImageDescriptors;
  drawImageWrite.descriptorCount = 1;
  drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  drawImageWrite.pImageInfo = &imgInfo;

  vkUpdateDescriptorSets(_device, 1, &drawImageWrite, 0, nullptr);

  // make sure both the descriptor allocator and the new layout get cleaned up
  // properly
  _mainDeletionQueue.add([&]() {
    globalDescriptorAllocator.destroy_pool(_device);

    vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
  });
}

void VulkanEngine::init_pipelines() { init_background_pipelines(); }

void VulkanEngine::init_background_pipelines() {
  VkPipelineLayoutCreateInfo computeLayout{};
  computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  computeLayout.pNext = nullptr;
  computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
  computeLayout.setLayoutCount = 1;

  vk_check(vkCreatePipelineLayout(_device, &computeLayout, nullptr,
                                  &_gradientPipelineLayout));

  VkShaderModule computeDrawShader;
  if (!vkutil::load_shader_module("build/shaders/gradient_cs.spv",
                                  _device, // TODO: HLSL Doesn't work
                                  &computeDrawShader)) {
    fmt::print("Error when building the compute shader \n");
    std::abort();
  }

  VkPipelineShaderStageCreateInfo stageinfo{};
  stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stageinfo.pNext = nullptr;
  stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stageinfo.module = computeDrawShader;
  stageinfo.pName = "main";

  VkComputePipelineCreateInfo computePipelineCreateInfo{};
  computePipelineCreateInfo.sType =
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.pNext = nullptr;
  computePipelineCreateInfo.layout = _gradientPipelineLayout;
  computePipelineCreateInfo.stage = stageinfo;

  vk_check(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1,
                                    &computePipelineCreateInfo, nullptr,
                                    &_gradientPipeline));

  vkDestroyShaderModule(_device, computeDrawShader, nullptr);

  _mainDeletionQueue.add([&]() {
    vkDestroyPipelineLayout(_device, _gradientPipelineLayout, nullptr);
    vkDestroyPipeline(_device, _gradientPipeline, nullptr);
  });
}

FrameData& VulkanEngine::get_current_frame() {
	return _frames[_frameNumber % FRAME_OVERLAP];
}
