//
// Created by ts on 2023/7/18.
//
#ifndef CAMERA2VK_VKCAMERAIMAGE_H
#define CAMERA2VK_VKCAMERAIMAGE_H

#include "VkBundle.h"
#include "CameraImageReader.h"
#include "vulkan_wrapper.h"

class VkCameraImage {
public:
    VkCameraImage(VkBundle *vk, AHardwareBuffer* hb);
    void update(VkBundle *vk, VkImageUsageFlags usage, VkSharingMode sharingMode, AHardwareBuffer* hb);
    VkImage getImg() { return mImage; };
    VkImageView getImgView() { return mImgView; };
    VkSampler getSampler(){ return mSampler; };
    void destroyResources(VkBundle *vk, bool isDestroySampler) noexcept;

private:
    size_t mDataSize = 0;
    VkImage mImage = VK_NULL_HANDLE;
    VkDeviceMemory mMemory = VK_NULL_HANDLE;
    VkImageView mImgView = VK_NULL_HANDLE;
    VkSampler mSampler = VK_NULL_HANDLE;
    VkSamplerYcbcrConversion mConversion = VK_NULL_HANDLE;
};


#endif //CAMERA2VK_VKCAMERAIMAGE_H
