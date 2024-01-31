
#include "VkHelper.h"
#include "VkShaderParam.h"
#include "Geometry.h"

const DriverFeature validationInstanceLayers[] = {
        {"VK_LAYER_KHRONOS_validation",    false, false}
};

const DriverFeature validationInstanceExtensions[] = {
        {VK_KHR_SURFACE_EXTENSION_NAME,          false,     true},
        {VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,    false,     true},
#ifdef RENDER_USE_SINGLE_BUFFER
        {VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME, false, true},
#endif
        {VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME, false,     true},
        {VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME, false,  true},
        {VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, false, true}
};

const DriverFeature validationDeviceExtensions[] = {
        {VK_KHR_SWAPCHAIN_EXTENSION_NAME, false, true},
#ifdef RENDER_USE_SINGLE_BUFFER
        {VK_KHR_SHARED_PRESENTABLE_IMAGE_EXTENSION_NAME, false, true},
#endif
        {VK_KHR_MAINTENANCE1_EXTENSION_NAME, false, true},
        {VK_KHR_BIND_MEMORY_2_EXTENSION_NAME, false, true},
        {VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, false, true},
        {VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME, false, true},
        {VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME, false, true},
        {VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME, false, true},
        {VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME, false, true},
        {VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME, false, true},
        {VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME, false, true},
        {VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME, false, true}
};

VkBool32 debugReportCallback(VkDebugReportFlagsEXT msgFlags, VkDebugReportObjectTypeEXT objType, uint64_t srcObject,
                             size_t location, int32_t msgCode, const char *pLayerPrefix, const char *pMsg, void *pUserData){
    if(msgFlags |= VK_DEBUG_REPORT_ERROR_BIT_EXT){
        LOG_E("%s: [%s] Code %d : %s", "Error", pLayerPrefix, msgCode, pMsg);
    } else if(msgFlags |= VK_DEBUG_REPORT_WARNING_BIT_EXT) {
        LOG_W("%s: [%s] Code %d : %s", "Warning", pLayerPrefix, msgCode, pMsg);
    } else {
        LOG_D("%s: [%s] Code %d : %s", "Info", pLayerPrefix, msgCode, pMsg);
    }
    return VK_FALSE;
}

void VkHelper::createInstance(bool bValidate, VkInstance *out_instance, VkDebugReportCallbackEXT *out_debugReport) {
    //instance layers
    uint32_t availableLayerCount;
    CALL_VK(vkEnumerateInstanceLayerProperties(&availableLayerCount, nullptr));
    VkLayerProperties availableLayerProps[availableLayerCount];
    CALL_VK(vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayerProps));
    LOG_D("---------------------------------");
    const char *enabledLayerNames[32] = {};
    uint32_t enabledLayerCount = 0;
    checkFeatures("Instance Layers", bValidate, false, validationInstanceLayers, ARRAY_SIZE(validationInstanceLayers),
                  availableLayerProps, availableLayerCount, enabledLayerNames, &enabledLayerCount);

    //instance extensions
    uint32_t availableExtensionCount;
    CALL_VK(vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr));
    VkExtensionProperties availableExtensionProps[availableExtensionCount];
    CALL_VK(vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availableExtensionProps));
    LOG_D("---------------------------------");
    const char *enableExtensionNames[32] = {};
    uint32_t enableExtensionCount = 0;
    checkFeatures("Instance Extensions", bValidate, true, validationInstanceExtensions, ARRAY_SIZE(validationInstanceExtensions),
                  availableExtensionProps, availableExtensionCount, enableExtensionNames, &enableExtensionCount);

    VkApplicationInfo application = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = APP_NAME,
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = APP_NAME,
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_MAKE_VERSION(1, 3, 0)
    };

    VkInstanceCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .pApplicationInfo = &application,
            .enabledLayerCount = enabledLayerCount,
            .ppEnabledLayerNames = enabledLayerCount == 0 ? nullptr : enabledLayerNames,
            .enabledExtensionCount = enableExtensionCount,
            .ppEnabledExtensionNames = enableExtensionCount == 0 ? nullptr : enableExtensionNames
    };

    //在创建VkInstance之前
    if(bValidate){
        VkDebugReportCallbackCreateInfoEXT debugReport = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
                .pNext = nullptr,
                .flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |				//错误信息
                         VK_DEBUG_REPORT_WARNING_BIT_EXT |				//警告信息
                         VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT,	//性能警告信息
                .pfnCallback = (PFN_vkDebugReportCallbackEXT) debugReportCallback,
                .pUserData = nullptr,
        };
        createInfo.pNext = &debugReport;
    }

    CALL_VK(vkCreateInstance(&createInfo, VK_ALLOC, out_instance));

    vkGetPhysicalDeviceSurfaceCapabilities2KHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR>(
            vkGetInstanceProcAddr(*out_instance, "vkGetPhysicalDeviceSurfaceCapabilities2KHR"));
    LOG_D("vkGetPhysicalDeviceSurfaceCapabilities2KHR==%p", vkGetPhysicalDeviceSurfaceCapabilities2KHR);
}

