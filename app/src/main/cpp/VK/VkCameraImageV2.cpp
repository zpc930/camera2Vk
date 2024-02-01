#include "VkCameraImageV2.h"
#include "VkHelper.h"

#define IMAGE_WIDTH 1920
#define IMAGE_HEIGHT 1440

VkCameraImageV2::VkCameraImageV2(VkBundle *vk){
    this->mImgBuffer = VK_NULL_HANDLE;
    this->mImgBufferMemory = VK_NULL_HANDLE;
    mVkBundle = vk;
    init();
}

VkCameraImageV2::~VkCameraImageV2() {
    destroyImgs();
}

void VkCameraImageV2::init() {
    size_t dataSize = IMAGE_WIDTH * IMAGE_HEIGHT * 3 / 2;
    VkHelper::createBuffer(mVkBundle->deviceInfo.physicalDevMemoProps, mVkBundle->deviceInfo.device, mVkBundle->cmdPool, mVkBundle->queueInfo.queue,
                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, dataSize, nullptr, &mImgBuffer, &mImgBufferMemory);
    VkCommandBuffer cmdBuffer;
    VkHelper::allocateCommandBuffers(mVkBundle->deviceInfo.device, mVkBundle->cmdPool, 1, &cmdBuffer);
    VkHelper::beginCommandBuffer(cmdBuffer, true);
    for(auto & cameraImage : mCameraImages) {
        //because YUV 4:2:0, 4Y->1UV, 8+2+2 = 12bits --> 1.5byte, so multi 1.5
        cameraImage.mDataSize = dataSize;

        // y plane
        initImgs(VK_FORMAT_R8_UNORM, IMAGE_WIDTH, IMAGE_HEIGHT, cmdBuffer,
                 &cameraImage.yImg.mImg, &cameraImage.yImg.mMemory,
                 &cameraImage.yImg.mImgView, &cameraImage.yImg.mSampler);

        // uv plane
        initImgs(VK_FORMAT_R8G8_UNORM, IMAGE_WIDTH / 2, IMAGE_HEIGHT / 2, cmdBuffer,
                 &cameraImage.uvImg.mImg, &cameraImage.uvImg.mMemory,
                 &cameraImage.uvImg.mImgView, &cameraImage.uvImg.mSampler);
    }
    VkHelper::endCommandBuffer(cmdBuffer, mVkBundle->deviceInfo.device, mVkBundle->cmdPool, mVkBundle->queueInfo.queue, true);
}

void VkCameraImageV2::initImgs(VkFormat format, uint32_t width, uint32_t height, VkCommandBuffer cmdBuffer,
                           VkImage *outImg, VkDeviceMemory *outMemory, VkImageView *outImgView, VkSampler *outSampler) {
    //image
    VkImageCreateInfo imageInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = VkExtent3D{ width, height, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_LINEAR,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    CALL_VK(vkCreateImage(mVkBundle->deviceInfo.device, &imageInfo, VK_ALLOC, outImg));

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(mVkBundle->deviceInfo.device, *outImg, &memReqs);

    VkMemoryAllocateInfo memoryAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memReqs.size,
            .memoryTypeIndex = VkHelper::findMemoryType(mVkBundle->deviceInfo.physicalDevMemoProps, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    CALL_VK(vkAllocateMemory(mVkBundle->deviceInfo.device, &memoryAllocateInfo, VK_ALLOC, outMemory));
    vkBindImageMemory(mVkBundle->deviceInfo.device, *outImg, *outMemory, 0);

    VkHelper::transition_image_layout(*outImg, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmdBuffer);

    // view
    VkImageViewCreateInfo imgViewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = *outImg,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .components = {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
            }
    };
    CALL_VK(vkCreateImageView(mVkBundle->deviceInfo.device, &imgViewInfo, VK_ALLOC, outImgView));

    //create sampler
    VkSamplerCreateInfo samplerCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_NEAREST,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .mipLodBias = 0.0f,
            .anisotropyEnable = VK_FALSE,
            .maxAnisotropy = 1.0f,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_NEVER,
            .minLod = 0.0f,
            .maxLod = 1.0f,
            .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            .unnormalizedCoordinates = VK_FALSE
    };
    CALL_VK(vkCreateSampler(mVkBundle->deviceInfo.device, &samplerCreateInfo, VK_ALLOC, outSampler));
}

