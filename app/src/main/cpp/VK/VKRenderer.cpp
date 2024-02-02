#include "VKRenderer.h"
#include <string>
#include <android/choreographer.h>
#include "../Common.h"
#include "../Camera/AndroidCameraPermission.h"
#include "../ProfileTrace.h"
#include "vulkan_wrapper.h"
#include "VkHelper.h"

const int64_t gFramePeriodNs = (int64_t)(1e9 / 90);  //90FPS
const float gTimeWarpWaitFramePercentage = 0.5f;
const bool gTimeWarpDelayBetweenEyes = false;
const int gWarpMeshType = 2; //0 = Columns (Left To Right); 1 = Columns (Right To Left); 2 = Rows (Top To Bottom); 3 = Rows (Bottom To Top)
const bool gRenderVst = true;

static uint64_t gLastVsyncTimeNs = 0;
static void VsyncCallback(long frameTimeNanos, void* data) {
    gLastVsyncTimeNs = frameTimeNanos;
}

std::vector<char> VKRenderer::ReadFileFromAndroidRes(const std::string& filePath) {
    AAsset* file = AAssetManager_open(mApp->activity->assetManager, filePath.c_str(), AASSET_MODE_BUFFER);
    size_t fileLength = AAsset_getLength(file);

    std::vector<char> buffer(fileLength);

    AAsset_read(file, buffer.data(), fileLength);
    AAsset_close(file);

    return buffer;
}

void VKRenderer::Init(struct android_app *app) {
    mApp = app;

    OpenCameras();
    InitVKEnv();
    mImageLeft = new VkCameraImageV2(&mVk);
    mImageRight = new VkCameraImageV2(&mVk);
    bRunning = true;
}

void VKRenderer::Destroy() {
    bRunning = false;
    SAFE_DELETE(mImageLeft);
    SAFE_DELETE(mImageRight);
    DestroyVKEnv();
    CloseCameras();
}

bool VKRenderer::IsRunning() {
    return bRunning;
}

