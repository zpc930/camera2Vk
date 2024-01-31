#pragma once

#include <android/asset_manager.h>
#include "vulkan_wrapper.h"

class Texture {
public:
    bool load(AAssetManager *assetManager, VkDevice device, VkPhysicalDeviceMemoryProperties physicalMemoType, VkCommandPool cmdPool, VkQueue queue, const char *fileName);
    void destroy(VkDevice device);
    VkImage getImg() { return mImage; };
    VkImageView getImgView() { return mImgView; };
    VkSampler getSampler(){ return mSampler; };
private:
    int width, height;
    int channels;
    VkImage mImage;
    VkDeviceMemory mMemory;

    VkImageView mImgView;
    VkSampler mSampler;
};