#include <android_native_app_glue.h>
#include <unistd.h>
#include "vulkan_wrapper.h"
#include "Common.h"
#include "VkBundle.h"
#include "AndroidCameraPermission.h"
#include "CameraImageReader.h"
#include "CameraManager.h"
#include "VkHelper.h"
#include "VkShaderParam.h"
#include "Geometry.h"
#include "VkCameraImage.h"

// ===================== global vars =================== //
bool isCamPermission;                   // camera permission?
bool isRunning;                         // app is running?
struct android_app *gApp;               // global app
VkBundle vk;                            // vulkan bundle
CameraImageReader *imageReader;         // camera image reader
CameraManager *camLeft;                 // camera 1
CameraManager *camRight;                // camera 2
Geometry geometryLeft;                  // geometry 1
Geometry geometryRight;                 // geometry 2
VkCameraImage *cameraImageLeft;         // image 1
VkCameraImage *cameraImageRight;        // image 2
// ===================== global vars =================== //

std::vector<char> readFileFromAndroidRes(const std::string& filePath){
    AAsset* file = AAssetManager_open(gApp->activity->assetManager, filePath.c_str(), AASSET_MODE_BUFFER);
    size_t fileLength = AAsset_getLength(file);

    std::vector<char> buffer(fileLength);

    AAsset_read(file, buffer.data(), fileLength);
    AAsset_close(file);

    return buffer;
}

void initCamera(){
    if(!isCamPermission){
        AndroidCameraPermission::requestCameraPermission(gApp);
    }

    imageReader = new CameraImageReader(1600, 1600, AIMAGE_FORMAT_PRIVATE, AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE, 4);
    camLeft = new CameraManager(imageReader->getWindow(), 2);
    camLeft->startCapturing();
}

void initCameraImage(){
    AHardwareBuffer *hbLeft;
    while (!(hbLeft = imageReader->getLatestBuffer())){
        usleep(10000u);
    }
    cameraImageLeft = new VkCameraImage(&vk, hbLeft);
}

void createWindow(){
    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .window = gApp->window
    };
    vkCreateAndroidSurfaceKHR(vk.instance, &surfaceCreateInfo, VK_ALLOC, &vk.surface);
    Print("surface: %p", vk.surface);
}

void initGeometry(){
    //left
    geometryLeft.vertices = {
            {{-0.9f, 0.8f, 0.0f},/*vertex*/ {1.0f, 0.0f},/*texcoord*/    {1.0f, 0.0f, 0.0f},/*normal*/},
            {{-0.9f, -0.8f, 0.0f},/*vertex*/ {1.0f, 1.0f},/*texcoord*/    {0.0f, 1.0f, 0.0f},/*normal*/},
            {{-0.1f, -0.8f, 0.0f},/*vertex*/ {0.0f, 1.0f},/*texcoord*/    {0.0f, 0.0f, 1.0f},/*normal*/},
            {{-0.1f, 0.8f, 0.0f},/*vertex*/ {0.0f, 0.0f},/*texcoord*/    {1.0f, 1.0f, 1.0f},/*normal*/},
    };
    geometryLeft.indices = {
            0, 1, 2, 0, 2, 3
    };
    VkHelper::initGeometryBuffers(vk.deviceInfo.physicalDevMemoProps, vk.deviceInfo.device, vk.cmdPool, vk.queueInfo.queue, geometryLeft);

    //right
    geometryRight.vertices = {
            {{0.1f, 0.8f, 0.0f},/*vertex*/ {1.0f, 0.0f},/*texcoord*/    {1.0f, 0.0f, 0.0f},/*normal*/},
            {{0.1f, -0.8f, 0.0f},/*vertex*/ {1.0f, 1.0f},/*texcoord*/    {0.0f, 1.0f, 0.0f},/*normal*/},
            {{0.9f, -0.8f, 0.0f},/*vertex*/ {0.0f, 1.0f},/*texcoord*/    {0.0f, 0.0f, 1.0f},/*normal*/},
            {{0.9f, 0.8f, 0.0f},/*vertex*/ {0.0f, 0.0f},/*texcoord*/    {1.0f, 1.0f, 1.0f},/*normal*/},
    };
    geometryRight.indices = {
            0, 1, 2, 0, 2, 3
    };
    VkHelper::initGeometryBuffers(vk.deviceInfo.physicalDevMemoProps, vk.deviceInfo.device, vk.cmdPool, vk.queueInfo.queue, geometryRight);
}

