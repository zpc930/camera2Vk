#pragma once

#include <android/native_window.h>
#include <camera/NdkCaptureRequest.h>
#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraError.h>
#include <camera/NdkCameraManager.h>
#include <memory>

class CameraManager{
public:
    CameraManager(ANativeWindow *nativeWindow, uint8_t selectCamIndex);
    void startCapturing();
    void stopCapturing();

    using ACameraManager_ptr = std::unique_ptr<ACameraManager, decltype(&ACameraManager_delete)>;
    using ACameraIdList_ptr = std::unique_ptr<ACameraIdList, decltype(&ACameraManager_deleteCameraIdList)>;
    using ACameraDevice_ptr = std::unique_ptr<ACameraDevice, decltype(&ACameraDevice_close)>;
    using ACaptureSessionOutputContainer_ptr = std::unique_ptr<ACaptureSessionOutputContainer, decltype(&ACaptureSessionOutputContainer_free)>;
    using ACaptureSessionOutput_ptr = std::unique_ptr<ACaptureSessionOutput, decltype(&ACaptureSessionOutput_free)>;
    using ACameraCaptureSession_ptr = std::unique_ptr<ACameraCaptureSession, decltype(&ACameraCaptureSession_close)>;
    using ACaptureRequest_ptr = std::unique_ptr<ACaptureRequest, decltype(&ACaptureRequest_free)>;
    using ACameraOutputTarget_ptr = std::unique_ptr<ACameraOutputTarget, decltype(&ACameraOutputTarget_free)>;
    static void onDeviceDisconnected(void* a_obj, ACameraDevice* a_device){}
    static void onDeviceError(void* a_obj, ACameraDevice* a_device, int a_err_code){}
    static void onSessionClosed(void* a_obj, ACameraCaptureSession* a_session){}
    static void onSessionReady(void* a_obj, ACameraCaptureSession* a_session){}
    static void onSessionActive(void* a_obj, ACameraCaptureSession* a_session){}
private:
    ACameraDevice_stateCallbacks mDevStateCbs{
        this,
        onDeviceDisconnected,
        onDeviceError
    };
    ACameraCaptureSession_stateCallbacks mCapStateCbs{
        this,
        onSessionClosed,
        onSessionReady,
        onSessionActive
    };

    ANativeWindow *mNativeWindow;
    uint8_t mSelectCamIndex;

    ACameraManager_ptr mManager;
    ACameraIdList_ptr mIds;
    ACameraDevice_ptr mDevice;
    ACaptureSessionOutputContainer_ptr mOutputs;
    ACaptureSessionOutput_ptr mImgReaderOutput;
    ACameraCaptureSession_ptr mSession;
    ACaptureRequest_ptr mCaptureReq;
    ACameraOutputTarget_ptr mTarget;
};