void VKRenderer::ProcessFrame(uint64_t frameIndex) {
    TRACE_BEGIN("ProcessFrame:%lu", frameIndex);
    AChoreographer *grapher = AChoreographer_getInstance();
    AChoreographer_postFrameCallback(grapher, VsyncCallback, nullptr);
    uint64_t startTimeNs = getTimeNano(CLOCK_MONOTONIC);
    mLastVsyncTimeNs = gLastVsyncTimeNs;
    if (mLastVsyncTimeNs > startTimeNs) {
        LOG_W("%lu: verify that the qvr timestamp is actually in the past...", startTimeNs);
        mLastVsyncTimeNs = startTimeNs;
    }
    if ((startTimeNs - mLastVsyncTimeNs) > gFramePeriodNs) {
        //We've managed to get here prior to the next vsync occuring so set it based on the previous (interrupt may have been delayed)
        LOG_W("%lu:Adding frame period time (%0.2f) to vsync time, Last VSync was %0.4f ms ago!", frameIndex,
              gFramePeriodNs * 1.f / U_TIME_1MS_IN_NS, (startTimeNs - mLastVsyncTimeNs) * 1.f / U_TIME_1MS_IN_NS);
        mLastVsyncTimeNs = mLastVsyncTimeNs + gFramePeriodNs;
    }
    mVsyncCount++;
    int64_t vsyncDiffTimeNs = startTimeNs - mLastVsyncTimeNs;
    double framePct = (double)mVsyncCount + ((double)vsyncDiffTimeNs) / (gFramePeriodNs);
    double fractFrame = framePct - ((long)framePct);

    int64_t waitTimeNs = 0;
    if (fractFrame < gTimeWarpWaitFramePercentage) {
        //We are currently in the first half of the display so wait until we hit the halfway point
        waitTimeNs = (int64_t)((gTimeWarpWaitFramePercentage - fractFrame) * gFramePeriodNs);
    } else {
        //We are currently past the halfway point in the display so wait until halfway through the next vsync cycle
        waitTimeNs = (int64_t)((0.9 + gTimeWarpWaitFramePercentage - fractFrame) * gFramePeriodNs);
        LOG_W("%lu: jank...", frameIndex);
    }
    LOG_D("%lu: Left EyeBuffer Wait : %.2f ms, Vsync diff : %.2f ms, Frame diff : %.2f ms, [%lu: %.2f, %.2f]", frameIndex, waitTimeNs * 1.f / U_TIME_1MS_IN_NS,
          vsyncDiffTimeNs * 1.f / U_TIME_1MS_IN_NS, (startTimeNs - mLastFrameTime) * 1.f / U_TIME_1MS_IN_NS, mVsyncCount, framePct, fractFrame);
    mLastFrameTime = startTimeNs;
    TRACE_BEGIN("Left wait:%.1f:%.1f", waitTimeNs * 1.f / U_TIME_1MS_IN_NS, vsyncDiffTimeNs * 1.f / U_TIME_1MS_IN_NS);
    NanoSleep(waitTimeNs);
    TRACE_END("Left wait:%.1f:%.1f", waitTimeNs * 1.f / U_TIME_1MS_IN_NS, vsyncDiffTimeNs * 1.f / U_TIME_1MS_IN_NS);
    uint64_t postLeftWaitTimeStamp = getTimeNano(CLOCK_MONOTONIC);

    RenderMeshOrder gMeshOrderEnum = MeshOrderLeftToRight;
    if (gWarpMeshType == 0) {
        gMeshOrderEnum = MeshOrderLeftToRight;
    } else if (gWarpMeshType == 1) {
        gMeshOrderEnum = MeshOrderRightToLeft;
    } else if (gWarpMeshType == 2) {
        gMeshOrderEnum = MeshOrderTopToBottom;
    } else if (gWarpMeshType == 3) {
        gMeshOrderEnum = MeshOrderBottomToTop;
    }

    AImage *imageLeft = mImageReaderLeft->getLatestImage();
    if(!imageLeft){
        return;
    }
    AImage *imageRight = mImageReaderRight->getLatestImage();
    if(!imageRight){
        return;
    }

    VkResult rt = vkAcquireNextImageKHR(mVk.deviceInfo.device, mVk.swapchain, std::numeric_limits<uint64_t>::max(), mVk.imageSemaphore, VK_NULL_HANDLE, &mCurrentImageIndex);
    LOG_D("image index: %d", mCurrentImageIndex);

    if(rt == VK_ERROR_OUT_OF_DATE_KHR){
        LOG_E("swapchain was out of date.");
        return;
    }

    TRACE_BEGIN("UpdateDescriptorSets");
    UpdateDescriptorSets(0, imageLeft);
    UpdateDescriptorSets(1, imageRight);
    TRACE_END("UpdateDescriptorSets");

    TRACE_BEGIN("First Render");
    switch (gMeshOrderEnum) {
        case MeshOrderLeftToRight:
            RenderSubArea(imageLeft, MeshLeft);
            break;

        case MeshOrderRightToLeft:
            RenderSubArea(imageRight, MeshRight);
            break;

        case MeshOrderTopToBottom:
            RenderSubArea(imageLeft, MeshUpperLeft);
            RenderSubArea(imageRight, MeshUpperRight);
            break;

        case MeshOrderBottomToTop:
            RenderSubArea(imageLeft, MeshLowerLeft);
            RenderSubArea(imageRight, MeshLowerRight);
            break;
    }
#ifdef RENDER_USE_SINGLE_BUFFER
    VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &mVk.presentSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &mVk.swapchain,
            .pImageIndices = &mCurrentImageIndex,
            .pResults = nullptr
    };
    CALL_VK(vkQueuePresentKHR(mVk.queueInfo.queue, &presentInfo));
