//
// Created by ts on 2023/7/18.
//
#include "VkCameraImage.h"
#include "VulkanCommon.h"
#include "VkHelper.h"

VkCameraImage::VkCameraImage(VkBundle *vk, AHardwareBuffer *hb) {
    AHardwareBuffer_Desc bufferDesc;
    AHardwareBuffer_describe(hb, &bufferDesc);

    mDataSize = bufferDesc.width * bufferDesc.height * bufferDesc.layers;

    // query format properties
    VkAndroidHardwareBufferFormatPropertiesANDROID formatInfo = {
            .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID,
            .pNext = nullptr,
    };
    VkAndroidHardwareBufferPropertiesANDROID propertiesInfo = {
            .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,
            .pNext = &formatInfo
    };
    CALL_VK(vkGetAndroidHardwareBufferPropertiesANDROID(vk->deviceInfo.device, hb, &propertiesInfo));

    // build sampler ycbcr create info
    VkExternalFormatANDROID externalFormat = {
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID,
            .pNext = nullptr
    };
    VkSamplerYcbcrConversionCreateInfo convInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
            .pNext = nullptr
    };
    if(formatInfo.format == VK_FORMAT_UNDEFINED)
    {
        externalFormat.externalFormat = formatInfo.externalFormat;
        convInfo.pNext = &externalFormat;
        convInfo.format = VK_FORMAT_UNDEFINED;
        convInfo.ycbcrModel = formatInfo.suggestedYcbcrModel;
    }
    else
    {
        convInfo.pNext = &externalFormat;
        convInfo.format = formatInfo.format;
        convInfo.ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709;
    }
    convInfo.ycbcrRange = formatInfo.suggestedYcbcrRange;
    convInfo.components = formatInfo.samplerYcbcrConversionComponents;
    convInfo.xChromaOffset = formatInfo.suggestedXChromaOffset;
    convInfo.yChromaOffset = formatInfo.suggestedYChromaOffset;
    convInfo.chromaFilter = VK_FILTER_NEAREST;
    convInfo.forceExplicitReconstruction = false;
    CALL_VK(vkCreateSamplerYcbcrConversion(vk->deviceInfo.device, &convInfo, VK_ALLOC, &mConversion));

    VkSamplerYcbcrConversionInfo samplerYcbcrConversionInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
            .pNext = nullptr,
            .conversion = mConversion
    };

    //create sampler
    VkSamplerCreateInfo samplerCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = &samplerYcbcrConversionInfo,
            .flags = 0,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_NEAREST,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .mipLodBias = 0.0f,
            .anisotropyEnable = false,
            .maxAnisotropy = 1.0f,
            .compareEnable = false,
            .compareOp = VK_COMPARE_OP_NEVER,
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            .unnormalizedCoordinates = false
    };
    CALL_VK(vkCreateSampler(vk->deviceInfo.device, &samplerCreateInfo, VK_ALLOC, &mSampler));
}