void updateDescriptorSets(VkCommandBuffer cmdBuffer){

    AHardwareBuffer *hb = imageReader->getLatestBuffer();
    if(nullptr == hb){
        Error("Can not read camera latest buffer!");
        return;
    }
    cameraImageLeft->update(&vk, VK_IMAGE_USAGE_SAMPLED_BIT, VK_SHARING_MODE_EXCLUSIVE, hb);

    VkDescriptorBufferInfo descBufferInfo = {
            .buffer = geometryLeft.uniformBuffers[0],
            .offset = 0,
            .range = sizeof(UniformObject)
    };

    VkDescriptorImageInfo imageInfo1[] = {
            {
                    .sampler = cameraImageLeft->getSampler(),
                    .imageView = cameraImageLeft->getImgView(),
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
    };

//    VkDescriptorImageInfo imageInfo2[] = {
//            {
//                    .sampler = texture2.imageSampler,
//                    .imageView = texture2.imageView,
//                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
//            }
//    };

    VkWriteDescriptorSet writeDescSets[] = {
            {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = vk.descriptorSets[0],
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .pBufferInfo = &descBufferInfo
            },
            {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = vk.descriptorSets[0],
                    .dstBinding = 1,
                    .dstArrayElement = 0,
                    .descriptorCount = ARRAY_SIZE(imageInfo1),
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = imageInfo1
            },
//            {
//                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
//                    .pNext = nullptr,
//                    .dstSet = vk.descriptorSets[0],
//                    .dstBinding = 2,
//                    .dstArrayElement = 0,
//                    .descriptorCount = ARRAY_SIZE(imageInfo2),
//                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//                    .pImageInfo = imageInfo2
//            }
    };
    vkUpdateDescriptorSets(vk.deviceInfo.device, ARRAY_SIZE(writeDescSets), writeDescSets, 0, nullptr);

    VkImageMemoryBarrier camFragBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
    };
    camFragBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    camFragBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    camFragBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL_KHR;
    camFragBarrier.dstQueueFamilyIndex = vk.queueInfo.presentQueueIndex;
    camFragBarrier.image = cameraImageLeft != nullptr ? cameraImageLeft->getImg() : nullptr;
    camFragBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    camFragBarrier.subresourceRange.baseMipLevel = 0;
    camFragBarrier.subresourceRange.levelCount = 1;
    camFragBarrier.subresourceRange.baseArrayLayer = 0;
    camFragBarrier.subresourceRange.layerCount = 1;
    camFragBarrier.srcAccessMask = 0;
    camFragBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         static_cast<VkDependencyFlags>(0), 0, nullptr, 0, nullptr, 1, &camFragBarrier);
}

