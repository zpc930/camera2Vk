//
// Created by ts on 2023/7/18.
//
#include "CameraImageReader.h"
#include "Common.h"

CameraImageReader::CameraImageReader(uint32_t width, uint32_t height, uint32_t format,
                                     uint64_t usage, uint32_t maxImages)
    : mCurIndex{maxImages - 1}, mReader{nullptr, AImageReader_delete}{

    if(maxImages < 2)
        throw std::runtime_error("Max images must be at least 2.");

    for(uint32_t i = 0; i < maxImages; ++i)
        mImages.push_back({nullptr, AImage_delete});

    mBuffers = std::vector<AHardwareBuffer*>(maxImages, nullptr);

    auto pt = mReader.release();
    AImageReader_newWithUsage(width, height, format, usage, mImages.size() + 2, &pt);
    mReader.reset(pt);

    if(!mReader)
        throw std::runtime_error("Failed to create image reader.");

    auto result = AImageReader_getWindow(mReader.get(), &mNativeWindow);

    if (result != AMEDIA_OK || mNativeWindow == nullptr)
        throw std::runtime_error("Failed to obtain window handle.");

    Print("Image reader created.");
}

AHardwareBuffer *CameraImageReader::getLatestBuffer() {
    AImage *image = nullptr;
    auto result = AImageReader_acquireLatestImage(mReader.get(), &image);
    if(result != AMEDIA_OK || !image){
        Error("Failed to acquire image from camera.");
    }
    else {
        AHardwareBuffer* buffer = nullptr;
        auto result = AImage_getHardwareBuffer(image, &buffer);
        if(result != AMEDIA_OK || !buffer){
            Error("Failed to acquire hardware buffer.");
        } else {
            mCurIndex++;
            if(mCurIndex == mImages.size())
                mCurIndex = 0;
            mImages[mCurIndex].reset(image);
            mBuffers[mCurIndex] = buffer;
        }
    }
    return mBuffers[mCurIndex];
}

ANativeWindow *CameraImageReader::getWindow() {
    return mNativeWindow;
}
