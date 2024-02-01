/*!
 * @brief  Camera image reader
 * @date 2023/7/18
 */
#pragma once

#include <media/NdkImageReader.h>
#include <media/NdkImage.h>
#include <memory>
#include <vector>

class CameraImageReader {
public:

    CameraImageReader(uint32_t width, uint32_t height, uint32_t format, uint32_t maxImages);
    AImage *getLatestImage();
    ANativeWindow *getWindow();

    using Image_ptr = std::unique_ptr<AImage, decltype(&AImage_delete)>;
    using ImgReader_ptr = std::unique_ptr<AImageReader, decltype(&AImageReader_delete)>;

private:
    uint32_t mCurIndex;
    ANativeWindow* mNativeWindow = nullptr;
    ImgReader_ptr mReader;
    std::vector<Image_ptr> mImages;
};