void VkHelper::pickPhyDevAndCreateDev(VkInstance instance, VkSurfaceKHR surface, DeviceInfo *out_deviceInfo, QueueInfo *out_queueInfo) {
    uint32_t physicalDevCount;
    CALL_VK(vkEnumeratePhysicalDevices(instance, &physicalDevCount, nullptr));
    VkPhysicalDevice physicalDevs[physicalDevCount];
    CALL_VK(vkEnumeratePhysicalDevices(instance, &physicalDevCount, physicalDevs));

    LOG_D("---------------------------------");
    LOG_D("Physical Devices:");
    //选取的物理设备和队列索引
    VkPhysicalDevice selectedPhysicalDev = nullptr;
    uint16_t workQueueIndex;
    uint16_t presentQueueIndex;

    for(int i = 0; i < physicalDevCount; i++){
        VkPhysicalDeviceProperties devProps;
        vkGetPhysicalDeviceProperties(physicalDevs[i], &devProps);

        //打印Log，不影响流程
        printPhysicalDevLog(devProps);

        uint32_t queueFamilyPropCount;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevs[i], &queueFamilyPropCount, nullptr);
        VkQueueFamilyProperties queueFamilyProps[queueFamilyPropCount];
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevs[i], &queueFamilyPropCount, queueFamilyProps);
        //表面查询支持
        for(int j = 0; j < queueFamilyPropCount; j++){
            VkBool32 surfaceSupport;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevs[i], i, surface, &surfaceSupport);
            LOG_D("Surface Support: %d", surfaceSupport);

            if(surfaceSupport && queueFamilyProps[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                selectedPhysicalDev = physicalDevs[i];
                workQueueIndex = j;
                presentQueueIndex = j;
                break;
            }
        }
    }
    if(nullptr == selectedPhysicalDev){
        throw std::runtime_error("Can not find any suitable Physical device.");
    }

    VkPhysicalDeviceProperties devProps;
    vkGetPhysicalDeviceProperties(selectedPhysicalDev, &devProps);
    out_deviceInfo->physicalDev = selectedPhysicalDev;
    out_deviceInfo->physicalDevLimits = devProps.limits;
    out_queueInfo->workQueueIndex = workQueueIndex;
    out_queueInfo->presentQueueIndex = presentQueueIndex;
    vkGetPhysicalDeviceMemoryProperties(selectedPhysicalDev, &out_deviceInfo->physicalDevMemoProps);

    uint32_t availableDeviceExtensionCount;
    CALL_VK(vkEnumerateDeviceExtensionProperties(out_deviceInfo->physicalDev, nullptr, &availableDeviceExtensionCount, nullptr));
    VkExtensionProperties availableDeviceExtensions[availableDeviceExtensionCount];
    CALL_VK(vkEnumerateDeviceExtensionProperties(out_deviceInfo->physicalDev, nullptr, &availableDeviceExtensionCount, availableDeviceExtensions));
    const char *enableDeviceExtensions[32];
    uint32_t enableDeviceExtensionCount;
    checkFeatures("Device Extensions", true, true, validationDeviceExtensions, ARRAY_SIZE(validationDeviceExtensions),
                  availableDeviceExtensions, availableDeviceExtensionCount, enableDeviceExtensions, &enableDeviceExtensionCount);

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = out_queueInfo->workQueueIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority, //队列优先级
    };

    //features
    VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcrFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,
            .pNext = nullptr,
            .samplerYcbcrConversion = VK_TRUE
    };
    VkPhysicalDeviceFeatures2 phyDevFeatures2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &ycbcrFeatures,
    };
    phyDevFeatures2.features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &phyDevFeatures2,
            .flags = 0,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCreateInfo,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = enableDeviceExtensionCount,
            .ppEnabledExtensionNames = enableDeviceExtensionCount == 0 ? nullptr : enableDeviceExtensions
    };
    CALL_VK(vkCreateDevice(out_deviceInfo->physicalDev, &deviceCreateInfo, VK_ALLOC, &out_deviceInfo->device));

    vkGetDeviceQueue(out_deviceInfo->device, out_queueInfo->workQueueIndex, 0, &out_queueInfo->queue);

    vkGetSwapchainStatusKHR = reinterpret_cast<PFN_vkGetSwapchainStatusKHR>(
            vkGetDeviceProcAddr(out_deviceInfo->device, "vkGetSwapchainStatusKHR"));
    LOG_D("vkGetSwapchainStatusKHR==%p", vkGetSwapchainStatusKHR);
}

