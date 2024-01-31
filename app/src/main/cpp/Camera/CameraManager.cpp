#include "CameraManager.h"
#include <stdexcept>
#include <sstream>
#include "../Common.h"

CameraManager::CameraManager(ANativeWindow *nativeWindow, uint8_t selectCamIndex)
    : mNativeWindow(nativeWindow),
      mSelectCamIndex(selectCamIndex),
      mManager{nullptr, ACameraManager_delete},
      mIds{nullptr, ACameraManager_deleteCameraIdList},
      mDevice{nullptr, ACameraDevice_close},
      mOutputs{nullptr, ACaptureSessionOutputContainer_free},
      mImgReaderOutput{nullptr, ACaptureSessionOutput_free},
      mSession{nullptr, ACameraCaptureSession_close},
      mCaptureReq{nullptr, ACaptureRequest_free},
      mTarget{nullptr, ACameraOutputTarget_free}{

    if(!nativeWindow)
            throw std::runtime_error("Invalid window handle.");

    mManager.reset(ACameraManager_create());
    if(!mManager.get())
        throw std::runtime_error("Cannot create camera manager.");

    auto pt = mIds.release();
    auto result = ACameraManager_getCameraIdList(mManager.get(), &pt);
    mIds.reset(pt);
    if(result != ACAMERA_OK){
        std::stringstream ss;
        ss << "Failed to acquire camera list. (code: " << result << ").";
        throw std::runtime_error(ss.str().c_str());
    }
    if(mIds->numCameras < 1)
        throw std::runtime_error("No cameras found.");

    if(mIds->numCameras <= selectCamIndex){
        std::stringstream ss;
        ss << "This camera not found: " << selectCamIndex << ".";
        throw std::runtime_error(ss.str().c_str());
    }

    std::string selectedCamera;
// if you want query camera's metadata
#if 1
    for(uint32_t i = 0; i < mIds->numCameras; ++i)
    {
        auto cam_id = mIds->cameraIds[i];
        LOG_D("camera %s", cam_id);

        ACameraMetadata* metadata = nullptr;
        result = ACameraManager_getCameraCharacteristics(mManager.get(), cam_id, &metadata);
        if(result != ACAMERA_OK || !metadata)
            continue;
        ACameraMetadata_const_entry entry;
        if(ACameraMetadata_getConstEntry(metadata, ACAMERA_LENS_FACING, &entry) == ACAMERA_OK)
        {
            auto facing = static_cast<acamera_metadata_enum_android_lens_facing_t>(entry.data.u8[0]);

            // Select which camera to use (front or back)
            if(facing == ACAMERA_LENS_FACING_BACK)
                selectedCamera = std::string(cam_id);
        }
        ACameraMetadata_free(metadata);
    }
    selectedCamera = std::to_string(selectCamIndex);
#else
    selectedCamera = std::to_string(selectCamIndex);
#endif

    //device
    {
        auto pt = mDevice.release();
        result = ACameraManager_openCamera(mManager.get(), selectedCamera.c_str(),
                                           &mDevStateCbs, &pt);
        mDevice.reset(pt);

        if(result != ACAMERA_OK || !mDevice.get()){
            std::stringstream sstr;
            sstr << "Couldn't open camera. (code: " << result << ").";
            throw std::runtime_error(sstr.str());
        }
    }

    //outputs
    {
        auto pt = mOutputs.release();
        result = ACaptureSessionOutputContainer_create(&pt);
        mOutputs.reset(pt);

        if(result != ACAMERA_OK)
            throw std::runtime_error("Capture session output container creation failed.");
    }

    //image reader output
    {
        auto pt = mImgReaderOutput.release();
        result = ACaptureSessionOutput_create(nativeWindow, &pt);
        mImgReaderOutput.reset(pt);
        if(result != ACAMERA_OK)
            throw std::runtime_error("Capture session image reader output creation failed.");
    }

    //capture session output container
    {
        result = ACaptureSessionOutputContainer_add(mOutputs.get(), mImgReaderOutput.get());
        if(result != ACAMERA_OK)
            throw std::runtime_error("Couldn't add image reader output session to container.");
    }

    //session
    {
        auto pt = mSession.release();
        result = ACameraDevice_createCaptureSession(mDevice.get(), mOutputs.get(),
                                                    &mCapStateCbs, &pt);
        mSession.reset(pt);
        if(result != ACAMERA_OK)
            throw std::runtime_error("Couldn't create capture session.");
    }

    //capture
    {
        auto pt = mCaptureReq.release();
        result = ACameraDevice_createCaptureRequest(mDevice.get(), TEMPLATE_PREVIEW, &pt);
        mCaptureReq.reset(pt);
        if(result != ACAMERA_OK)
            throw std::runtime_error("Couldn't create capture request.");
    }

    //target
    {
        auto pt = mTarget.release();
        result = ACameraOutputTarget_create(nativeWindow, &pt);
        mTarget.reset(pt);
        if(result != ACAMERA_OK)
            throw std::runtime_error("Couldn't create camera output target.");
    }

    result = ACaptureRequest_addTarget(mCaptureReq.get(), mTarget.get());
    if(result != ACAMERA_OK)
        throw std::runtime_error("Couldn't add capture request to camera output target.");

    //end
    LOG_D("Camera %d logical device created.", selectCamIndex);
}

void CameraManager::startCapturing(){
    auto pt = mCaptureReq.release();
    ACameraCaptureSession_setRepeatingRequest(mSession.get(), nullptr, 1, &pt, nullptr);
    mCaptureReq.reset(pt);
}

void CameraManager::stopCapturing(){
    ACameraCaptureSession_stopRepeating(mSession.get());
}