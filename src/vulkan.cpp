#include "vulkan.hpp"

#include "shader_spirv.hpp"
#include "window.hpp"

#include <chrono>
#include <span>
#include <unordered_map>

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<char const *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

bool framebufferResized = false;

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f, 0.0f},  {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f},   {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f},    {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f},   {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f},  {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f},   {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f},  {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};

vk::SampleCountFlagBits Vulkan::getMaxUsableSampleCount() {
  vk::PhysicalDeviceProperties physicalDeviceProperties = physicalDevice.getProperties();

  vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
                                physicalDeviceProperties.limits.framebufferDepthSampleCounts;
  if (counts & vk::SampleCountFlagBits::e64) {
    return vk::SampleCountFlagBits::e64;
  }
  if (counts & vk::SampleCountFlagBits::e32) {
    return vk::SampleCountFlagBits::e32;
  }
  if (counts & vk::SampleCountFlagBits::e16) {
    return vk::SampleCountFlagBits::e16;
  }
  if (counts & vk::SampleCountFlagBits::e8) {
    return vk::SampleCountFlagBits::e8;
  }
  if (counts & vk::SampleCountFlagBits::e4) {
    return vk::SampleCountFlagBits::e4;
  }
  if (counts & vk::SampleCountFlagBits::e2) {
    return vk::SampleCountFlagBits::e2;
  }

  return vk::SampleCountFlagBits::e1;
}

void Vulkan::generateMipmaps(vk::raii::CommandBuffer &commandBuffer, vk::raii::Image &image, vk::Format imageFormat,
                             int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
  // Check if image format supports linear blitting
  vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(imageFormat);
  if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
    throw ERROR_LOC("texture image format does not support linear blitting!");
  }

  vk::ImageMemoryBarrier barrier = {
      .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
      .dstAccessMask = vk::AccessFlagBits::eTransferRead,
      .oldLayout = vk::ImageLayout::eTransferDstOptimal,
      .newLayout = vk::ImageLayout::eTransferSrcOptimal,
      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
      .image = image,
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1}
  };

  int32_t mipWidth = texWidth;
  int32_t mipHeight = texHeight;

  for (uint32_t i = 1; i < mipLevels; i++) {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {},
                                  {}, barrier);

    vk::ImageBlit blit = {
        .srcSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = i - 1, .layerCount = 1},
        .srcOffsets = std::array<vk::Offset3D, 2>({{}, {mipWidth, mipHeight, 1}}
          ),
        .dstSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = i, .layerCount = 1},
        .dstOffsets =
            std::array<vk::Offset3D, 2>({{}, {1 < mipWidth ? mipWidth / 2 : 1, 1 < mipHeight ? mipHeight / 2 : 1, 1}}
          )
    };

    commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal,
                            blit, vk::Filter::eLinear);

    barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {},
                                  {}, {}, barrier);

    if (1 < mipWidth) {
      mipWidth /= 2;
    }
    if (1 < mipHeight) {
      mipHeight /= 2;
    }
  }

  barrier.subresourceRange.baseMipLevel = mipLevels - 1;
  barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
  barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
  barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

  commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {},
                                {}, {}, barrier);
}

void Vulkan::cleanupSwapChain() {
  swapChainImageViews.clear();
  swapChain = nullptr;
}

vk::raii::ImageView Vulkan::createImageView(vk::Image const &image, vk::Format format, vk::ImageAspectFlags aspectFlags,
                                            uint32_t mipLevels) {
  vk::ImageViewCreateInfo viewInfo {
      .image = image,
      .viewType = vk::ImageViewType::e2D,
      .format = format,
      .subresourceRange = {
                           .aspectMask = aspectFlags, .baseMipLevel = 0, .levelCount = mipLevels, .baseArrayLayer = 0, .layerCount = 1}
  };
  return vk::raii::ImageView(device, viewInfo);
}

void Vulkan::createImageViews() {
  assert(swapChainImageViews.empty());

  swapChainImageViews.reserve(swapChainImages.size());
  for (auto &image : swapChainImages) {
    swapChainImageViews.emplace_back(
        createImageView(image, swapChainSurfaceFormat.format, vk::ImageAspectFlagBits::eColor, 1));
  }
}

vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities, Window &w) {
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  }
  int width, height;
  glfwGetFramebufferSize(w, &width, &height);

  return {std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
          std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
}

static vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes) {
  assert(std::ranges::any_of(availablePresentModes,
                             [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
  return std::ranges::any_of(availablePresentModes,
                             [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; })
             ? vk::PresentModeKHR::eMailbox
             : vk::PresentModeKHR::eFifo;
}

static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
  assert(!availableFormats.empty());
  const auto formatIt = std::ranges::find_if(availableFormats, [](const auto &format) {
    return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
  });
  return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}

static uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities) {
  auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
  if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount)) {
    minImageCount = surfaceCapabilities.maxImageCount;
  }
  return minImageCount;
}

void Vulkan::createSwapChain() {
  vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
  swapChainExtent = chooseSwapExtent(surfaceCapabilities, window);
  uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);

  std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
  swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);

  std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);
  vk::PresentModeKHR presentMode = chooseSwapPresentMode(availablePresentModes);

  vk::SwapchainCreateInfoKHR swapChainCreateInfo {.surface = *surface,
                                                  .minImageCount = minImageCount,
                                                  .imageFormat = swapChainSurfaceFormat.format,
                                                  .imageColorSpace = swapChainSurfaceFormat.colorSpace,
                                                  .imageExtent = swapChainExtent,
                                                  .imageArrayLayers = 1,
                                                  .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
                                                  .imageSharingMode = vk::SharingMode::eExclusive,
                                                  .preTransform = surfaceCapabilities.currentTransform,
                                                  .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
                                                  .presentMode = presentMode,
                                                  .clipped = true};

  swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
  swapChainImages = swapChain.getImages();
}

void Vulkan::recreateSwapChain() {
  int width = 0, height = 0;
  glfwGetFramebufferSize(window, &width, &height);
  while ((width == 0 || height == 0) && !glfwWindowShouldClose(window)) {
    glfwGetFramebufferSize(window, &width, &height);
    glfwWaitEvents();
  }
  if (glfwWindowShouldClose(window)) {
    return;
  }

  device.waitIdle();

  cleanupSwapChain();
  createSwapChain();
  createImageViews();
  createColorResources();
  createDepthResources();
}

std::vector<const char *> getRequiredInstanceExtensions() {
  uint32_t glfwExtensionCount = 0;
  auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
  if (enableValidationLayers) {
    extensions.push_back(vk::EXTDebugUtilsExtensionName);
  }

  return extensions;
}

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                                      vk::DebugUtilsMessageTypeFlagsEXT type,
                                                      const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                      void *) {

  static std::unordered_map<vk::DebugUtilsMessageSeverityFlagBitsEXT, Log::Level> map = {
      {vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose, Log::Level::DEBUG},
      {vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo,    Log::Level::INFO },
      {vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning, Log::Level::WARN },
      {vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,   Log::Level::ERROR},
  };

  LOG_LOC(map[severity], "Validation layer: type {}, msg: {}\n", to_string(type), pCallbackData->pMessage);

  return vk::False;
}

void Vulkan::createInstance() {
  constexpr vk::ApplicationInfo appInfo {.pApplicationName = "Hello Triangle",
                                         .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                         .pEngineName = "No Engine",
                                         .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                         .apiVersion = vk::ApiVersion14};

  // Get the required layers
  std::vector<char const *> requiredLayers;
  if (enableValidationLayers) {
    requiredLayers.assign(validationLayers.begin(), validationLayers.end());
  }

  // Check if the required layers are supported by the Vulkan implementation.
  auto layerProperties = context.enumerateInstanceLayerProperties();
  auto unsupportedLayerIt = std::ranges::find_if(requiredLayers, [&layerProperties](auto const &requiredLayer) {
    return std::ranges::none_of(layerProperties, [requiredLayer](auto const &layerProperty) {
      return strcmp(layerProperty.layerName, requiredLayer) == 0;
    });
  });
  if (unsupportedLayerIt != requiredLayers.end()) {
    throw ERROR_LOC("Required layer not supported: {}", *unsupportedLayerIt);
  }

  // Get the required extensions.
  auto requiredExtensions = getRequiredInstanceExtensions();

  // Check if the required extensions are supported by the Vulkan implementation.
  auto extensionProperties = context.enumerateInstanceExtensionProperties();
  auto unsupportedPropertyIt =
      std::ranges::find_if(requiredExtensions, [&extensionProperties](auto const &requiredExtension) {
        return std::ranges::none_of(extensionProperties, [requiredExtension](auto const &extensionProperty) {
          return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
        });
      });
  if (unsupportedPropertyIt != requiredExtensions.end()) {
    throw ERROR_LOC("Required extension not supported: {}", *unsupportedPropertyIt);
  }

  vk::InstanceCreateInfo createInfo {.pApplicationInfo = &appInfo,
                                     .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
                                     .ppEnabledLayerNames = requiredLayers.data(),
                                     .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
                                     .ppEnabledExtensionNames = requiredExtensions.data()};
  instance = vk::raii::Instance(context, createInfo);
}