void VkHelper::printPhysicalDevLog(const VkPhysicalDeviceProperties &devProps) {
    /****************** For log *********************/
    const uint32_t driverMajor = VK_VERSION_MAJOR(devProps.driverVersion);
    const uint32_t driverMinor = VK_VERSION_MINOR(devProps.driverVersion);
    const uint32_t driverPatch = VK_VERSION_PATCH(devProps.driverVersion);

    const uint32_t apiMajor = VK_VERSION_MAJOR(devProps.apiVersion);
    const uint32_t apiMinor = VK_VERSION_MINOR(devProps.apiVersion);
    const uint32_t apiPatch = VK_VERSION_PATCH(devProps.apiVersion);

    LOG_D("---------------------------------");
    LOG_D("Device Name          : %s", devProps.deviceName);
    LOG_D("Device Type          : %s",
          ((devProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) ? "integrated GPU" :
           ((devProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? "discrete GPU" :
            ((devProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) ? "virtual GPU" :
             ((devProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) ? "CPU" : "unknown")))));
    LOG_D("Vendor ID            : 0x%04X", devProps.vendorID);
    LOG_D("Device ID            : 0x%04X", devProps.deviceID);
    LOG_D("Driver Version       : %d.%d.%d", driverMajor, driverMinor, driverPatch);
    LOG_D("API Version          : %d.%d.%d", apiMajor, apiMinor, apiPatch);
    /****************** For log *********************/
}

void VkHelper::createCommandPool(VkDevice device, uint32_t workQueueIndex, VkCommandPool *out_cmdPool) {
    VkCommandPoolCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = workQueueIndex
    };
    CALL_VK(vkCreateCommandPool(device, &createInfo, VK_ALLOC, out_cmdPool));
}