void VkCameraImageV2::updateImg(uint64_t frameIndex, uint32_t eyeIndex, const AImage *image) {
    if(!image){
        return;
    }
    VkCameraImage cameraImage = mCameraImages[eyeIndex];
    //readHardwareBuffer(mVkBundle, frameIndex, eyeIndex, buf, cameraImage.mDataSize, 0, mImgBufferMemory);

    VkCommandBuffer cmdBuffer;
    VkHelper::allocateCommandBuffers(mVkBundle->deviceInfo.device, mVkBundle->cmdPool, 1, &cmdBuffer);
    VkHelper::beginCommandBuffer(cmdBuffer, true);
    VkHelper::transition_image_layout(cameraImage.yImg.mImg, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cmdBuffer);
    VkHelper::transition_image_layout(cameraImage.uvImg.mImg, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cmdBuffer);
    VkBufferImageCopy bufferCopyRegions ={
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
            },
            .imageOffset = {0, 0, 0},
            .imageExtent = { IMAGE_WIDTH, IMAGE_HEIGHT, 1},
    };
    vkCmdCopyBufferToImage(cmdBuffer, mImgBuffer, cameraImage.yImg.mImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegions);
    bufferCopyRegions.bufferOffset = IMAGE_WIDTH * IMAGE_HEIGHT;
    bufferCopyRegions.imageExtent = { IMAGE_WIDTH / 2, IMAGE_HEIGHT / 2, 1 };
    vkCmdCopyBufferToImage(cmdBuffer, mImgBuffer, cameraImage.uvImg.mImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegions);

    VkHelper::transition_image_layout(cameraImage.yImg.mImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmdBuffer);
    VkHelper::transition_image_layout(cameraImage.uvImg.mImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmdBuffer);
    VkHelper::endCommandBuffer(cmdBuffer, mVkBundle->deviceInfo.device, mVkBundle->cmdPool, mVkBundle->queueInfo.queue, true);
}

VkImageView VkCameraImageV2::getImgView(uint16_t eyeIndex, YuvPlane plane) {
    if(eyeIndex != 0 && eyeIndex != 1){
        LOG_E("eye index must in (0, 1).");
        return VK_NULL_HANDLE;
    }
    if(plane == PLANE_Y){
        return mCameraImages[eyeIndex].yImg.mImgView;
    } else {
        return mCameraImages[eyeIndex].uvImg.mImgView;
    }
}

VkSampler VkCameraImageV2::getSampler(uint16_t eyeIndex, YuvPlane plane) {
    if(eyeIndex != 0 && eyeIndex != 1){
        LOG_E("eye index must in (0, 1).");
        return VK_NULL_HANDLE;
    }
    if(plane == PLANE_Y){
        return mCameraImages[eyeIndex].yImg.mSampler;
    } else {
        return mCameraImages[eyeIndex].uvImg.mSampler;
    }
}

void VkCameraImageV2::destroyImgs() {
    for(auto &cameraImg : mCameraImages){
        // y plane
        if(cameraImg.yImg.mImgView != VK_NULL_HANDLE){
            vkDestroyImageView(mVkBundle->deviceInfo.device, cameraImg.yImg.mImgView, VK_ALLOC);
            cameraImg.yImg.mImgView = VK_NULL_HANDLE;
        }
        if(cameraImg.yImg.mMemory != VK_NULL_HANDLE){
            vkFreeMemory(mVkBundle->deviceInfo.device, cameraImg.yImg.mMemory, VK_ALLOC);
            cameraImg.yImg.mMemory = VK_NULL_HANDLE;
        }
        if(cameraImg.yImg.mImg != VK_NULL_HANDLE){
            vkDestroyImage(mVkBundle->deviceInfo.device, cameraImg.yImg.mImg, VK_ALLOC);
            cameraImg.yImg.mImg = VK_NULL_HANDLE;
        }
        if(cameraImg.yImg.mSampler != VK_NULL_HANDLE){
            vkDestroySampler(mVkBundle->deviceInfo.device, cameraImg.yImg.mSampler, VK_ALLOC);
            cameraImg.yImg.mSampler = VK_NULL_HANDLE;
        }

        // uv plane
        if(cameraImg.uvImg.mImgView != VK_NULL_HANDLE){
            vkDestroyImageView(mVkBundle->deviceInfo.device, cameraImg.uvImg.mImgView, VK_ALLOC);
            cameraImg.uvImg.mImgView = VK_NULL_HANDLE;
        }
        if(cameraImg.uvImg.mMemory != VK_NULL_HANDLE){
            vkFreeMemory(mVkBundle->deviceInfo.device, cameraImg.uvImg.mMemory, VK_ALLOC);
            cameraImg.uvImg.mMemory = VK_NULL_HANDLE;
        }
        if(cameraImg.uvImg.mImg != VK_NULL_HANDLE){
            vkDestroyImage(mVkBundle->deviceInfo.device, cameraImg.uvImg.mImg, VK_ALLOC);
            cameraImg.uvImg.mImg = VK_NULL_HANDLE;
        }
        if(cameraImg.uvImg.mSampler != VK_NULL_HANDLE){
            vkDestroySampler(mVkBundle->deviceInfo.device, cameraImg.uvImg.mSampler, VK_ALLOC);
            cameraImg.uvImg.mSampler = VK_NULL_HANDLE;
        }
    }
    vkDestroyBuffer(mVkBundle->deviceInfo.device, mImgBuffer, VK_ALLOC);
    vkFreeMemory(mVkBundle->deviceInfo.device, mImgBufferMemory, VK_ALLOC);
}