void Vulkan::setupDebugMessenger() {
  if (!enableValidationLayers)
    return;

  vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
  vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                     vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                                     vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
  vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT {
      .messageSeverity = severityFlags, .messageType = messageTypeFlags, .pfnUserCallback = &debugCallback};
  debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
}

void Vulkan::createSurface() {
  VkSurfaceKHR _surface;
  if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0) {
    throw ERROR_LOC("failed to create window surface!");
  }
  surface = vk::raii::SurfaceKHR(instance, _surface);
}

bool Vulkan::isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice) {
  // Check if the physicalDevice supports the Vulkan 1.3 API version
  bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= VK_API_VERSION_1_3;

  // Check if any of the queue families support both graphics and presentation to our surface
  auto queueFamilies = physicalDevice.getQueueFamilyProperties();
  uint32_t qfpIndex = 0;
  bool supportsGraphicsAndPresent =
      std::ranges::any_of(queueFamilies, [&physicalDevice, &surface = surface, &qfpIndex](auto const &qfp) {
        bool const suitable =
            (qfp.queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface);
        qfpIndex++;
        return suitable;
      });

  // Check if all required physicalDevice extensions are available
  auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
  bool supportsAllRequiredExtensions =
      std::ranges::all_of(requiredDeviceExtension, [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
        return std::ranges::any_of(
            availableDeviceExtensions, [requiredDeviceExtension](auto const &availableDeviceExtension) {
              return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0;
            });
      });

  // Check if the physicalDevice supports the required features
  auto features = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
                                                       vk::PhysicalDeviceVulkan13Features,
                                                       vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
  bool supportsRequiredFeatures =
      features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
      features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
      features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
      features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

  // Return true if the physicalDevice meets all the criteria
  return supportsVulkan1_3 && supportsGraphicsAndPresent && supportsAllRequiredExtensions && supportsRequiredFeatures;
}

void Vulkan::pickPhysicalDevice() {
  std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
  auto const devIter = std::ranges::find_if(
      physicalDevices, [&](auto const &physicalDevice) { return isDeviceSuitable(physicalDevice); });
  if (devIter == physicalDevices.end()) {
    throw ERROR_LOC("failed to find a suitable GPU!");
  }
  physicalDevice = *devIter;
  msaaSamples = getMaxUsableSampleCount();
}

void Vulkan::createLogicalDevice() {
  std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

  // get the first index into queueFamilyProperties which supports both graphics and present
  for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++) {
    if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
        physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface)) {
      // found a queue family that supports both graphics and present
      queueIndex = qfpIndex;
      break;
    }
  }
  if (queueIndex == (uint32_t)~0) {
    throw ERROR_LOC("Could not find a queue for graphics and present.");
  }

  // query for Vulkan 1.3 features
  vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
                     vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
      featureChain = {
          {.features = {.samplerAnisotropy = true}}, // vk::PhysicalDeviceFeatures2
          {.synchronization2 = true, .dynamicRendering = true}, // vk::PhysicalDeviceVulkan13Features
          {.extendedDynamicState = true}  // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
  };

  // create a Device
  float queuePriority = 0.5f;
  vk::DeviceQueueCreateInfo deviceQueueCreateInfo {
      .queueFamilyIndex = queueIndex, .queueCount = 1, .pQueuePriorities = &queuePriority};
  vk::DeviceCreateInfo deviceCreateInfo {.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
                                         .queueCreateInfoCount = 1,
                                         .pQueueCreateInfos = &deviceQueueCreateInfo,
                                         .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
                                         .ppEnabledExtensionNames = requiredDeviceExtension.data()};

  device = vk::raii::Device(physicalDevice, deviceCreateInfo);
  queue = vk::raii::Queue(device, queueIndex, 0);
}

[[nodiscard]] vk::raii::ShaderModule Vulkan::createShaderModule(std::span<const uint32_t> code) {
  if (code.empty()) {
    throw ERROR_LOC("Shader SPIR-V was not embedded in the executable.");
  }

  vk::ShaderModuleCreateInfo createInfo {.codeSize = code.size_bytes(), .pCode = code.data()};
  vk::raii::ShaderModule shaderModule {device, createInfo};

  return shaderModule;
}