void querySwapchainParam(VkPhysicalDevice physicalDev, VkSurfaceKHR surface, SwapchainParam *out_swapchainParam){
    //capabilities
    CALL_VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDev, surface, &out_swapchainParam->capabilities));
    out_swapchainParam->extent = out_swapchainParam->capabilities.currentExtent;
    LOG_D("Extent: %d, %d", out_swapchainParam->extent.width, out_swapchainParam->extent.height);

    VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
            .pNext = nullptr,
            .surface = surface
    };
    out_swapchainParam->capabilities2.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
    out_swapchainParam->capabilities2.pNext = nullptr;
    CALL_VK(vkGetPhysicalDeviceSurfaceCapabilities2KHR(physicalDev, &surfaceInfo, &out_swapchainParam->capabilities2));

    //format
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDev, surface, &formatCount, nullptr);
    VkSurfaceFormatKHR formats[formatCount];
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDev, surface, &formatCount, formats);
    bool bFindFormat = false;
    if(formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED){
        out_swapchainParam->format = {VK_FORMAT_B8G8R8A8_UNORM,   VK_COLORSPACE_SRGB_NONLINEAR_KHR};
        bFindFormat = true;
    }  else {
        for (const auto &item: formats) {
            if(item.format == VK_FORMAT_B8G8R8A8_UNORM && item.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR){
                out_swapchainParam->format = item;
                bFindFormat = true;
                break;
            }
        }
    }
    if(!bFindFormat){
        out_swapchainParam->format = formats[0];
    }

    //present mode
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDev, surface, &presentModeCount, nullptr);
    VkPresentModeKHR presentModes[presentModeCount];
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDev, surface, &presentModeCount, presentModes);

    //VK_PRESENT_MODE_IMMEDIATE_KHR 立即提交，可能导致画面撕裂
    //VK_PRESENT_MODE_FIFO_KHR 先进先出的队列，队列满时需要等待，类似垂直同步
    //VK_PRESENT_MODE_FIFO_RELAXED_KHR 和上一模式的区别是：如果应用程序延迟，导致交换链的队列在上一次垂直回扫时为空， 那么，如果应用程序在下一次垂直回扫前提交图像，图像会立即被显示。这一模式可能会导致撕裂现象。
    //VK_PRESENT_MODE_MAILBOX_KHR 这一模式是第二种模式的另一个变种。它不会在交换链的队列满时阻塞应用程序，队列中的图像会被直接替换为应用程序新提交的图像。这一模式可以用来实现三倍缓冲，避免撕裂现象的同时减小了延迟问题
    bool bFoundPrentMode = false;
    for (const auto &item: presentModes) {
        if(VK_PRESENT_MODE_MAILBOX_KHR == item){
            out_swapchainParam->presentMode = item;
            bFoundPrentMode = true;
        } else if(VK_PRESENT_MODE_IMMEDIATE_KHR == item){ //目前大部分硬件不不支持VK_PRESENT_MODE_MAILBOX_KHR
            out_swapchainParam->presentMode = item;
            bFoundPrentMode = true;
        }
    }
    if(!bFoundPrentMode){
        out_swapchainParam->presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    }
}

void VkHelper::createSwapchain(VkPhysicalDevice physicalDev, VkDevice device, VkSurfaceKHR surface,
                               uint16_t workQueueIndex, uint16_t presentQueueIndex,
                               SwapchainParam *out_swapchainParam, VkSwapchainKHR *out_swapchain){
    querySwapchainParam(physicalDev, surface, out_swapchainParam);

    uint32_t queueFamilysArr[2] = {static_cast<uint32_t>(workQueueIndex), static_cast<uint32_t>(presentQueueIndex)};

    VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE;
    if(nullptr != out_swapchain && VK_NULL_HANDLE != *out_swapchain){
        oldSwapchain = *out_swapchain;
    }

#ifdef RENDER_USE_SINGLE_BUFFER
    uint32_t minImageCount = 1;
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR;// or VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR
#else
    uint32_t minImageCount = out_swapchainParam->capabilities.minImageCount;
    VkPresentModeKHR presentMode = out_swapchainParam->presentMode;
#endif

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            //上文创建的窗口表面
            .surface = surface,
            .minImageCount = minImageCount,
            .imageFormat = out_swapchainParam->format.format,
            .imageColorSpace = out_swapchainParam->format.colorSpace,
            .imageExtent = out_swapchainParam->extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = workQueueIndex == presentQueueIndex ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
            .queueFamilyIndexCount = static_cast<uint32_t>(workQueueIndex == presentQueueIndex ? 1 : 2),
            .pQueueFamilyIndices = workQueueIndex == presentQueueIndex ? nullptr : queueFamilysArr,
            .preTransform = out_swapchainParam->capabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
            .presentMode = presentMode,
            .clipped = VK_TRUE,
            .oldSwapchain = oldSwapchain
    };

    CALL_VK(vkCreateSwapchainKHR(device, &swapchainCreateInfo, VK_ALLOC, out_swapchain));
}