#endif
    TRACE_END("First Render");

    if (gTimeWarpDelayBetweenEyes) {
        uint64_t rightTimestamp = getTimeNano(CLOCK_MONOTONIC);
        //We started the left eye half way through vsync, figure out how long
        //we have left in the half frame until the raster starts over so we can render the right eye
        uint64_t delta = rightTimestamp - postLeftWaitTimeStamp;
        uint64_t waitTimeNs = 0;
        if (delta < ((uint64_t)(gFramePeriodNs / 2.0))) {
            waitTimeNs = ((uint64_t)(gFramePeriodNs / 2.0)) - delta;
            LOG_D("%lu: Right EyeBuffer Wait : %.2f ms, Left take : %.2f ms", frameIndex, waitTimeNs * 1.f / U_TIME_1MS_IN_NS, delta * 1.f / U_TIME_1MS_IN_NS);
            TRACE_BEGIN("Right wait:%.1f:%.1f", waitTimeNs * 1.f / U_TIME_1MS_IN_NS, vsyncDiffTimeNs * 1.f / U_TIME_1MS_IN_NS);
            NanoSleep(waitTimeNs + gFramePeriodNs / 8);
            TRACE_END("Right wait:%.1f:%.1f", waitTimeNs * 1.f / U_TIME_1MS_IN_NS, vsyncDiffTimeNs * 1.f / U_TIME_1MS_IN_NS);
        } else {
            //The left eye took longer than 1/2 the refresh so the raster has already wrapped around and is
            //in the left half of the screen.  Skip the wait and get right on rendering the right eye.
            LOG_D("Left Eye took too long!!! ( %.2f ms )", delta * 1.f / U_TIME_1MS_IN_NS);
        }
    }

    TRACE_BEGIN("Second Render");
    switch (gMeshOrderEnum) {
        case MeshOrderLeftToRight:
            RenderSubArea(imageRight, MeshRight);
            break;

        case MeshOrderRightToLeft:
            RenderSubArea(imageLeft, MeshLeft);
            break;

        case MeshOrderTopToBottom:
            RenderSubArea(imageLeft, MeshLowerLeft);
            RenderSubArea(imageRight, MeshLowerRight);
            break;

        case MeshOrderBottomToTop:
            RenderSubArea(imageLeft, MeshUpperLeft);
            RenderSubArea(imageRight, MeshUpperRight);
            break;
    }
#ifdef RENDER_USE_SINGLE_BUFFER
    CALL_VK(vkQueuePresentKHR(mVk.queueInfo.queue, &presentInfo));
#else
    VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &mVk.presentSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &mVk.swapchain,
            .pImageIndices = &mCurrentImageIndex,
            .pResults = nullptr
    };
    CALL_VK(vkQueuePresentKHR(mVk.queueInfo.queue, &presentInfo));
    CALL_VK(vkQueueWaitIdle(mVk.queueInfo.queue));
#endif
    TRACE_END("Second Render");
    TRACE_END("ProcessFrame:%lu", frameIndex);
}

void VKRenderer::OpenCameras() {
    if(!AndroidCameraPermission::isCameraPermitted(mApp)){
        AndroidCameraPermission::requestCameraPermission(mApp);
    }
    mImageReaderLeft = new CameraImageReader(1920, 1440, AIMAGE_FORMAT_YUV_420_888, 4);
    mCameraLeft = new CameraManager(mImageReaderLeft->getWindow(), 2);
    mCameraLeft->startCapturing();

    mImageReaderRight= new CameraImageReader(1920, 1440, AIMAGE_FORMAT_YUV_420_888, 4);
    mCameraRight = new CameraManager(mImageReaderRight->getWindow(), 3);
    mCameraRight->startCapturing();
}

void VKRenderer::CloseCameras() {
    mCameraLeft->stopCapturing();
    mCameraRight->stopCapturing();
    SAFE_DELETE(mImageReaderLeft);
    SAFE_DELETE(mImageReaderRight);
    SAFE_DELETE(mCameraLeft);
    SAFE_DELETE(mCameraRight);
}