void Vulkan::createGraphicsPipeline() {
  vk::raii::ShaderModule shaderModule =
      createShaderModule(std::span<const uint32_t> {Shader::spirv, Shader::spirvWordCount});

  vk::PipelineShaderStageCreateInfo vertShaderStageInfo {
      .stage = vk::ShaderStageFlagBits::eVertex, .module = shaderModule, .pName = "vertMain"};
  vk::PipelineShaderStageCreateInfo fragShaderStageInfo {
      .stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModule, .pName = "fragMain"};
  vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

  auto bindingDescription = Vertex::getBindingDescription();
  auto attributeDescriptions = Vertex::getAttributeDescriptions();
  vk::PipelineVertexInputStateCreateInfo vertexInputInfo {.vertexBindingDescriptionCount = 1,
                                                          .pVertexBindingDescriptions = &bindingDescription,
                                                          .vertexAttributeDescriptionCount =
                                                              static_cast<uint32_t>(attributeDescriptions.size()),
                                                          .pVertexAttributeDescriptions = attributeDescriptions.data()};
  vk::PipelineInputAssemblyStateCreateInfo inputAssembly {.topology = vk::PrimitiveTopology::eTriangleList};
  vk::PipelineViewportStateCreateInfo viewportState {.viewportCount = 1, .scissorCount = 1};

  vk::PipelineRasterizationStateCreateInfo rasterizer {.depthClampEnable = vk::False,
                                                       .rasterizerDiscardEnable = vk::False,
                                                       .polygonMode = vk::PolygonMode::eFill,
                                                       .cullMode = vk::CullModeFlagBits::eBack,
                                                       .frontFace = vk::FrontFace::eCounterClockwise,
                                                       .depthBiasEnable = vk::False,
                                                       .lineWidth = 1.0f};

  vk::PipelineMultisampleStateCreateInfo multisampling {.rasterizationSamples = msaaSamples,
                                                        .sampleShadingEnable = vk::False};

  vk::PipelineDepthStencilStateCreateInfo depthStencil {.depthTestEnable = vk::True,
                                                        .depthWriteEnable = vk::True,
                                                        .depthCompareOp = vk::CompareOp::eLess,
                                                        .depthBoundsTestEnable = vk::False,
                                                        .stencilTestEnable = vk::False};

  vk::PipelineColorBlendAttachmentState colorBlendAttachment {
      .blendEnable = vk::False,
      .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

  vk::PipelineColorBlendStateCreateInfo colorBlending {.logicOpEnable = vk::False,
                                                       .logicOp = vk::LogicOp::eCopy,
                                                       .attachmentCount = 1,
                                                       .pAttachments = &colorBlendAttachment};

  std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
  vk::PipelineDynamicStateCreateInfo dynamicState {.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                                                   .pDynamicStates = dynamicStates.data()};

  vk::PipelineLayoutCreateInfo pipelineLayoutInfo {
      .setLayoutCount = 1, .pSetLayouts = &*descriptorSetLayout, .pushConstantRangeCount = 0};
  pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

  vk::Format depthFormat = findDepthFormat();

  vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
      {.stageCount = 2,
       .pStages = shaderStages,
       .pVertexInputState = &vertexInputInfo,
       .pInputAssemblyState = &inputAssembly,
       .pViewportState = &viewportState,
       .pRasterizationState = &rasterizer,
       .pMultisampleState = &multisampling,
       .pDepthStencilState = &depthStencil,
       .pColorBlendState = &colorBlending,
       .pDynamicState = &dynamicState,
       .layout = pipelineLayout,
       .renderPass = nullptr},
      {.colorAttachmentCount = 1,
       .pColorAttachmentFormats = &swapChainSurfaceFormat.format,
       .depthAttachmentFormat = depthFormat}
  };

  graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}

void Vulkan::createCommandPool() {
  vk::CommandPoolCreateInfo poolInfo {.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                      .queueFamilyIndex = queueIndex};
  commandPool = vk::raii::CommandPool(device, poolInfo);
}

uint32_t Vulkan::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
  vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }

  throw ERROR_LOC("failed to find suitable memory type!");
}

std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>
Vulkan::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
  vk::BufferCreateInfo bufferInfo {.size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive};
  vk::raii::Buffer buffer = vk::raii::Buffer(device, bufferInfo);
  vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
  vk::MemoryAllocateInfo allocInfo {.allocationSize = memRequirements.size,
                                    .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)};
  vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
  buffer.bindMemory(*bufferMemory, 0);
  return {std::move(buffer), std::move(bufferMemory)};
}

vk::raii::CommandBuffer Vulkan::beginSingleTimeCommands() {
  vk::CommandBufferAllocateInfo allocInfo {
      .commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1};
  vk::raii::CommandBuffer commandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());

  vk::CommandBufferBeginInfo beginInfo {.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
  commandBuffer.begin(beginInfo);

  return std::move(commandBuffer);
}