void VkHelper::createRenderPass(VkDevice device, SwapchainParam swapchainParam, VkRenderPass *out_renderPass) {
    VkAttachmentDescription attachDesc[] = {
            {
                    .flags = 0,
                    .format = swapchainParam.format.format,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    //VK_ATTACHMENT_LOAD_OP_LOAD：保持附着的现有内容
                    //VK_ATTACHMENT_LOAD_OP_CLEAR：使用一个常量值来清除附着的内容
                    //VK_ATTACHMENT_LOAD_OP_DONT_CARE：不关心附着现存的内容
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    //K_ATTACHMENT_STORE_OP_STORE：渲染的内容会被存储起来，以便之后读取
                    //VK_ATTACHMENT_STORE_OP_DONT_CARE：渲染后，不会读取帧缓冲的内容
                    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            }
    };
    VkAttachmentReference colorAttachRef = {
            .attachment = 0,										// 0表示上文attachDesc[]数组的第1位
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// 作为颜色附件使用
    };

    VkSubpassDescription subpass = {
            .flags = 0,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = 0,
            .pInputAttachments = nullptr,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachRef,
            .preserveAttachmentCount = 0,
            .pPreserveAttachments = nullptr,
    };

    VkSubpassDependency dependencys[] = {
            {
                    .srcSubpass = VK_SUBPASS_EXTERNAL,
                    .dstSubpass = 0,
                    .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                    .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            },
            {
                    .srcSubpass = 0,
                    .dstSubpass = VK_SUBPASS_EXTERNAL,
                    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            }
    };

    VkRenderPassCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .attachmentCount = ARRAY_SIZE(attachDesc),
            .pAttachments = attachDesc,
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = ARRAY_SIZE(dependencys),
            .pDependencies = dependencys
    };

    CALL_VK(vkCreateRenderPass(device, &createInfo, VK_ALLOC, out_renderPass));
}

void VkHelper::createFramebuffer(VkDevice device, VkRenderPass renderPass,
                                 uint32_t width, uint32_t height,
                                 uint32_t framebufferCount, VkImageView *imageViews, VkFramebuffer *out_framebuffers) {
    for(uint32_t i = 0; i < framebufferCount; i++){
        VkImageView attachment[] = {
                imageViews[i],
        };
        VkFramebufferCreateInfo createInfo = {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .renderPass = renderPass,
                .attachmentCount = ARRAY_SIZE(attachment),
                .pAttachments = attachment,
                .width = width,
                .height = height,
                .layers = 1
        };
        CALL_VK(vkCreateFramebuffer(device, &createInfo, VK_ALLOC, &out_framebuffers[i]));
    }
}

void VkHelper::createPipelineLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, VkPipelineLayout *out_pipelineLayout) {
    VkPipelineLayoutCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = 1,
            .pSetLayouts = &descriptorSetLayout,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr
    };
    CALL_VK(vkCreatePipelineLayout(device, &createInfo, VK_ALLOC, out_pipelineLayout));
}

VkShaderModule VkHelper::createShaderModule(VkDevice device, std::vector<char> &code) {
    VkShaderModuleCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule shaderModule;
    if(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }
    return shaderModule;
}

void VkHelper::createPipeline(VkDevice device, VkPipelineLayout pipelineLayout, VkRenderPass renderPass,
                              VkShaderModule vertShaderModule, VkShaderModule fragShaderModule, SwapchainParam swapchainParam,
                              VkPipeline *out_pipeline) {
    VkPipelineShaderStageCreateInfo stages[] = {
            {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .stage = VK_SHADER_STAGE_VERTEX_BIT,
                    .module = vertShaderModule,
                    .pName = "main",
                    .pSpecializationInfo = nullptr
            },
            {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .module = fragShaderModule,
                    .pName = "main",
                    .pSpecializationInfo = nullptr
            }
    };

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptors = Vertex::getAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &bindingDescription,
            .vertexAttributeDescriptionCount = attributeDescriptors.size(),
            .pVertexAttributeDescriptions = attributeDescriptors.data()
    };

    VkPipelineInputAssemblyStateCreateInfo assemblyStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
    };

    VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = static_cast<float>(swapchainParam.extent.width),
            .height = static_cast<float>(swapchainParam.extent.height),
            .minDepth = 0,
            .maxDepth = 1
    };

    VkRect2D scissor = {
            .offset = {0, 0},
            .extent = swapchainParam.extent,
    };

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .viewportCount = 1,
            .pViewports = &viewport,
            .scissorCount = 1,
            .pScissors = &scissor
    };

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthClampEnable = VK_FALSE,        //计算阴影
            .rasterizerDiscardEnable = VK_FALSE, //VK_TRUE表示所有几何图元都不能通过光栅化阶段。这一设置会禁止一切片段输出到帧缓冲
            // VK_POLYGON_MODE_FILL：整个多边形，包括多边形内部都产生片段
            // VK_POLYGON_MODE_LINE：只有多边形的边会产生片段
            // VK_POLYGON_MODE_POINT：只有多边形的顶点会产生片段
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,   //表面剔除
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, //逆时针标识为前面
            .depthBiasEnable = VK_FALSE,
            .depthBiasConstantFactor = 0,
            .depthBiasClamp = 0,
            .depthBiasSlopeFactor = 0,
            .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,    //抗锯齿
            .minSampleShading = 1.0f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE
    };

    VkPipelineColorBlendAttachmentState attachmentState = {
            .blendEnable = VK_FALSE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
                    | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &attachmentState,
            .blendConstants = {0, 0, 0, 0},
    };

    VkDynamicState dynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .dynamicStateCount = ARRAY_SIZE(dynamicStates),
            .pDynamicStates = dynamicStates
    };

    VkGraphicsPipelineCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stageCount = ARRAY_SIZE(stages),
            .pStages = stages,
            .pVertexInputState = &vertexInputStateCreateInfo,
            .pInputAssemblyState = &assemblyStateCreateInfo,
            .pTessellationState = nullptr,
            .pViewportState = &viewportStateCreateInfo,
            .pRasterizationState = &rasterizationStateCreateInfo,
            .pMultisampleState = &multisampleStateCreateInfo,
            .pDepthStencilState = nullptr,
            .pColorBlendState = &colorBlendStateCreateInfo,
            .pDynamicState = &dynamicStateCreateInfo,
            .layout = pipelineLayout,
            .renderPass = renderPass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = 0
    };

    CALL_VK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, VK_ALLOC, out_pipeline));
}