int VKRenderer::InitVKEnv() {
    if(!InitVulkan()){
        throw std::runtime_error("Failed to init vulkan!");
    }
    VkHelper::createInstance(true, &mVk.instance, &mVk.debugReport);
    CreateWindowSurface();
    VkHelper::pickPhyDevAndCreateDev(mVk.instance, mVk.surface, &mVk.deviceInfo, &mVk.queueInfo);
    VkHelper::createCommandPool(mVk.deviceInfo.device, mVk.queueInfo.workQueueIndex, &mVk.cmdPool);
    VkHelper::createSwapchain(mVk.deviceInfo.physicalDev, mVk.deviceInfo.device, mVk.surface, mVk.queueInfo.workQueueIndex, mVk.queueInfo.presentQueueIndex, &mVk.swapchainParam,&mVk.swapchain);
    CALL_VK(vkGetSwapchainImagesKHR(mVk.deviceInfo.device, mVk.swapchain, &mVk.swapchainImage.imageCount, nullptr));
    mVk.swapchainImage.images = static_cast<VkImage *>(malloc(sizeof(VkImage) * mVk.swapchainImage.imageCount));
    CALL_VK(vkGetSwapchainImagesKHR(mVk.deviceInfo.device, mVk.swapchain, &mVk.swapchainImage.imageCount, mVk.swapchainImage.images));
    mVk.swapchainImage.views = static_cast<VkImageView *>(malloc(sizeof(VkImageView) * mVk.swapchainImage.imageCount));
    for(uint32_t i = 0; i < mVk.swapchainImage.imageCount; i++){
        VkImageViewCreateInfo createInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .image = mVk.swapchainImage.images[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = mVk.swapchainParam.format.format,
                .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
                .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                }
        };
        CALL_VK(vkCreateImageView(mVk.deviceInfo.device, &createInfo, VK_ALLOC, &mVk.swapchainImage.views[i]));
    }
    VkHelper::createRenderPass(mVk.deviceInfo.device, mVk.swapchainParam, &mVk.renderPass);
    mVk.framebufferCount = mVk.swapchainImage.imageCount;
    mVk.framebuffers = static_cast<VkFramebuffer *>(malloc(sizeof(VkFramebuffer) * mVk.framebufferCount));
    VkHelper::createFramebuffer(mVk.deviceInfo.device, mVk.renderPass, mVk.swapchainParam.extent.width, mVk.swapchainParam.extent.height,
                                mVk.framebufferCount, mVk.swapchainImage.views, mVk.framebuffers);
    VkHelper::createDescriptorSetLayout(mVk.deviceInfo.device, nullptr, &mVk.descriptorSetLayout);
    VkHelper::createDescriptorPool(mVk.deviceInfo.device, &mVk.descriptorPool);
    mVk.descriptorSets = static_cast<VkDescriptorSet *>(malloc(sizeof(VkDescriptorSet) * 2));
    VkHelper::allocateDescriptorSets(mVk.deviceInfo.device, mVk.descriptorPool, mVk.descriptorSetLayout, mVk.descriptorSets);
    VkHelper::createPipelineLayout(mVk.deviceInfo.device, mVk.descriptorSetLayout, &mVk.pipelineLayout);
    auto vertexShaderCode = ReadFileFromAndroidRes("shaders/demo001.vert.spv");
    auto fragShaderCode = ReadFileFromAndroidRes("shaders/demo001.frag.spv");
    mVk.vertexShaderModule = VkHelper::createShaderModule(mVk.deviceInfo.device, vertexShaderCode);
    mVk.fragShaderModule = VkHelper::createShaderModule(mVk.deviceInfo.device, fragShaderCode);
    VkHelper::createPipeline(mVk.deviceInfo.device, mVk.pipelineLayout, mVk.renderPass,
                             mVk.vertexShaderModule, mVk.fragShaderModule, mVk.swapchainParam, &mVk.graphicPipeline);
    mVk.cmdBufferCount = 2;
    mVk.cmdBuffers = static_cast<VkCommandBuffer *>(malloc(sizeof(VkCommandBuffer) * mVk.cmdBufferCount));
    VkHelper::allocateCommandBuffers(mVk.deviceInfo.device, mVk.cmdPool, mVk.cmdBufferCount, mVk.cmdBuffers);

    VkSemaphoreCreateInfo semaphoreCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
    };
    CALL_VK(vkCreateSemaphore(mVk.deviceInfo.device, &semaphoreCreateInfo, VK_ALLOC, &mVk.imageSemaphore));
    CALL_VK(vkCreateSemaphore(mVk.deviceInfo.device, &semaphoreCreateInfo, VK_ALLOC, &mVk.presentSemaphore));

    InitGeometry();
    return 1;
}