void Vulkan::endSingleTimeCommands(vk::raii::CommandBuffer &&commandBuffer) {
  commandBuffer.end();

  vk::SubmitInfo submitInfo {.commandBufferCount = 1, .pCommandBuffers = &*commandBuffer};
  queue.submit(submitInfo, nullptr);
  queue.waitIdle();
}

void Vulkan::copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::DeviceSize size) {
  vk::raii::CommandBuffer commandCopyBuffer = beginSingleTimeCommands();
  commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy {.size = size});
  endSingleTimeCommands(std::move(commandCopyBuffer));
}

void transitionImageLayout(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Image &image,
                           vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels) {
  vk::ImageMemoryBarrier barrier {
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
      .image = image,
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = mipLevels, .layerCount = 1}
  };

  vk::PipelineStageFlags sourceStage;
  vk::PipelineStageFlags destinationStage;

  if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.srcAccessMask = {};
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

    sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    destinationStage = vk::PipelineStageFlagBits::eTransfer;
  } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
             newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    sourceStage = vk::PipelineStageFlagBits::eTransfer;
    destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
  } else {
    throw ERROR_LOC("unsupported layout transition!");
  }
  commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, {}, barrier);
}

void copyBufferToImage(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Buffer &buffer, vk::raii::Image &image,
                       uint32_t width, uint32_t height) {
  vk::BufferImageCopy region {
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .mipLevel = 0,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
      .imageOffset = {0, 0, 0},
      .imageExtent = {width, height, 1}
  };
  commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
}

void Vulkan::createTextureImage() {
  int texWidth, texHeight, texChannels;
  stbi_uc *pixels = stbi_load("textures/image.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
  vk::DeviceSize imageSize = texWidth * texHeight * 4;
  mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

  if (!pixels) {
    throw ERROR_LOC("failed to load texture image!");
  }

  auto [stagingBuffer, stagingBufferMemory] =
      createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc,
                   vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

  void *data = stagingBufferMemory.mapMemory(0, imageSize);
  memcpy(data, pixels, imageSize);
  stagingBufferMemory.unmapMemory();

  stbi_image_free(pixels);

  std::tie(textureImage, textureImageMemory) = createImage(
      texWidth, texHeight, mipLevels, vk::SampleCountFlagBits::e1, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
      vk::MemoryPropertyFlagBits::eDeviceLocal);

  vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();
  transitionImageLayout(commandBuffer, textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                        mipLevels);
  copyBufferToImage(commandBuffer, stagingBuffer, textureImage, static_cast<uint32_t>(texWidth),
                    static_cast<uint32_t>(texHeight));
  generateMipmaps(commandBuffer, textureImage, vk::Format::eR8G8B8A8Srgb, texWidth, texHeight, mipLevels);
  endSingleTimeCommands(std::move(commandBuffer));
}

void Vulkan::createVertexBuffer() {
  vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

  auto [stagingBuffer, stagingBufferMemory] =
      createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                   vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

  void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
  memcpy(dataStaging, vertices.data(), bufferSize);
  stagingBufferMemory.unmapMemory();

  std::tie(vertexBuffer, vertexBufferMemory) =
      createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                   vk::MemoryPropertyFlagBits::eDeviceLocal);

  copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
}

void Vulkan::createIndexBuffer() {
  vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

  auto [stagingBuffer, stagingBufferMemory] =
      createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                   vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

  void *data = stagingBufferMemory.mapMemory(0, bufferSize);
  memcpy(data, indices.data(), (size_t)bufferSize);
  stagingBufferMemory.unmapMemory();

  std::tie(indexBuffer, indexBufferMemory) =
      createBuffer(bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                   vk::MemoryPropertyFlagBits::eDeviceLocal);

  copyBuffer(stagingBuffer, indexBuffer, bufferSize);
}

void Vulkan::createCommandBuffers() {
  commandBuffers.clear();
  vk::CommandBufferAllocateInfo allocInfo {.commandPool = commandPool,
                                           .level = vk::CommandBufferLevel::ePrimary,
                                           .commandBufferCount = MAX_FRAMES_IN_FLIGHT};
  commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
}

void Vulkan::transition_image_layout(vk::Image image, vk::ImageLayout old_layout, vk::ImageLayout new_layout,
                                     vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
                                     vk::PipelineStageFlags2 src_stage_mask, vk::PipelineStageFlags2 dst_stage_mask,
                                     vk::ImageAspectFlags image_aspect_flags) {
  vk::ImageMemoryBarrier2 barrier = {
      .srcStageMask = src_stage_mask,
      .srcAccessMask = src_access_mask,
      .dstStageMask = dst_stage_mask,
      .dstAccessMask = dst_access_mask,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {
                           .aspectMask = image_aspect_flags, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}
  };
  vk::DependencyInfo dependency_info = {
      .dependencyFlags = {}, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier};
  commandBuffers[frameIndex].pipelineBarrier2(dependency_info);
}