void VkCameraImage::update(VkBundle *vk, VkImageUsageFlags usage, VkSharingMode sharingMode, AHardwareBuffer *hb) {
    vkDeviceWaitIdle(vk->deviceInfo.device);

    //destroy resources
    destroyResources(vk, false);

    AHardwareBuffer_Desc bufferDesc;
    AHardwareBuffer_describe(hb, &bufferDesc);

    if(bufferDesc.width * bufferDesc.height * bufferDesc.layers != mDataSize)
        throw std::runtime_error{"Data size differs. Cannot update image."};

    // query format properties
    VkAndroidHardwareBufferFormatPropertiesANDROID formatInfo = {
            .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID,
    };
    VkAndroidHardwareBufferPropertiesANDROID propertiesInfo = {
            .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,
            .pNext = &formatInfo
    };
    CALL_VK(vkGetAndroidHardwareBufferPropertiesANDROID(vk->deviceInfo.device, hb, &propertiesInfo));

    VkExternalMemoryImageCreateInfo extMemInfo = {
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID
    };

//    VkPhysicalDeviceImageFormatInfo2 physicalDevImgFmtInfo2 = {
//            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
//            .pNext = nullptr,
//            .format = VK_FORMAT_UNDEFINED,
//            .type = VK_IMAGE_TYPE_2D,
//            .tiling = VK_IMAGE_TILING_LINEAR,
//            .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
//            .flags = 0
//    };
//    VkImageFormatProperties2 imgFmtProps2 = {
//            .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
//            .pNext = nullptr,
//    };
//    CALL_VK(vkGetPhysicalDeviceImageFormatProperties2(vk->deviceInfo.physicalDev, &physicalDevImgFmtInfo2, &imgFmtProps2));

    // build image create info
    VkExternalFormatANDROID externalFormat = {
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID,
            .pNext = &extMemInfo
    };

    VkImageCreateInfo imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
    };

    if(formatInfo.format == VK_FORMAT_UNDEFINED)
    {
        externalFormat.externalFormat = formatInfo.externalFormat;
        imageCreateInfo.pNext = &externalFormat;
        imageCreateInfo.format = VK_FORMAT_UNDEFINED;
    }
    else
    {
        imageCreateInfo.pNext = &externalFormat;
        imageCreateInfo.format = formatInfo.format;
    }
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.extent = VkExtent3D{ bufferDesc.width, bufferDesc.height, 1 };
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = bufferDesc.layers;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = usage;
    imageCreateInfo.sharingMode = sharingMode;
    imageCreateInfo.queueFamilyIndexCount = 0;
    imageCreateInfo.pQueueFamilyIndices = nullptr;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    CALL_VK(vkCreateImage(vk->deviceInfo.device, &imageCreateInfo, VK_ALLOC, &mImage));

    // allocate and bind image memory
    VkImportAndroidHardwareBufferInfoANDROID importInfo = {
            .sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
            .pNext = nullptr,
            .buffer = hb
    };

    VkMemoryDedicatedAllocateInfo memDedInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
            .pNext = &importInfo,
            .image = mImage,
            .buffer = VK_NULL_HANDLE
    };

    VkMemoryAllocateInfo memoryAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = &memDedInfo,
            .allocationSize = propertiesInfo.allocationSize,
            .memoryTypeIndex = VkHelper::getMemoryIndex(vk->deviceInfo.physicalDevMemoProps, propertiesInfo.memoryTypeBits, MemoryLocation::EXTERNAL)
    };
    CALL_VK(vkAllocateMemory(vk->deviceInfo.device, &memoryAllocateInfo, VK_ALLOC, &mMemory));
    CALL_VK(vkBindImageMemory(vk->deviceInfo.device, mImage, mMemory, 0));

    //check memory requirements
    VkImageMemoryRequirementsInfo2 memReqsInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
            .pNext = nullptr,
            .image = mImage
    };
    VkMemoryDedicatedRequirements memDedReqs = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,
            .pNext = nullptr
    };
    VkMemoryRequirements2 memReqs = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
            .pNext = &memDedReqs,
    };
//    vkGetImageMemoryRequirements2(vk->deviceInfo.device, &memReqsInfo, &memReqs);
//
//    if(!memDedReqs.prefersDedicatedAllocation || !memDedReqs.requiresDedicatedAllocation){
//        Error("VkMemoryDedicatedRequirements.prefersDedicatedAllocation or requiresDedicatedAllocation!!!");
//        return;
//    }

    VkSamplerYcbcrConversionInfo samplerYcbcrConversionInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
            .pNext = nullptr,
            .conversion = mConversion
    };

    VkImageViewCreateInfo imgViewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = &samplerYcbcrConversionInfo,
            .flags = 0,
            .image = mImage,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM_KHR,
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
    CALL_VK(vkCreateImageView(vk->deviceInfo.device, &imgViewInfo, VK_ALLOC, &mImgView));
}

void VkCameraImage::destroyResources(VkBundle *vk, bool isDestroySampler) noexcept {
    if(mImgView != VK_NULL_HANDLE)
        vkDestroyImageView(vk->deviceInfo.device, mImgView, VK_ALLOC);
    if(mImage != VK_NULL_HANDLE)
        vkDestroyImage(vk->deviceInfo.device, mImage, VK_ALLOC);
    if(mMemory != VK_NULL_HANDLE)
        vkFreeMemory(vk->deviceInfo.device, mMemory, VK_ALLOC);
    mImgView = VK_NULL_HANDLE;
    mImage = VK_NULL_HANDLE;
    mMemory = VK_NULL_HANDLE;

    if(isDestroySampler) {
        if (mSampler != VK_NULL_HANDLE)
            vkDestroySampler(vk->deviceInfo.device, mSampler, VK_ALLOC);
        if (mConversion != VK_NULL_HANDLE)
            vkDestroySamplerYcbcrConversion(vk->deviceInfo.device, mConversion, VK_ALLOC);
    }
}
