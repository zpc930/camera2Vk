//
// Created by ts on 2023/7/18.
//
#include "CameraImageReader.h"
#include "../Common.h"

CameraImageReader::CameraImageReader(uint32_t width, uint32_t height, uint32_t format, uint32_t maxImages)
                            : mCurIndex{maxImages - 1}, mReader{nullptr, AImageReader_delete}{

    if(maxImages < 2)
        throw std::runtime_error("Max images must be at least 2.");

    for(uint32_t i = 0; i < maxImages; ++i)
        mImages.push_back({nullptr, AImage_delete});

    auto pt = mReader.release();
    //AImageReader_newWithUsage(width, height, format, usage, mImages.size() + 2, &pt);
    auto rt = AImageReader_new(width, height, format, mImages.size() + 2, &pt);
    if(rt != AMEDIA_OK){
        LOG_E("Failed to create image reader.");
    }
//    AImageReader_ImageListener listener;
//    listener.context = nullptr;
//    listener.onImageAvailable = leftImageCallback;
//    AImageReader_setImageListener(pt, &listener);
    mReader.reset(pt);

    if(!mReader)
        throw std::runtime_error("Failed to create image reader.");

    auto result = AImageReader_getWindow(mReader.get(), &mNativeWindow);

    if (result != AMEDIA_OK || mNativeWindow == nullptr)
        throw std::runtime_error("Failed to obtain window handle.");

    LOG_D("Image reader created.");
}

AImage *CameraImageReader::getLatestImage() {
    AImage *image = nullptr;
    auto result = AImageReader_acquireLatestImage(mReader.get(), &image);
    if(result == AMEDIA_OK && image) {
        mCurIndex++;
        if(mCurIndex == mImages.size())
            mCurIndex = 0;
        mImages[mCurIndex].reset(image);
    }
    return mImages[mCurIndex].get();
}

ANativeWindow *CameraImageReader::getWindow() {
    return mNativeWindow;
}