void Vulkan::recordCommandBuffer(uint32_t imageIndex) {
  auto &commandBuffer = commandBuffers[frameIndex];
  commandBuffer.begin({});

  // Before starting rendering, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
  transition_image_layout(swapChainImages[imageIndex], vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eColorAttachmentOptimal,
                          {}, // srcAccessMask (no need to wait for previous operations)
                          vk::AccessFlagBits2::eColorAttachmentWrite,         // dstAccessMask
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput, // dstStage
                          vk::ImageAspectFlagBits::eColor);

  // Transition the multisampled color image to vk::ImageLayout::eColorAttachmentOptimal
  transition_image_layout(*colorImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
                          vk::AccessFlagBits2::eColorAttachmentWrite, vk::AccessFlagBits2::eColorAttachmentWrite,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::ImageAspectFlagBits::eColor);

  // Transition the depth image to vk::ImageLayout::eDepthAttachmentOptimal
  transition_image_layout(
      *depthImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal,
      vk::AccessFlagBits2::eDepthStencilAttachmentWrite, vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
      vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
      vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
      vk::ImageAspectFlagBits::eDepth);

  vk::ClearValue clearColor = {};
  clearColor.color.float32[0] = 0.0f;
  clearColor.color.float32[1] = 0.0f;
  clearColor.color.float32[2] = 0.0f;
  clearColor.color.float32[3] = 1.0f;
  vk::ClearValue clearDepth = {};
  clearDepth.depthStencil.depth = 1.0f;
  clearDepth.depthStencil.stencil = 0;
  vk::RenderingAttachmentInfo colorAttachment = {.imageView = colorImageView,
                                                 .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                 .resolveMode = vk::ResolveModeFlagBits::eAverage,
                                                 .resolveImageView = swapChainImageViews[imageIndex],
                                                 .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                 .loadOp = vk::AttachmentLoadOp::eClear,
                                                 .storeOp = vk::AttachmentStoreOp::eStore,
                                                 .clearValue = clearColor};
  vk::RenderingAttachmentInfo depthAttachment = {.imageView = depthImageView,
                                                 .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
                                                 .loadOp = vk::AttachmentLoadOp::eClear,
                                                 .storeOp = vk::AttachmentStoreOp::eDontCare,
                                                 .clearValue = clearDepth};
  vk::RenderingInfo renderingInfo = {
      .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachment,
      .pDepthAttachment = &depthAttachment
  };
  commandBuffer.beginRendering(renderingInfo);
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
  commandBuffer.setViewport(0, vk::Viewport(0.0f, static_cast<float>(swapChainExtent.height),
                                            static_cast<float>(swapChainExtent.width),
                                            -static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
  commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));
  commandBuffer.bindVertexBuffers(0, *vertexBuffer, {0});
  commandBuffer.bindVertexBuffers(0, *vertexBuffer, {0});
  commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexTypeValue<decltype(indices)::value_type>::value);
  commandBuffers[frameIndex].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0,
                                                *descriptorSets[frameIndex], nullptr);
  commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
  commandBuffer.endRendering();

  // After rendering, transition the swapchain image to vk::ImageLayout::ePresentSrcKHR
  transition_image_layout(swapChainImages[imageIndex], vk::ImageLayout::eColorAttachmentOptimal,
                          vk::ImageLayout::ePresentSrcKHR,
                          vk::AccessFlagBits2::eColorAttachmentWrite,         // srcAccessMask
                          {},                                                 // dstAccessMask
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                          vk::PipelineStageFlagBits2::eBottomOfPipe,          // dstStage
                          vk::ImageAspectFlagBits::eColor);
  commandBuffer.end();
}

void Vulkan::createSyncObjects() {
  assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() && inFlightFences.empty());

  for (size_t i = 0; i < swapChainImages.size(); i++) {
    renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
  }

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
    inFlightFences.emplace_back(device, vk::FenceCreateInfo {.flags = vk::FenceCreateFlagBits::eSignaled});
  }
}