void VKRenderer::DestroyVKEnv() {
    vkDeviceWaitIdle(mVk.deviceInfo.device);
    mGeometryLeft.destroy(mVk.deviceInfo.device);
    mGeometryRight.destroy(mVk.deviceInfo.device);
    vkFreeDescriptorSets(mVk.deviceInfo.device, mVk.descriptorPool, 1, mVk.descriptorSets);
    free(mVk.descriptorSets);
    vkDestroyDescriptorPool(mVk.deviceInfo.device, mVk.descriptorPool, VK_ALLOC);
    vkDestroyDescriptorSetLayout(mVk.deviceInfo.device, mVk.descriptorSetLayout, VK_ALLOC);
    vkDestroySemaphore(mVk.deviceInfo.device, mVk.presentSemaphore, VK_ALLOC);
    vkDestroySemaphore(mVk.deviceInfo.device, mVk.imageSemaphore, VK_ALLOC);
    vkFreeCommandBuffers(mVk.deviceInfo.device, mVk.cmdPool, mVk.cmdBufferCount, mVk.cmdBuffers);
    free(mVk.cmdBuffers);
    vkDestroyPipeline(mVk.deviceInfo.device, mVk.graphicPipeline, VK_ALLOC);
    vkDestroyShaderModule(mVk.deviceInfo.device, mVk.vertexShaderModule, VK_ALLOC);
    vkDestroyShaderModule(mVk.deviceInfo.device, mVk.fragShaderModule, VK_ALLOC);
    vkDestroyPipelineLayout(mVk.deviceInfo.device, mVk.pipelineLayout, VK_ALLOC);
    for (size_t i = 0; i < mVk.framebufferCount; i++) {
        vkDestroyFramebuffer(mVk.deviceInfo.device, mVk.framebuffers[i], VK_ALLOC);
    }
    free(mVk.framebuffers);
    for(uint32_t i = 0; i < mVk.swapchainImage.imageCount; i++){
        vkDestroyImageView(mVk.deviceInfo.device, mVk.swapchainImage.views[i], VK_ALLOC);
    }
    free(mVk.swapchainImage.views);
    free(mVk.swapchainImage.images);
    vkDestroyRenderPass(mVk.deviceInfo.device, mVk.renderPass, VK_ALLOC);
    vkDestroySwapchainKHR(mVk.deviceInfo.device, mVk.swapchain, VK_ALLOC);
    vkDestroyCommandPool(mVk.deviceInfo.device, mVk.cmdPool, VK_ALLOC);
    vkDestroyDevice(mVk.deviceInfo.device, VK_ALLOC);
    vkDestroySurfaceKHR(mVk.instance, mVk.surface, VK_ALLOC);
    vkDestroyInstance(mVk.instance, VK_ALLOC);
}

void VKRenderer::RenderSubArea(const AImage *image, RenderMeshArea area) {
    int eyeIndex = 0;
    if(area == MeshLeft | area == MeshUpperLeft | area == MeshLowerLeft){
        eyeIndex = 0;
    } else {
        eyeIndex = 1;
    }
    VkCommandBuffer cmdBuffer = mVk.cmdBuffers[eyeIndex];
    VkCommandBufferBeginInfo cmdBufferBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
            .pInheritanceInfo = nullptr
    };
    CALL_VK(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo));

    uint32_t surfaceWidth = mVk.swapchainParam.extent.width;
    uint32_t surfaceHeight = mVk.swapchainParam.extent.height;
    VkClearValue defaultClearValues = { 0.1f,  0.2f,  0.3f,  1.0f};
    VkRect2D renderArea = { 0, 0, mVk.swapchainParam.extent };
    VkRect2D scissor = { 0, 0, surfaceWidth, surfaceHeight };
    switch (area) {
        case MeshLeft:
            defaultClearValues = { 1.f, 0.f, 0.f, 1.f };
            renderArea = { 0, 0, surfaceWidth / 2, surfaceHeight };
            scissor = { 0, 0, surfaceWidth / 2, surfaceHeight };
            break;
        case MeshRight:
            defaultClearValues = { 0.f, 1.f, 0.f, 1.f };
            renderArea = { static_cast<int32_t>(surfaceWidth / 2), 0, surfaceWidth / 2, surfaceHeight };
            scissor = { static_cast<int32_t>(surfaceWidth / 2), 0, surfaceWidth / 2, surfaceHeight };
            break;
        case MeshUpperLeft:
            defaultClearValues = { 1.f, 0.f, 0.f, 1.f };
            renderArea = { 0, 0, surfaceWidth / 2, surfaceHeight / 2 };
            scissor = { 0, 0, surfaceWidth / 2, surfaceHeight / 2 };
            break;
        case MeshUpperRight:
            defaultClearValues = { 0.f, 1.f, 0.f, 1.f };
            renderArea = { static_cast<int32_t>(surfaceWidth / 2), 0, surfaceWidth / 2, surfaceHeight / 2 };
            scissor = { static_cast<int32_t>(surfaceWidth / 2), 0, surfaceWidth / 2, surfaceHeight / 2 };
            break;
        case MeshLowerLeft:
            defaultClearValues = { 1.f, 1.f, 0.f, 1.f };
            renderArea = { 0, static_cast<int32_t>(surfaceHeight / 2), surfaceWidth / 2, surfaceHeight / 2 };
            scissor = { 0, static_cast<int32_t>(surfaceHeight / 2), surfaceWidth / 2, surfaceHeight / 2 };
            break;
        case MeshLowerRight:
            defaultClearValues = { 0.f, 1.f, 1.f, 1.f };
            renderArea = { static_cast<int32_t>(surfaceWidth / 2), static_cast<int32_t>(surfaceHeight / 2), surfaceWidth / 2, surfaceHeight / 2 };
            scissor = { static_cast<int32_t>(surfaceWidth / 2), static_cast<int32_t>(surfaceHeight / 2), surfaceWidth / 2, surfaceHeight / 2 };
            break;
    }
    VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = nullptr,
            .renderPass = mVk.renderPass,
            .framebuffer = mVk.framebuffers[mCurrentImageIndex],
            .renderArea = renderArea,
            .clearValueCount = 1,
            .pClearValues = &defaultClearValues,
    };
    vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

    if(gRenderVst){
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVk.graphicPipeline);
        if(mVk.descriptorSets != VK_NULL_HANDLE){
            vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVk.pipelineLayout, 0, 1, &mVk.descriptorSets[eyeIndex], 0, nullptr);
        }
        VkHelper::geometryDraw(cmdBuffer, mVk.graphicPipeline, mVk.swapchainParam, eyeIndex == 0 ? mGeometryLeft : mGeometryRight);
    }

    vkCmdEndRenderPass(cmdBuffer);
    CALL_VK(vkEndCommandBuffer(cmdBuffer));

    VkPipelineStageFlags stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &mVk.imageSemaphore,
            .pWaitDstStageMask = stages,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmdBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &mVk.presentSemaphore,
    };
    CALL_VK(vkQueueSubmit(mVk.queueInfo.queue, 1, &submitInfo, VK_NULL_HANDLE));
}

