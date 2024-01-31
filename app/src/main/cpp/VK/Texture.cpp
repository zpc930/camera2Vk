#include "Texture.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <sstream>
#include "VkHelper.h"

bool Texture::load(AAssetManager *assetManager, VkDevice device, VkPhysicalDeviceMemoryProperties physicalMemoType, VkCommandPool cmdPool, VkQueue queue, const char *fileName) {
    AAsset* file = AAssetManager_open(assetManager, fileName, AASSET_MODE_BUFFER);
    if(!file){
        std::stringstream ss;
        ss << "can not load the texture, fileName=" << fileName;
        throw std::runtime_error(ss.str());
    }
    auto fileData = std::vector<stbi_uc>(AAsset_getLength(file));
    if(AAsset_read(file, fileData.data(), fileData.size()) != fileData.size())
    {
        AAsset_close(file);
        throw std::runtime_error{"Unknown error. Couldn't read from texture file."};
    }
    AAsset_close(file);

    auto pixels = stbi_load_from_memory(fileData.data(), fileData.size(), &height, &width,
                                          &channels, STBI_rgb_alpha);
    if(nullptr == pixels){
        std::stringstream ss;
        ss << "can not load the texture, fileName=" << fileName;
        throw std::runtime_error(ss.str());
    }

    VkDeviceSize size = width * height * 4;//rgba
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VkHelper::createBufferInternal(physicalMemoType, device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   size, &stagingBuffer, &stagingMemory);
    void* data;
    vkMapMemory(device, stagingMemory, 0, size, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(size));
    vkUnmapMemory(device, stagingMemory);
    stbi_image_free(pixels);

    VkHelper::createImage(physicalMemoType, device, width, height, 1, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_SRGB,
                          VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                          &mImage, &mMemory);

    VkCommandBuffer cmdBuffer;
    VkHelper::allocateCommandBuffers(device, cmdPool, 1, &cmdBuffer);
    VkHelper::beginCommandBuffer(cmdBuffer, true);
    VkBufferImageCopy region = {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1
            },
            .imageOffset = {0, 0, 0},
            .imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1}
    };
    vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    VkHelper::endCommandBuffer(cmdBuffer, device, cmdPool, queue, true);

    vkDestroyBuffer(device, stagingBuffer, VK_ALLOC);
    vkFreeMemory(device, stagingMemory, VK_ALLOC);

    //image view
    VkHelper::createImageView(device, mImage, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, &mImgView);
    //sampler
    VkHelper::createImageSampler(device, &mSampler);

    return false;
}

void Texture::destroy(VkDevice device) {
    vkDestroySampler(device, mSampler, VK_ALLOC);
    vkDestroyImageView(device, mImgView, VK_ALLOC);
    vkDestroyImage(device, mImage, VK_ALLOC);
    vkFreeMemory(device, mMemory, VK_ALLOC);
}
