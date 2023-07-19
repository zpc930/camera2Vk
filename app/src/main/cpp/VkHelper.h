#ifndef LEARN_VULKAN_VKHELPER_H
#define LEARN_VULKAN_VKHELPER_H

#include "VulkanCommon.h"
#include "VkBundle.h"
#include "Geometry.h"

enum class MemoryLocation
{
    DEVICE,
    HOST,
    EXTERNAL
};

class VkHelper {
public:
    static void createInstance(bool bValidate, VkInstance *out_instance, VkDebugReportCallbackEXT *out_debugReport);
    static void pickPhyDevAndCreateDev(VkInstance instance, VkSurfaceKHR surface, DeviceInfo *out_deviceInfo, QueueInfo *out_queueInfo);
    static void createCommandPool(VkDevice device, uint32_t workQueueIndex, VkCommandPool *out_cmdPool);
    static void createSwapchain(VkPhysicalDevice physicalDev, VkDevice device, VkSurfaceKHR surface, uint16_t workQueueIndex, uint16_t presentQueueIndex,
                                SwapchainParam *out_swapchainParam, VkSwapchainKHR *out_swapchain);
    static void createRenderPass(VkDevice device, SwapchainParam swapchainParam, VkRenderPass *out_renderPass);
    static void createFramebuffer(VkDevice device, VkRenderPass renderPass, uint32_t width, uint32_t height,
                                     uint32_t framebufferCount, VkImageView *imageViews, VkFramebuffer *out_framebuffers);
    static void createPipelineLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, VkPipelineLayout *out_pipelineLayout);
    static VkShaderModule createShaderModule(VkDevice device, std::vector<char> &code);
    static void createPipeline(VkDevice device, VkPipelineLayout pipelineLayout, VkRenderPass renderPass,
                                  VkShaderModule vertShaderModule, VkShaderModule fragShaderModule, SwapchainParam swapchainParam,
                                  VkPipeline *out_pipeline);
    static void allocateCommandBuffers(VkDevice device, VkCommandPool cmdPool, uint32_t cmdBufferCount, VkCommandBuffer *cmdBuffers);
    static void printPhysicalDevLog(const VkPhysicalDeviceProperties &devProps);
    static void beginCommandBuffer(VkCommandBuffer cmdBuffer, bool oneTime);
    static void endCommandBuffer(VkCommandBuffer cmdBuffer, VkDevice device, VkCommandPool cmdPool, VkQueue queue, bool bFree);
    static void createBuffer(VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer *outBuffer);
    static uint32_t findMemoryType(VkPhysicalDeviceMemoryProperties phyProperties, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    static uint32_t getMemoryIndex(VkPhysicalDeviceMemoryProperties phyProperties, uint32_t memoryTypeBits, MemoryLocation location);
    static void copyBuffer(VkDevice device, VkCommandPool cmdPool, VkQueue queue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    static void createBufferInternal(VkPhysicalDeviceMemoryProperties physicalMemoType, VkDevice device,
                                        VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps, VkDeviceSize size,
                                        VkBuffer *outBuffer, VkDeviceMemory *outMemory);
    static void createBuffer(VkPhysicalDeviceMemoryProperties physicalMemoType, VkDevice device, VkCommandPool cmdPool,
                                VkQueue queue, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps,
                                uint32_t dataSize, const void *data, VkBuffer *outBuffer,
                                VkDeviceMemory *outMemory);
    static void initGeometryBuffers(VkPhysicalDeviceMemoryProperties physicalMemoType, VkDevice device,
                                       VkCommandPool cmdPool, VkQueue queue, Geometry &geometry);
    static void geometryDraw(VkCommandBuffer cmdBuffer, VkPipeline graphicPipeline, SwapchainParam swapchainParam, const Geometry& geometry);
    static void createDescriptorSetLayout(VkDevice device, VkSampler immutableSampler, VkDescriptorSetLayout *out_descriptorSetLayout);
    static void createDescriptorPool(VkDevice device, VkDescriptorPool *out_descriptorPool);
    static void allocateDescriptorSets(VkDevice device, VkDescriptorPool pool,
                                          VkDescriptorSetLayout layout, VkDescriptorSet *out_descriptorSets);
    static void createImage(VkPhysicalDeviceMemoryProperties physicalMemoType, VkDevice device, int width, int height,
                               int depth, VkImageType imageType, VkFormat format, VkSampleCountFlagBits sampleCount,
                               VkImageTiling tiling, VkImageUsageFlags usage,
                               VkImage *out_image, VkDeviceMemory *out_imageMemory);
    static void createImageView(VkDevice device, VkImage image, VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspectMask, VkImageView *out_imageView);
    static void createImageSampler(VkDevice device, VkSampler *out_sampler);
};

#endif //LEARN_VULKAN_VKHELPER_H