void Vulkan::updateUniformBuffer(uint32_t currentImage) {
  static auto startTime = std::chrono::high_resolution_clock::now();

  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

  UniformBufferObject ubo {};
  ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.proj = glm::perspective(glm::radians(45.0f),
                              static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height),
                              0.1f, 10.0f);

  memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void Vulkan::drawFrame() {

  // Note: inFlightFences, presentCompleteSemaphores, and commandBuffers are indexed by frameIndex,
  //       while renderFinishedSemaphores is indexed by imageIndex
  auto fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
  if (fenceResult != vk::Result::eSuccess) {
    throw ERROR_LOC("failed to wait for fence!");
  }

  auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);

  // Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined, eErrorOutOfDateKHR can be checked as a result
  // here and does not need to be caught by an exception.
  if (result == vk::Result::eErrorOutOfDateKHR) {
    recreateSwapChain();
    return;
  }
  // On other success codes than eSuccess and eSuboptimalKHR we just throw an exception.
  // On any error code, aquireNextImage already threw an exception.
  if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
    assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
    throw ERROR_LOC("failed to acquire swap chain image!");
  }
  updateUniformBuffer(frameIndex);

  // Only reset the fence if we are submitting work
  device.resetFences(*inFlightFences[frameIndex]);

  commandBuffers[frameIndex].reset();
  recordCommandBuffer(imageIndex);

  vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
  const vk::SubmitInfo submitInfo {.waitSemaphoreCount = 1,
                                   .pWaitSemaphores = &*presentCompleteSemaphores[frameIndex],
                                   .pWaitDstStageMask = &waitDestinationStageMask,
                                   .commandBufferCount = 1,
                                   .pCommandBuffers = &*commandBuffers[frameIndex],
                                   .signalSemaphoreCount = 1,
                                   .pSignalSemaphores = &*renderFinishedSemaphores[imageIndex]};
  queue.submit(submitInfo, *inFlightFences[frameIndex]);

  const vk::PresentInfoKHR presentInfoKHR {.waitSemaphoreCount = 1,
                                           .pWaitSemaphores = &*renderFinishedSemaphores[imageIndex],
                                           .swapchainCount = 1,
                                           .pSwapchains = &*swapChain,
                                           .pImageIndices = &imageIndex};
  result = queue.presentKHR(presentInfoKHR);
  // Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined, eErrorOutOfDateKHR can be checked as a result
  // here and does not need to be caught by an exception.
  if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || framebufferResized) {
    framebufferResized = false;
    recreateSwapChain();
  } else {
    // There are no other success codes than eSuccess; on any error code, presentKHR already threw an exception.
    assert(result == vk::Result::eSuccess);
  }
  frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Vulkan::fbResizeCallback(GLFWwindow * /* window */, int /* width */, int /* height */) {
  framebufferResized = true;
}

void Vulkan::createDescriptorSetLayout() {
  std::array<vk::DescriptorSetLayoutBinding, 2> bindings {
      {{.binding = 0,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex},
       {.binding = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment}}
  };

  vk::DescriptorSetLayoutCreateInfo layoutInfo {.bindingCount = static_cast<uint32_t>(bindings.size()),
                                                .pBindings = bindings.data()};
  descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
}

void Vulkan::createUniformBuffers() {
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
    auto [buffer, bufferMem] =
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    uniformBuffers.emplace_back(std::move(buffer));
    uniformBuffersMemory.emplace_back(std::move(bufferMem));
    uniformBuffersMapped.emplace_back(uniformBuffersMemory.back().mapMemory(0, bufferSize));
  }
}

void Vulkan::createDescriptorPool() {
  std::array<vk::DescriptorPoolSize, 2> poolSize {
      {{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = MAX_FRAMES_IN_FLIGHT},
       {.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = MAX_FRAMES_IN_FLIGHT}}
  };
  vk::DescriptorPoolCreateInfo poolInfo {.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                                         .maxSets = MAX_FRAMES_IN_FLIGHT,
                                         .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
                                         .pPoolSizes = poolSize.data()};
  descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
}

void Vulkan::createDescriptorSets() {
  std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);
  vk::DescriptorSetAllocateInfo allocInfo {.descriptorPool = descriptorPool,
                                           .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
                                           .pSetLayouts = layouts.data()};

  descriptorSets = device.allocateDescriptorSets(allocInfo);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DescriptorBufferInfo bufferInfo {
        .buffer = uniformBuffers[i], .offset = 0, .range = sizeof(UniformBufferObject)};
    vk::DescriptorImageInfo imageInfo {.sampler = textureSampler,
                                       .imageView = textureImageView,
                                       .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
    std::array<vk::WriteDescriptorSet, 2> descriptorWrites {
        {{.dstSet = descriptorSets[i],
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .pBufferInfo = &bufferInfo},
         {.dstSet = descriptorSets[i],
          .dstBinding = 1,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eCombinedImageSampler,
          .pImageInfo = &imageInfo}}
    };
    device.updateDescriptorSets(descriptorWrites, {});
  }
}