void VkHelper::allocateCommandBuffers(VkDevice device, VkCommandPool cmdPool, uint32_t cmdBufferCount, VkCommandBuffer *cmdBuffers) {
    VkCommandBufferAllocateInfo allocateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = cmdPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = cmdBufferCount
    };

    CALL_VK(vkAllocateCommandBuffers(device, &allocateInfo, cmdBuffers));
}

void VkHelper::beginCommandBuffer(VkCommandBuffer cmdBuffer, bool oneTime) {
    VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            //VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT：指令缓冲在执行一次后，就被用来记录新的指令。
            //VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT：这是一个只在一个渲染流程内使用的辅助指令缓冲。
            //VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT：在指令缓冲等待执行时，仍然可以提交这一指令缓冲。
            .flags = oneTime ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
            .pInheritanceInfo = nullptr
    };

    CALL_VK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));
}

void VkHelper::endCommandBuffer(VkCommandBuffer cmdBuffer, VkDevice device, VkCommandPool cmdPool, VkQueue queue, bool bFree) {
    CALL_VK(vkEndCommandBuffer(cmdBuffer));
    if(bFree){
        VkSubmitInfo submitInfo = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &cmdBuffer
        };
        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);
        vkFreeCommandBuffers(device, cmdPool, 1, &cmdBuffer);
    }
}

void VkHelper::createBuffer(VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer *outBuffer) {
    VkBufferCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = size,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE    //不同步
    };
    CALL_VK(vkCreateBuffer(device, &createInfo, VK_ALLOC, outBuffer));
}

uint32_t VkHelper::findMemoryType(VkPhysicalDeviceMemoryProperties physicalMemoType, uint32_t typeFilter,
                                  VkMemoryPropertyFlags properties) {
    for(uint32_t i = 0; i < physicalMemoType.memoryTypeCount; i++){
        if(typeFilter & (1 << i) && (physicalMemoType.memoryTypes[i].propertyFlags & properties) == properties){
            return i;
        }
    }
    LOG_E("failed to find suitable memory type.");
    return -1;
}

uint32_t VkHelper::getMemoryIndex(VkPhysicalDeviceMemoryProperties phyProperties, uint32_t memoryTypeBits, MemoryLocation location){

    int mem_index {0};
    VkMemoryPropertyFlags mem_flags = VkMemoryPropertyFlags{0};

    switch(location)
    {
        case MemoryLocation::DEVICE:
            mem_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            break;
        case MemoryLocation::HOST:
            mem_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            break;
        default:
            break;
    }

    if(location == MemoryLocation::EXTERNAL)
    {
        for(auto& type : phyProperties.memoryTypes)
        {
            if((memoryTypeBits & (1 << mem_index)) &&
               ((type.propertyFlags & mem_flags) == VkMemoryPropertyFlags{0}))
                break;
            mem_index++;
        }
    }
    else
    {
        for(auto& type : phyProperties.memoryTypes)
        {
            if((memoryTypeBits & (1 << mem_index)) &&
               (type.propertyFlags & mem_flags))
                break;
            mem_index++;
        }
    }
    return mem_index;
}