void initGraphics(){
    if(!InitVulkan()){
        throw std::runtime_error("Failed to init vulkan!");
    }
    VkHelper::createInstance(true, &vk.instance, &vk.debugReport);
    createWindow();
    VkHelper::pickPhyDevAndCreateDev(vk.instance, vk.surface, &vk.deviceInfo, &vk.queueInfo);
    VkHelper::createCommandPool(vk.deviceInfo.device, vk.queueInfo.workQueueIndex, &vk.cmdPool);
    VkHelper::createSwapchain(vk.deviceInfo.physicalDev, vk.deviceInfo.device, vk.surface, vk.queueInfo.workQueueIndex,
                              vk.queueInfo.presentQueueIndex, &vk.swapchainParam,&vk.swapchain);
    CALL_VK(vkGetSwapchainImagesKHR(vk.deviceInfo.device, vk.swapchain, &vk.swapchainImage.imageCount, nullptr));
    vk.swapchainImage.images = static_cast<VkImage *>(malloc(sizeof(VkImage) * vk.swapchainImage.imageCount));
    CALL_VK(vkGetSwapchainImagesKHR(vk.deviceInfo.device, vk.swapchain, &vk.swapchainImage.imageCount, vk.swapchainImage.images));
    vk.swapchainImage.views = static_cast<VkImageView *>(malloc(sizeof(VkImageView) * vk.swapchainImage.imageCount));
    for(uint32_t i = 0; i < vk.swapchainImage.imageCount; i++){
        VkImageViewCreateInfo createInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .image = vk.swapchainImage.images[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = vk.swapchainParam.format.format,
                .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
                .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                }
        };
        CALL_VK(vkCreateImageView(vk.deviceInfo.device, &createInfo, VK_ALLOC, &vk.swapchainImage.views[i]));
    }
    VkHelper::createRenderPass(vk.deviceInfo.device, vk.swapchainParam, &vk.renderPass);
    vk.framebufferCount = vk.swapchainImage.imageCount;
    vk.framebuffers = static_cast<VkFramebuffer *>(malloc(sizeof(VkFramebuffer) * vk.framebufferCount));
    VkHelper::createFramebuffer(vk.deviceInfo.device, vk.renderPass, vk.swapchainParam.extent.width, vk.swapchainParam.extent.height,
                                vk.framebufferCount, vk.swapchainImage.views, vk.framebuffers);
    initCameraImage();
    VkHelper::createDescriptorSetLayout(vk.deviceInfo.device, cameraImageLeft->getSampler(), &vk.descriptorSetLayout);
    VkHelper::createDescriptorPool(vk.deviceInfo.device, &vk.descriptorPool);
    vk.descriptorSets = static_cast<VkDescriptorSet *>(malloc(sizeof(VkDescriptorSet) * 1));
    VkHelper::allocateDescriptorSets(vk.deviceInfo.device, vk.descriptorPool, vk.descriptorSetLayout, vk.descriptorSets);
    VkHelper::createPipelineLayout(vk.deviceInfo.device, vk.descriptorSetLayout, &vk.pipelineLayout);
    auto vertexShaderCode = readFileFromAndroidRes("shaders/demo001.vert.spv");
    auto fragShaderCode = readFileFromAndroidRes("shaders/demo001.frag.spv");
    vk.vertexShaderModule = VkHelper::createShaderModule(vk.deviceInfo.device, vertexShaderCode);
    vk.fragShaderModule = VkHelper::createShaderModule(vk.deviceInfo.device, fragShaderCode);
    VkHelper::createPipeline(vk.deviceInfo.device, vk.pipelineLayout, vk.renderPass,
                             vk.vertexShaderModule, vk.fragShaderModule, vk.swapchainParam, &vk.graphicPipeline);
    vk.cmdBufferCount = vk.swapchainImage.imageCount;
    vk.cmdBuffers = static_cast<VkCommandBuffer *>(malloc(sizeof(VkCommandBuffer) * vk.cmdBufferCount));
    VkHelper::allocateCommandBuffers(vk.deviceInfo.device, vk.cmdPool, vk.cmdBufferCount, vk.cmdBuffers);

    VkSemaphoreCreateInfo semaphoreCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
    };
    CALL_VK(vkCreateSemaphore(vk.deviceInfo.device, &semaphoreCreateInfo, VK_ALLOC, &vk.imageSemaphore));
    CALL_VK(vkCreateSemaphore(vk.deviceInfo.device, &semaphoreCreateInfo, VK_ALLOC, &vk.presentSemaphore));

    initGeometry();
}