std::pair<vk::raii::Image, vk::raii::DeviceMemory>
Vulkan::createImage(uint32_t width, uint32_t height, uint32_t mipLevels, vk::SampleCountFlagBits numSamples,
                    vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                    vk::MemoryPropertyFlags properties) {
  vk::ImageCreateInfo imageInfo {
      .imageType = vk::ImageType::e2D,
      .format = format,
      .extent = {width, height, 1},
      .mipLevels = mipLevels,
      .arrayLayers = 1,
      .samples = numSamples,
      .tiling = tiling,
      .usage = usage,
      .sharingMode = vk::SharingMode::eExclusive,
      .initialLayout = vk::ImageLayout::eUndefined
  };

  vk::raii::Image image = vk::raii::Image(device, imageInfo);

  vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
  vk::MemoryAllocateInfo allocInfo {.allocationSize = memRequirements.size,
                                    .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)};
  vk::raii::DeviceMemory imageMemory = vk::raii::DeviceMemory(device, allocInfo);
  image.bindMemory(imageMemory, 0);

  return {std::move(image), std::move(imageMemory)};
}

void Vulkan::createTextureImageView() {
  textureImageView =
      createImageView(*textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, mipLevels);
}

void Vulkan::createTextureSampler() {
  vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
  vk::SamplerCreateInfo samplerInfo {.magFilter = vk::Filter::eLinear,
                                     .minFilter = vk::Filter::eLinear,
                                     .mipmapMode = vk::SamplerMipmapMode::eLinear,
                                     .addressModeU = vk::SamplerAddressMode::eRepeat,
                                     .addressModeV = vk::SamplerAddressMode::eRepeat,
                                     .addressModeW = vk::SamplerAddressMode::eRepeat,
                                     .mipLodBias = 0.0f,
                                     .anisotropyEnable = vk::True,
                                     .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
                                     .compareEnable = vk::False,
                                     .compareOp = vk::CompareOp::eAlways,
                                     .minLod = 0.0f,
                                     .maxLod = vk::LodClampNone};

  textureSampler = vk::raii::Sampler(device, samplerInfo);
}

vk::Format Vulkan::findSupportedFormat(const std::vector<vk::Format> &candidates, vk::ImageTiling tiling,
                                       vk::FormatFeatureFlags features) {
  for (const auto format : candidates) {
    vk::FormatProperties props = physicalDevice.getFormatProperties(format);

    if (((tiling == vk::ImageTiling::eLinear) && ((props.linearTilingFeatures & features) == features)) ||
        ((tiling == vk::ImageTiling::eOptimal) && ((props.optimalTilingFeatures & features) == features))) {
      return format;
    }
  }

  throw ERROR_LOC("failed to find supported format!");
}

vk::Format Vulkan::findDepthFormat() {
  return findSupportedFormat({vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
                             vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

void Vulkan::createDepthResources() {
  vk::Format depthFormat = findDepthFormat();

  std::tie(depthImage, depthImageMemory) =
      createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples, depthFormat, vk::ImageTiling::eOptimal,
                  vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
  depthImageView = createImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);
}

void Vulkan::createColorResources() {
  vk::Format colorFormat = swapChainSurfaceFormat.format;

  std::tie(colorImage, colorImageMemory) =
      createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples, colorFormat, vk::ImageTiling::eOptimal,
                  vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
                  vk::MemoryPropertyFlagBits::eDeviceLocal);
  colorImageView = createImageView(colorImage, colorFormat, vk::ImageAspectFlagBits::eColor, 1);
}

Vulkan::Vulkan(Window &w) : window(w) {
  LOG_LOC(Log::Level::DEBUG, "Initializing Vulkan.");

  glfwSetFramebufferSizeCallback(window, fbResizeCallback);
  createInstance();
  setupDebugMessenger();
  createSurface();
  pickPhysicalDevice();
  createLogicalDevice();
  createSwapChain();
  createImageViews();
  createDescriptorSetLayout();
  createGraphicsPipeline();
  createCommandPool();
  createColorResources();
  createDepthResources();
  createTextureImage();
  createTextureImageView();
  createTextureSampler();
  createVertexBuffer();
  createIndexBuffer();
  createUniformBuffers();
  createDescriptorPool();
  createDescriptorSets();
  createCommandBuffers();
  createSyncObjects();
}

Vulkan::~Vulkan() { device.waitIdle(); }