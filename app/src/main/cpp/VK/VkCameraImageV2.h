#pragma once

#include <media/NdkImage.h>
#include "VulkanCommon.h"
#include "VkBundle.h"

enum YuvPlane{
    PLANE_Y,
    PLANE_UV
};

struct VkCameraImage{
    struct{
        VkImage mImg = VK_NULL_HANDLE;
        VkDeviceMemory mMemory = VK_NULL_HANDLE;
        VkImageView mImgView = VK_NULL_HANDLE;
        VkSampler mSampler = VK_NULL_HANDLE;
    } yImg;

    struct{
        VkImage mImg = VK_NULL_HANDLE;
        VkDeviceMemory mMemory = VK_NULL_HANDLE;
        VkImageView mImgView = VK_NULL_HANDLE;
        VkSampler mSampler = VK_NULL_HANDLE;
    } uvImg;
    size_t mDataSize = 0;
};

class VkCameraImageV2{
public:
    VkCameraImageV2(VkBundle *vk);
    ~VkCameraImageV2();
    void init();
    void updateImg(uint64_t frameIndex, uint32_t eyeIndex, const AImage *image);
    VkImageView getImgView(uint16_t eyeIndex, YuvPlane plane);
    VkSampler getSampler(uint16_t eyeIndex, YuvPlane plane);
private:
    void initImgs(VkFormat format, uint32_t width, uint32_t height, VkCommandBuffer cmdBuffer,
                  VkImage *outImg, VkDeviceMemory *outMemory, VkImageView *outImgView, VkSampler *outSampler);
    void destroyImgs();

    VkBundle *mVkBundle;
    VkCameraImage mCameraImages[2];

    VkBuffer mImgBuffer;
    VkDeviceMemory mImgBufferMemory;
};