void startRender(struct android_app *app){
    gApp = app;
    initCamera();
    initGraphics();

    //change running state
    isRunning = true;
}

void stopRender(){
    isRunning = false;
    vkDeviceWaitIdle(vk.deviceInfo.device);
    geometryRight.destroy(vk.deviceInfo.device);
    geometryLeft.destroy(vk.deviceInfo.device);
    vkFreeDescriptorSets(vk.deviceInfo.device, vk.descriptorPool, 1, vk.descriptorSets);
    free(vk.descriptorSets);
    vkDestroyDescriptorPool(vk.deviceInfo.device, vk.descriptorPool, VK_ALLOC);
    vkDestroyDescriptorSetLayout(vk.deviceInfo.device, vk.descriptorSetLayout, VK_ALLOC);
    vkDestroySemaphore(vk.deviceInfo.device, vk.presentSemaphore, VK_ALLOC);
    vkDestroySemaphore(vk.deviceInfo.device, vk.imageSemaphore, VK_ALLOC);
    vkFreeCommandBuffers(vk.deviceInfo.device, vk.cmdPool, vk.cmdBufferCount, vk.cmdBuffers);
    free(vk.cmdBuffers);
    vkDestroyPipeline(vk.deviceInfo.device, vk.graphicPipeline, VK_ALLOC);
    vkDestroyShaderModule(vk.deviceInfo.device, vk.vertexShaderModule, VK_ALLOC);
    vkDestroyShaderModule(vk.deviceInfo.device, vk.fragShaderModule, VK_ALLOC);
    vkDestroyPipelineLayout(vk.deviceInfo.device, vk.pipelineLayout, VK_ALLOC);
    for (size_t i = 0; i < vk.framebufferCount; i++) {
        vkDestroyFramebuffer(vk.deviceInfo.device, vk.framebuffers[i], VK_ALLOC);
    }
    free(vk.framebuffers);
    for(uint32_t i = 0; i < vk.swapchainImage.imageCount; i++){
        vkDestroyImageView(vk.deviceInfo.device, vk.swapchainImage.views[i], VK_ALLOC);
    }
    free(vk.swapchainImage.views);
    free(vk.swapchainImage.images);
    vkDestroyRenderPass(vk.deviceInfo.device, vk.renderPass, VK_ALLOC);
    vkDestroySwapchainKHR(vk.deviceInfo.device, vk.swapchain, VK_ALLOC);
    vkDestroyCommandPool(vk.deviceInfo.device, vk.cmdPool, VK_ALLOC);
    vkDestroyDevice(vk.deviceInfo.device, VK_ALLOC);
    vkDestroySurfaceKHR(vk.instance, vk.surface, VK_ALLOC);
    vkDestroyInstance(vk.instance, VK_ALLOC);
    cameraImageRight->destroyResources(&vk, true);
    cameraImageLeft->destroyResources(&vk, true);
    SAFE_DELETE(cameraImageRight);
    SAFE_DELETE(cameraImageLeft);
    SAFE_DELETE(camRight);
    SAFE_DELETE(camLeft);
    SAFE_DELETE(imageReader);
}