void VkHelper::copyBuffer(VkDevice device, VkCommandPool cmdPool, VkQueue queue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer cmdBuffer;
    allocateCommandBuffers(device, cmdPool, 1, &cmdBuffer);
    beginCommandBuffer(cmdBuffer, true);

    VkBufferCopy copyRegion;
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(cmdBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endCommandBuffer(cmdBuffer, device, cmdPool, queue, true);
}

void VkHelper::createBufferInternal(VkPhysicalDeviceMemoryProperties physicalMemoType, VkDevice device,
                                    VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps, VkDeviceSize size,
                                    VkBuffer *outBuffer, VkDeviceMemory *outMemory) {
    createBuffer(device, size, usage, outBuffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, *outBuffer, &memRequirements);
    VkMemoryAllocateInfo memAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = findMemoryType(physicalMemoType, memRequirements.memoryTypeBits, memProps)
    };
    CALL_VK(vkAllocateMemory(device, &memAllocateInfo, VK_ALLOC, outMemory));
    CALL_VK(vkBindBufferMemory(device, *outBuffer, *outMemory, 0));
}

void VkHelper::createBuffer(VkPhysicalDeviceMemoryProperties physicalMemoType, VkDevice device, VkCommandPool cmdPool,
                            VkQueue queue, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps,
                            uint32_t dataSize, const void *data, VkBuffer *outBuffer,
                            VkDeviceMemory *outMemory) {

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemo;
    createBufferInternal(physicalMemoType, device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         dataSize, &stagingBuffer, &stagingMemo);
    void *data_ptr;
    CALL_VK(vkMapMemory(device, stagingMemo, 0, dataSize, 0, &data_ptr));
    memcpy(data_ptr, data, (size_t) dataSize);

    createBufferInternal(physicalMemoType, device, usage, memProps, dataSize, outBuffer, outMemory);
    copyBuffer(device, cmdPool, queue, stagingBuffer, *outBuffer, dataSize);

    vkDestroyBuffer(device, stagingBuffer, VK_ALLOC);
    vkFreeMemory(device, stagingMemo, VK_ALLOC);
}

void VkHelper::initGeometryBuffers(VkPhysicalDeviceMemoryProperties physicalMemoType, VkDevice device,
                                   VkCommandPool cmdPool, VkQueue queue, Geometry &geometry){
    if(geometry.vertices.empty()){
        LOG_E("The geometry's vertices is empty.");
        return;
    }
    createBuffer(physicalMemoType, device, cmdPool, queue,VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sizeof(Vertex) * geometry.vertices.size(), geometry.vertices.data(),
                 &geometry.vertexBuffer, &geometry.vertexMemory);

    if(!geometry.indices.empty()){
        createBuffer(physicalMemoType, device, cmdPool, queue,VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sizeof(uint32_t) * geometry.indices.size(), geometry.indices.data(),
                     &geometry.indexBuffer, &geometry.indexMemory);
    }

    //uniform buffers
    geometry.uniformBuffers.resize(1);
    geometry.uniformMemorys.resize(1);
    geometry.uniformBuffersMapped.resize(1);

    for(uint32_t i = 0; i < 1; i++){
        createBufferInternal(physicalMemoType, device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             sizeof(UniformObject), &geometry.uniformBuffers[i], &geometry.uniformMemorys[i]);

        vkMapMemory(device, geometry.uniformMemorys[i], 0, sizeof(UniformObject), 0, &geometry.uniformBuffersMapped[i]);
    }
}

