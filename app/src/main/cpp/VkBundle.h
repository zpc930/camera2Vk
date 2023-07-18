#pragma once

#include "vulkan_wrapper.h"

struct DeviceInfo {
    VkPhysicalDevice physicalDev;
    VkPhysicalDeviceLimits physicalDevLimits;
    VkPhysicalDeviceMemoryProperties physicalDevMemoProps;
    VkDevice device;
};

struct QueueInfo {
    uint16_t workQueueIndex;
    uint16_t presentQueueIndex;
    VkQueue queue;
};

struct SwapchainImage{
    uint32_t imageCount;
    VkImage *images;
    VkImageView *views;
};

struct SwapchainParam{
    VkSurfaceCapabilitiesKHR capabilities;	//功能
    VkSurfaceFormatKHR format;				//格式
    VkPresentModeKHR presentMode;			//呈现模式
    VkExtent2D extent;						//范围
};

struct VkBundle {
    VkInstance instance;
    VkDebugReportCallbackEXT debugReport;

    VkSurfaceKHR surface;
    struct DeviceInfo deviceInfo;
    struct QueueInfo queueInfo;
    VkCommandPool cmdPool;

    SwapchainParam swapchainParam;
    VkSwapchainKHR swapchain;
    SwapchainImage swapchainImage;

    VkRenderPass renderPass;
    uint32_t framebufferCount;
    VkFramebuffer *framebuffers;

    VkPipelineLayout pipelineLayout;
    VkShaderModule vertexShaderModule;
    VkShaderModule fragShaderModule;
    VkPipeline graphicPipeline;

    uint32_t cmdBufferCount;
    VkCommandBuffer *cmdBuffers;

    VkSemaphore imageSemaphore;
    VkSemaphore presentSemaphore;

    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet *descriptorSets;
};