void VKRenderer::CreateWindowSurface(){
    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .window = mApp->window
    };
    vkCreateAndroidSurfaceKHR(mVk.instance, &surfaceCreateInfo, VK_ALLOC, &mVk.surface);
    LOG_D("surface: %p", mVk.surface);
}

void VKRenderer::InitGeometry(){
    //left
    mGeometryLeft.vertices = {
            {{-0.95f, 0.95f},/*vertex*/ {0.0f, 0.0f},/*texcoord*/  },
            {{-0.95f, -0.95f},/*vertex*/ {0.0f, 1.0f},/*texcoord*/ },
            {{-0.05f, 0.95f},/*vertex*/ {1.0f, 0.0f},/*texcoord*/  },
            {{-0.05f, -0.95f},/*vertex*/ {1.0f, 1.0f},/*texcoord*/ },
    };
    VkHelper::initGeometryBuffers(mVk.deviceInfo.physicalDevMemoProps, mVk.deviceInfo.device, mVk.cmdPool, mVk.queueInfo.queue, mGeometryLeft);

    //right
    mGeometryRight.vertices = {
            {{0.05f, 0.95f},/*vertex*/ {0.0f, 0.0f},/*texcoord*/  },
            {{0.05f, -0.95f},/*vertex*/ {0.0f, 1.0f},/*texcoord*/ },
            {{0.95f, 0.95f},/*vertex*/ {1.0f, 0.0f},/*texcoord*/ },
            {{0.95f, -0.95f},/*vertex*/ {1.0f, 1.0f},/*texcoord*/  },
    };
    VkHelper::initGeometryBuffers(mVk.deviceInfo.physicalDevMemoProps, mVk.deviceInfo.device, mVk.cmdPool, mVk.queueInfo.queue, mGeometryRight);
}

void VKRenderer::UpdateDescriptorSets(uint8_t eyeIndex, const AImage *image){
    VkCameraImageV2 *cameraImage = nullptr;
    if(eyeIndex == 0){
        cameraImage = mImageLeft;
    } else {
        cameraImage = mImageRight;
    }
    cameraImage->updateImg(eyeIndex, image);

    VkDescriptorImageInfo imageInfoY = {
            .sampler = cameraImage->getSampler(eyeIndex, PLANE_Y),
            .imageView = cameraImage->getImgView(eyeIndex, PLANE_Y),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkDescriptorImageInfo imageInfoUV = {
            .sampler = cameraImage->getSampler(eyeIndex, PLANE_UV),
            .imageView = cameraImage->getImgView(eyeIndex, PLANE_UV),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkWriteDescriptorSet writeDescSets[] = {
            {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = mVk.descriptorSets[eyeIndex],
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &imageInfoY
            },
            {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = mVk.descriptorSets[eyeIndex],
                    .dstBinding = 1,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &imageInfoUV
            }
    };
    vkUpdateDescriptorSets(mVk.deviceInfo.device, ARRAY_SIZE(writeDescSets), writeDescSets, 0, nullptr);
}