void VkHelper::geometryDraw(VkCommandBuffer cmdBuffer, VkPipeline graphicPipeline,
                            SwapchainParam swapchainParam, const Geometry& geometry) {
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicPipeline);
    if(geometry.vertexBuffer != VK_NULL_HANDLE) {
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &geometry.vertexBuffer, offsets);
    }
    if(geometry.indexBuffer != VK_NULL_HANDLE && geometry.bUseIndexDraw()){
        VkDeviceSize offsets = 0;
        vkCmdBindIndexBuffer(cmdBuffer, geometry.indexBuffer, offsets, VK_INDEX_TYPE_UINT32);
    }

    VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = static_cast<float>(swapchainParam.extent.width),
            .height = static_cast<float>(swapchainParam.extent.height),
            .minDepth = 0,
            .maxDepth = 1
    };
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    VkRect2D scissor = {
            .offset = {0, 0},
            .extent = {swapchainParam.extent.width, swapchainParam.extent.height}
    };
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

    if(geometry.bUseIndexDraw()){
        vkCmdDrawIndexed(cmdBuffer, geometry.geIndexCount(), 1, 0, 0, 0);
    } else {
        /*
         * commandBuffer 命令缓冲
         * vertexCount 顶点数量
         * instanceCount 实例数量，我们目前只绘制一个实例。当绘制大批量对象时会用到。
         * firstVertex 起始顶点，用于偏移
         * firstInstance 起始实例，用于偏移
         */
        vkCmdDraw(cmdBuffer, geometry.getVertexCount(), 1, 0, 0);
    }
}

void VkHelper::createDescriptorSetLayout(VkDevice device, VkSampler immutableSampler, VkDescriptorSetLayout *out_descriptorSetLayout) {
    VkDescriptorSetLayoutBinding layoutBindings[] = {
            {
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
            },
            {
                    .binding = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .pImmutableSamplers = immutableSampler == VK_NULL_HANDLE ? nullptr : &immutableSampler
            }
    };
    VkDescriptorSetLayoutCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .bindingCount = ARRAY_SIZE(layoutBindings),
            .pBindings = layoutBindings
    };
    CALL_VK(vkCreateDescriptorSetLayout(device, &createInfo, VK_ALLOC, out_descriptorSetLayout));
}

void VkHelper::createDescriptorPool(VkDevice device, VkDescriptorPool *out_descriptorPool) {
    VkDescriptorPoolSize poolSizeInfo[] = {
            {
                    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 2
            },
            {
                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 2
            }
    };
    VkDescriptorPoolCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets = 2,
            .poolSizeCount = ARRAY_SIZE(poolSizeInfo),
            .pPoolSizes = poolSizeInfo,
    };
    CALL_VK(vkCreateDescriptorPool(device, &createInfo, VK_ALLOC, out_descriptorPool));
}

void VkHelper::allocateDescriptorSets(VkDevice device, VkDescriptorPool pool,
                                      VkDescriptorSetLayout layout, VkDescriptorSet *out_descriptorSets) {

    std::vector<VkDescriptorSetLayout> layouts(2, layout);
    VkDescriptorSetAllocateInfo allocateInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = pool,
            .descriptorSetCount = 2,
            .pSetLayouts = layouts.data()
    };
    CALL_VK(vkAllocateDescriptorSets(device, &allocateInfo, out_descriptorSets));
}

void VkHelper::createImage(VkPhysicalDeviceMemoryProperties physicalMemoType, VkDevice device, int width, int height,
                           int depth, VkImageType imageType, VkFormat format, VkSampleCountFlagBits sampleCount,
                           VkImageTiling tiling, VkImageUsageFlags usage,
                           VkImage *out_image, VkDeviceMemory *out_imageMemory) {
    VkImageCreateInfo imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = imageType,
            .format = format,
            .extent = {
                    .width = static_cast<uint32_t>(width),
                    .height = static_cast<uint32_t>(height),
                    .depth = static_cast<uint32_t>(depth)
            },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = sampleCount,
            .tiling = tiling,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    CALL_VK(vkCreateImage(device, &imageCreateInfo, VK_ALLOC, out_image));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, *out_image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VkHelper::findMemoryType(physicalMemoType, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, out_imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(device, *out_image, *out_imageMemory, 0);
}

void VkHelper::createImageView(VkDevice device, VkImage image, VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspectMask, VkImageView *out_imageView) {
    VkImageViewCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = image,
            .viewType = viewType,
            .format = format,
            .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {
                    .aspectMask = aspectMask,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
            }
    };
    CALL_VK(vkCreateImageView(device, &createInfo, VK_ALLOC, out_imageView));
}

void VkHelper::createImageSampler(VkDevice device, VkSampler *out_sampler) {
    VkSamplerCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias = 0.0f,
            .anisotropyEnable = VK_FALSE,//各向异性过滤
            .maxAnisotropy = 1.0f,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE
    };
    CALL_VK(vkCreateSampler(device, &createInfo, VK_ALLOC, out_sampler));
}