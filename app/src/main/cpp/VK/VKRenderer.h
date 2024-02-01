#pragma once

#include <cstdint>
#include "../Camera/CameraImageReader.h"
#include "../Camera/CameraManager.h"
#include "VkBundle.h"
#include "Geometry.h"
#include "VkCameraImageV2.h"

enum RenderMeshOrder{
    MeshOrderLeftToRight = 0,
    MeshOrderRightToLeft,
    MeshOrderTopToBottom,
    MeshOrderBottomToTop
};

enum RenderMeshArea{
    MeshLeft = 0,      // Columns Left to Right [-1, 0]
    MeshRight,         // Columns Left to Right [0, 1]
    MeshUpperLeft,     // Rows Top to Bottom [Upper Left]
    MeshUpperRight,    // Rows Top to Bottom [Upper Right]
    MeshLowerLeft,     // Rows Top to Bottom [Lower Left]
    MeshLowerRight,    // Rows Top to Bottom [Lower Right]
};

class VKRenderer{
public:
    void Init(struct android_app *app);
    void Destroy();
    bool IsRunning();
    void ProcessFrame(uint64_t frameIndex);
private:
    void OpenCameras();
    void CloseCameras();
    int InitVKEnv();
    void DestroyVKEnv();
    std::vector<char> ReadFileFromAndroidRes(const std::string& filePath);
    void RenderSubArea(const AImage *image, RenderMeshArea area);

    void CreateWindowSurface();
    void InitGeometry();
    void UpdateDescriptorSets(VkCommandBuffer cmdBuffer, uint8_t eyeIndex, const AImage *image);

    struct android_app *mApp;
    bool bRunning = false;
    CameraImageReader *mImageReaderLeft;     // camera image reader
    CameraImageReader *mImageReaderRight;    // camera image reader
    CameraManager *mCameraLeft;              // camera left
    CameraManager *mCameraRight;             // camera right

    uint64_t mLastVsyncTimeNs = 0;
    uint64_t mVsyncCount = 0;
    uint64_t mLastFrameTime = 0;

    uint32_t mCurrentImageIndex;
    VkBundle mVk;                            // vulkan bundle
    Geometry mGeometryLeft;
    Geometry mGeometryRight;
    VkCameraImageV2 *mImageLeft;
    VkCameraImageV2 *mImageRight;
};