void processRenderFrame(){
    //Print("processRenderFrame");
    if(!isRunning) return;

    uint32_t imageIndex = 0;
    VkResult rt = vkAcquireNextImageKHR(vk.deviceInfo.device, vk.swapchain, std::numeric_limits<uint64_t>::max(), vk.imageSemaphore, VK_NULL_HANDLE, &imageIndex);

    if(rt == VK_ERROR_OUT_OF_DATE_KHR){
        Error("swapchain was out of date.");
        return;
    }

    VkCommandBufferBeginInfo cmdBufferBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
            .pInheritanceInfo = nullptr
    };
    CALL_VK(vkBeginCommandBuffer(vk.cmdBuffers[imageIndex], &cmdBufferBeginInfo));

    updateDescriptorSets(vk.cmdBuffers[imageIndex]);

    VkClearValue defaultClearValues = { 0.1f,  0.1f,  0.2f,  1.0f};
    VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = nullptr,
            .renderPass = vk.renderPass,
            .framebuffer = vk.framebuffers[imageIndex],
            .renderArea = {
                    .offset = {0, 0},
                    .extent = vk.swapchainParam.extent
            },
            .clearValueCount = 1,
            .pClearValues = &defaultClearValues,
    };

    vkCmdBeginRenderPass(vk.cmdBuffers[imageIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(vk.cmdBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, vk.graphicPipeline);

    if(vk.descriptorSets != VK_NULL_HANDLE){
        vkCmdBindDescriptorSets(vk.cmdBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelineLayout, 0, 1, vk.descriptorSets, 0, nullptr);
    }
    VkHelper::geometryDraw(vk.cmdBuffers[imageIndex], vk.graphicPipeline, vk.swapchainParam, geometryLeft);
    VkHelper::geometryDraw(vk.cmdBuffers[imageIndex], vk.graphicPipeline, vk.swapchainParam, geometryRight);

    vkCmdEndRenderPass(vk.cmdBuffers[imageIndex]);
    CALL_VK(vkEndCommandBuffer(vk.cmdBuffers[imageIndex]));

    VkPipelineStageFlags stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &vk.imageSemaphore,
            .pWaitDstStageMask = stages,
            .commandBufferCount = 1,
            .pCommandBuffers = &vk.cmdBuffers[imageIndex],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &vk.presentSemaphore,
    };
    CALL_VK(vkQueueSubmit(vk.queueInfo.queue, 1, &submitInfo, VK_NULL_HANDLE));

    VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &vk.presentSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &vk.swapchain,
            .pImageIndices = &imageIndex,
            .pResults = nullptr
    };
    CALL_VK(vkQueuePresentKHR(vk.queueInfo.queue, &presentInfo));
    CALL_VK(vkQueueWaitIdle(vk.queueInfo.queue));
}

void cmd_handler(struct android_app *app, int32_t cmd) {
    switch ( cmd ) {
        case APP_CMD_START: {
            Print("onStart()");
            Print("    APP_CMD_START");
            break;
        }
        case APP_CMD_RESUME: {
            Print("onResume()");
            Print("    APP_CMD_RESUME");
            break;
        }
        case APP_CMD_GAINED_FOCUS: {
            Print("onGainedFocus()");
            Print("    APP_CMD_GAINED_FOCUS");
            break;
        }
        case APP_CMD_PAUSE: {
            Print("onPause()");
            Print("    APP_CMD_PAUSE");
            break;
        }
        case APP_CMD_STOP: {
            Print("onStop()");
            Print("    APP_CMD_STOP");
            break;
        }
        case APP_CMD_DESTROY: {
            Print("onDestroy()");
            Print("    APP_CMD_DESTROY");
            break;
        }
        case APP_CMD_INIT_WINDOW: {
            Print("surfaceCreated()");
            Print("    APP_CMD_INIT_WINDOW");
            startRender(app);
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            Print("surfaceDestroyed()");
            Print("    APP_CMD_TERM_WINDOW");
            break;
        }
    }
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events.
 */
void android_main( struct android_app *app)
{
    Print( "----------------------------------------------------------------" );
    Print( "    android_main()" );

    app->onAppCmd = cmd_handler;

    //request camera permission
    AndroidCameraPermission::requestCameraPermission(app);

    int32_t result;
    android_poll_source* source;

    for (;;) {
        while((result = ALooper_pollAll(isRunning ? 0 : -1, nullptr, nullptr,
                                        reinterpret_cast<void**>(&source))) >= 0){
            if(source != nullptr)
                source->process(app, source);

            if(app->destroyRequested)
            {
                Print("Terminating event loop...");
                stopRender();
                return;
            }
        }

        if(isRunning){
            processRenderFrame();
        }
    }
}