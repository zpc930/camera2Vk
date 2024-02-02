#include "GLRenderer.h"
#include <string>
#include <android/choreographer.h>
#include "GLShaderUtil.h"
#include "../Common.h"
#include "../Camera/AndroidCameraPermission.h"
#include "../ProfileTrace.h"

const int64_t gFramePeriodNs = (int64_t)(1e9 / 90);  //90FPS
const float gTimeWarpWaitFramePercentage = 0.5f;
const bool gTimeWarpDelayBetweenEyes = false;
const int gWarpMeshType = 2; //0 = Columns (Left To Right); 1 = Columns (Right To Left); 2 = Rows (Top To Bottom); 3 = Rows (Bottom To Top)

const GLfloat gLeftMeshVertices[] = {
        -0.95f, 0.95f,
        -0.95f, -0.95f,
        -0.05f, 0.95f,
        -0.05f, -0.95f
};
const GLfloat gRightMeshVertices[] = {
        0.05f, 0.95f,
        0.05f, -0.95f,
        0.95f, 0.95f,
        0.95f, -0.95f
};
const GLfloat gMeshTexcoords[] = {
        0, 0,
        0, 1,
        1, 0,
        1, 1
};

static uint64_t gLastVsyncTimeNs = 0;
static void VsyncCallback(long frameTimeNanos, void* data) {
    gLastVsyncTimeNs = frameTimeNanos;
}

std::vector<char> GLRenderer::ReadFileFromAndroidRes(const std::string& filePath) {
    AAsset* file = AAssetManager_open(mApp->activity->assetManager, filePath.c_str(), AASSET_MODE_BUFFER);
    size_t fileLength = AAsset_getLength(file);

    std::vector<char> buffer(fileLength);

    AAsset_read(file, buffer.data(), fileLength);
    AAsset_close(file);

    return buffer;
}

void GLRenderer::Init(struct android_app *app) {
    mApp = app;

    OpenCameras();
    InitEGLEnv();
    CreateProgram();

    glGenTextures(1, &mLeftTextureY);
    glBindTexture(GL_TEXTURE_2D, mLeftTextureY);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenTextures(1, &mLeftTextureUV);
    glBindTexture(GL_TEXTURE_2D, mLeftTextureUV);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenTextures(1, &mRightTextureY);
    glBindTexture(GL_TEXTURE_2D, mRightTextureY);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenTextures(1, &mRightTextureUV);
    glBindTexture(GL_TEXTURE_2D, mRightTextureUV);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    bRunning = true;
}

void GLRenderer::Destroy() {
    bRunning = false;
    glDeleteTextures(1, &mLeftTextureY);
    glDeleteTextures(1, &mLeftTextureUV);
    glDeleteTextures(1, &mRightTextureY);
    glDeleteTextures(1, &mRightTextureUV);
    glDeleteShader(mVertexShader);
    glDeleteShader(mFragShader);
    glDeleteProgram(mProgram);
    DestroyEGLEnv();
    CloseCameras();
}

bool GLRenderer::IsRunning() {
    return bRunning;
}

void GLRenderer::ProcessFrame(uint64_t frameIndex) {
    TRACE_BEGIN("ProcessFrame:%lu", frameIndex);
    AChoreographer *grapher = AChoreographer_getInstance();
    AChoreographer_postFrameCallback(grapher, VsyncCallback, nullptr);
    uint64_t startTimeNs = getTimeNano(CLOCK_MONOTONIC);
    mLastVsyncTimeNs = gLastVsyncTimeNs;
    if (mLastVsyncTimeNs > startTimeNs) {
        LOG_W("%lu: verify that the qvr timestamp is actually in the past...", startTimeNs);
        mLastVsyncTimeNs = startTimeNs;
    }
    if ((startTimeNs - mLastVsyncTimeNs) > gFramePeriodNs) {
        //We've managed to get here prior to the next vsync occuring so set it based on the previous (interrupt may have been delayed)
        LOG_W("%lu:Adding frame period time (%0.2f) to vsync time, Last VSync was %0.4f ms ago!", frameIndex,
             gFramePeriodNs * 1.f / U_TIME_1MS_IN_NS, (startTimeNs - mLastVsyncTimeNs) * 1.f / U_TIME_1MS_IN_NS);
        mLastVsyncTimeNs = mLastVsyncTimeNs + gFramePeriodNs;
    }
    mVsyncCount++;
    int64_t vsyncDiffTimeNs = startTimeNs - mLastVsyncTimeNs;
    double framePct = (double)mVsyncCount + ((double)vsyncDiffTimeNs) / (gFramePeriodNs);
    double fractFrame = framePct - ((long)framePct);

    int64_t waitTimeNs = 0;
    if (fractFrame < gTimeWarpWaitFramePercentage) {
        //We are currently in the first half of the display so wait until we hit the halfway point
        waitTimeNs = (int64_t)((gTimeWarpWaitFramePercentage - fractFrame) * gFramePeriodNs);
    } else {
        //We are currently past the halfway point in the display so wait until halfway through the next vsync cycle
        waitTimeNs = (int64_t)((0.9 + gTimeWarpWaitFramePercentage - fractFrame) * gFramePeriodNs);
        LOG_W("%lu: jank...", frameIndex);
    }
    LOG_D("%lu: Left EyeBuffer Wait : %.2f ms, Vsync diff : %.2f ms, Frame diff : %.2f ms, [%lu: %.2f, %.2f]", frameIndex, waitTimeNs * 1.f / U_TIME_1MS_IN_NS,
          vsyncDiffTimeNs * 1.f / U_TIME_1MS_IN_NS, (startTimeNs - mLastFrameTime) * 1.f / U_TIME_1MS_IN_NS, mVsyncCount, framePct, fractFrame);
    mLastFrameTime = startTimeNs;
    TRACE_BEGIN("Left wait:%.1f:%.1f", waitTimeNs * 1.f / U_TIME_1MS_IN_NS, vsyncDiffTimeNs * 1.f / U_TIME_1MS_IN_NS);
    NanoSleep(waitTimeNs);
    TRACE_END("Left wait:%.1f:%.1f", waitTimeNs * 1.f / U_TIME_1MS_IN_NS, vsyncDiffTimeNs * 1.f / U_TIME_1MS_IN_NS);
    uint64_t postLeftWaitTimeStamp = getTimeNano(CLOCK_MONOTONIC);

    RenderMeshOrder gMeshOrderEnum = MeshOrderLeftToRight;
    if (gWarpMeshType == 0) {
        gMeshOrderEnum = MeshOrderLeftToRight;
    } else if (gWarpMeshType == 1) {
        gMeshOrderEnum = MeshOrderRightToLeft;
    } else if (gWarpMeshType == 2) {
        gMeshOrderEnum = MeshOrderTopToBottom;
    } else if (gWarpMeshType == 3) {
        gMeshOrderEnum = MeshOrderBottomToTop;
    }

    glUseProgram(mProgram);
    glUniform1i(glGetUniformLocation(mProgram, "y_texture"), 0);
    glUniform1i(glGetUniformLocation(mProgram, "uv_texture"), 1);

    AImage *imageLeft = mImageReaderLeft->getLatestImage();
    if(!imageLeft){
        return;
    }
    AImage *imageRight = mImageReaderRight->getLatestImage();
    if(!imageRight){
        return;
    }

    TRACE_BEGIN("UpdateDescriptorSets");
    UpdateTextures(0, imageLeft);
    UpdateTextures(1, imageRight);
    TRACE_END("UpdateDescriptorSets");

    TRACE_BEGIN("First Render");
    switch (gMeshOrderEnum) {
        case MeshOrderLeftToRight:
            RenderSubArea(imageLeft, MeshLeft);
            break;

        case MeshOrderRightToLeft:
            RenderSubArea(imageRight, MeshRight);
            break;

        case MeshOrderTopToBottom:
            RenderSubArea(imageLeft, MeshUpperLeft);
            RenderSubArea(imageRight, MeshUpperRight);
            break;

        case MeshOrderBottomToTop:
            RenderSubArea(imageLeft, MeshLowerLeft);
            RenderSubArea(imageRight, MeshLowerRight);
            break;
    }
#ifdef RENDER_USE_SINGLE_BUFFER
    glFlush(); // its important
#endif
    TRACE_END("First Render");

    if (gTimeWarpDelayBetweenEyes) {
        uint64_t rightTimestamp = getTimeNano(CLOCK_MONOTONIC);
        //We started the left eye half way through vsync, figure out how long
        //we have left in the half frame until the raster starts over so we can render the right eye
        uint64_t delta = rightTimestamp - postLeftWaitTimeStamp;
        uint64_t waitTimeNs = 0;
        if (delta < ((uint64_t)(gFramePeriodNs / 2.0))) {
            waitTimeNs = ((uint64_t)(gFramePeriodNs / 2.0)) - delta;
            LOG_D("%lu: Right EyeBuffer Wait : %.2f ms, Left take : %.2f ms", frameIndex, waitTimeNs * 1.f / U_TIME_1MS_IN_NS, delta * 1.f / U_TIME_1MS_IN_NS);
            TRACE_BEGIN("Right wait:%.1f:%.1f", waitTimeNs * 1.f / U_TIME_1MS_IN_NS, vsyncDiffTimeNs * 1.f / U_TIME_1MS_IN_NS);
            NanoSleep(waitTimeNs + gFramePeriodNs / 8);
            TRACE_END("Right wait:%.1f:%.1f", waitTimeNs * 1.f / U_TIME_1MS_IN_NS, vsyncDiffTimeNs * 1.f / U_TIME_1MS_IN_NS);
        } else {
            //The left eye took longer than 1/2 the refresh so the raster has already wrapped around and is
            //in the left half of the screen.  Skip the wait and get right on rendering the right eye.
            LOG_D("Left Eye took too long!!! ( %.2f ms )", delta * 1.f / U_TIME_1MS_IN_NS);
        }
    }

    TRACE_BEGIN("Second Render");
    switch (gMeshOrderEnum) {
        case MeshOrderLeftToRight:
            RenderSubArea(imageRight, MeshRight);
            break;

        case MeshOrderRightToLeft:
            RenderSubArea(imageLeft, MeshLeft);
            break;

        case MeshOrderTopToBottom:
            RenderSubArea(imageLeft, MeshLowerLeft);
            RenderSubArea(imageRight, MeshLowerRight);
            break;

        case MeshOrderBottomToTop:
            RenderSubArea(imageLeft, MeshUpperLeft);
            RenderSubArea(imageRight, MeshUpperRight);
            break;
    }
#ifdef RENDER_USE_SINGLE_BUFFER
    glFlush(); // its important
#else
    eglSwapBuffers(m_EglDisplay, m_EglSurface);
#endif
    TRACE_END("Second Render");
    TRACE_END("ProcessFrame:%lu", frameIndex);
}

void GLRenderer::OpenCameras() {
    if(!AndroidCameraPermission::isCameraPermitted(mApp)){
        AndroidCameraPermission::requestCameraPermission(mApp);
    }
    mImageReaderLeft = new CameraImageReader(1920, 1440, AIMAGE_FORMAT_YUV_420_888, 4);
    mCameraLeft = new CameraManager(mImageReaderLeft->getWindow(), 2);
    mCameraLeft->startCapturing();

    mImageReaderRight= new CameraImageReader(1920, 1440, AIMAGE_FORMAT_YUV_420_888, 4);
    mCameraRight = new CameraManager(mImageReaderRight->getWindow(), 3);
    mCameraRight->startCapturing();
}

void GLRenderer::CloseCameras() {
    mCameraLeft->stopCapturing();
    mCameraRight->stopCapturing();
    SAFE_DELETE(mImageReaderLeft);
    SAFE_DELETE(mImageReaderRight);
    SAFE_DELETE(mCameraLeft);
    SAFE_DELETE(mCameraRight);
}

int GLRenderer::InitEGLEnv() {
    const EGLint confAttr[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,             // EGL_PBUFFER_BIT
            EGL_RED_SIZE,   8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE,  8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, EGL_DONT_CARE,
            EGL_STENCIL_SIZE, EGL_DONT_CARE,
            EGL_NONE
    };

    // EGL context attributes
    const EGLint ctxAttr[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
    };

    const EGLint surfaceAttr[] = {
#ifdef RENDER_USE_SINGLE_BUFFER
            EGL_RENDER_BUFFER, EGL_SINGLE_BUFFER,
#endif
            EGL_NONE
    };
    EGLint eglMajVers, eglMinVers;
    EGLint numConfigs;
    int resultCode = 0;
    do {
        m_EglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if(m_EglDisplay == EGL_NO_DISPLAY) {
            //Unable to open connection to local windowing system
            LOG_E("GLRenderer::CreateGlesEnv Unable to open connection to local windowing system");
            resultCode = -1;
            break;
        }

        if(!eglInitialize(m_EglDisplay, &eglMajVers, &eglMinVers)) {
            // Unable to initialize EGL. Handle and recover
            LOG_E("GLRenderer::CreateGlesEnv Unable to initialize EGL");
            resultCode = -1;
            break;
        }
        LOG_D("GLRenderer::CreateGlesEnv EGL init with version %d.%d", eglMajVers, eglMinVers);

        if(!eglChooseConfig(m_EglDisplay, confAttr, &mEglConfig, 1, &numConfigs)) {
            LOG_E("GLRenderer::CreateGlesEnv some config is wrong");
            resultCode = -1;
            break;
        }

        //m_EglSurface = eglCreatePbufferSurface(m_EglDisplay, mEglConfig:, surfaceAttr);
        m_EglSurface = eglCreateWindowSurface(m_EglDisplay, mEglConfig, mApp->window, surfaceAttr);
        if(m_EglSurface == EGL_NO_SURFACE) {
            switch(eglGetError()) {
                case EGL_BAD_ALLOC:
                    LOG_E("GLRenderer::CreateGlesEnv Not enough resources available");
                    break;
                case EGL_BAD_CONFIG:
                    LOG_E("GLRenderer::CreateGlesEnv provided EGLConfig is invalid");
                    break;
                case EGL_BAD_PARAMETER:
                    LOG_E("GLRenderer::CreateGlesEnv provided EGL_WIDTH and EGL_HEIGHT is invalid");
                    break;
                case EGL_BAD_MATCH:
                    LOG_E("GLRenderer::CreateGlesEnv Check window and EGLConfig attributes");
                    break;
            }
        }
#ifdef RENDER_USE_SINGLE_BUFFER
        if(!eglSurfaceAttrib(m_EglDisplay, m_EglSurface, EGL_FRONT_BUFFER_AUTO_REFRESH_ANDROID, 1)){
            LOG_E("GLRenderer::eglSurfaceAttrib EGL_FRONT_BUFFER_AUTO_REFRESH_ANDROID fail.");
        }
#endif

        m_EglContext = eglCreateContext(m_EglDisplay, mEglConfig, EGL_NO_CONTEXT, ctxAttr);
        if(m_EglContext == EGL_NO_CONTEXT) {
            EGLint error = eglGetError();
            if(error == EGL_BAD_CONFIG) {
                LOG_E("GLRenderer::CreateGlesEnv EGL_BAD_CONFIG");
                resultCode = -1;
                break;
            }
        }

        if(!eglMakeCurrent(m_EglDisplay, m_EglSurface, m_EglSurface, m_EglContext)) {
            LOG_E("GLRenderer::CreateGlesEnv MakeCurrent failed");
            resultCode = -1;
            break;
        }
        LOG_E("GLRenderer::CreateGlesEnv initialize success!");
    } while (false);

    if (resultCode != 0){
        LOG_E("GLRenderer::CreateGlesEnv fail");
    }
    return resultCode;
}

void GLRenderer::DestroyEGLEnv() {
    if (m_EglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(m_EglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(m_EglDisplay, m_EglContext);
        eglDestroySurface(m_EglDisplay, m_EglSurface);
        eglReleaseThread();
        eglTerminate(m_EglDisplay);
    }
    m_EglDisplay = EGL_NO_DISPLAY;
    m_EglSurface = EGL_NO_SURFACE;
    m_EglContext = EGL_NO_CONTEXT;
}

void GLRenderer::CreateProgram() {
//    std::vector<char> vsSource = ReadFileFromAndroidRes("shaders/gles/camera_preview.vert");
//    std::vector<char> fsSource = ReadFileFromAndroidRes("shaders/gles/camera_preview.frag");
//    mVertexShader = CreateGLShader(std::string(vsSource.data(), vsSource.size()).c_str(), GL_VERTEX_SHADER);
//    mFragShader = CreateGLShader(std::string(fsSource.data(), fsSource.size()).c_str(), GL_FRAGMENT_SHADER);
    mVertexShader = CreateGLShader(vertexShader, GL_VERTEX_SHADER);
    mFragShader = CreateGLShader(fragYUV420P, GL_FRAGMENT_SHADER);
    mProgram = CreateGLProgram(mVertexShader, mFragShader);
}

void GLRenderer::UpdateTextures(uint32_t eyeIndex, const AImage *image) {
    int64_t timeStamp = 0;
    AImage_getTimestamp(image, &timeStamp);
    int64_t diffNs = getTimeNano(CLOCK_MONOTONIC) - timeStamp;
    LOG_D("%s Update:%.2f, %lu", eyeIndex == 0 ? "Left" : "Right", (diffNs * 1.f) / U_TIME_1MS_IN_NS, timeStamp);
    int width, height;
    int numPlanes = 0;
    uint8_t *yData, *uvData;
    int32_t yDataLen = 0, uvDataLen = 0;
    TRACE_BEGIN("%s Update:%.2f", eyeIndex == 0 ? "Left" : "Right", (diffNs * 1.f) / U_TIME_1MS_IN_NS);
    AImage_getWidth(image, &width);
    AImage_getHeight(image, &height);
    AImage_getNumberOfPlanes(image, &numPlanes);
    AImage_getPlaneData(image, 0, &yData, &yDataLen);
    AImage_getPlaneData(image, 2, &uvData, &uvDataLen);

    glBindTexture(GL_TEXTURE_2D, eyeIndex == 0 ? mLeftTextureY : mRightTextureY);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, yData);

    glBindTexture(GL_TEXTURE_2D, eyeIndex == 0 ? mLeftTextureUV : mRightTextureUV);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, width / 2, height / 2, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, uvData);
    TRACE_END("%s Update:%.2f", eyeIndex == 0 ? "Left" : "Right", (diffNs * 1.f) / U_TIME_1MS_IN_NS);
}

void GLRenderer::RenderSubArea(const AImage *image, RenderMeshArea area) {
    int surfaceWidth, surfaceHeight;
    eglQuerySurface(m_EglDisplay, m_EglSurface, EGL_WIDTH, &surfaceWidth);
    eglQuerySurface(m_EglDisplay, m_EglSurface, EGL_HEIGHT, &surfaceHeight);
    glEnable(GL_SCISSOR_TEST);
    switch (area) {
        case MeshLeft:
            glScissor(0, 0, surfaceWidth / 2, surfaceHeight);
            glClearColor(1.f, 0.f, 0.f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);
            break;
        case MeshRight:
            glScissor(surfaceWidth / 2, 0, surfaceWidth / 2, surfaceHeight);
            glClearColor(0.f, 1.f, 0.f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);
            break;
        case MeshUpperLeft:
            glScissor(0, surfaceHeight / 2, surfaceWidth / 2, surfaceHeight / 2);
            glClearColor(1.f, 0.f, 0.f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);
            break;
        case MeshUpperRight:
            glScissor(surfaceWidth / 2, surfaceHeight / 2, surfaceWidth / 2, surfaceHeight / 2);
            glClearColor(0.f, 1.f, 0.f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);
            break;
        case MeshLowerLeft:
            glScissor(0, 0, surfaceWidth / 2, surfaceHeight / 2);
            glClearColor(1.f, 1.f, 0.f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);
            break;
        case MeshLowerRight:
            glScissor(surfaceWidth / 2, 0, surfaceWidth / 2, surfaceHeight / 2);
            glClearColor(0.f, 1.f, 1.f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);
            break;
    }

    int eyeIndex = 0;
    if(area == MeshLeft | area == MeshUpperLeft | area == MeshLowerLeft){
        eyeIndex = 0;
        GLuint posLoc = glGetAttribLocation(mProgram, "a_position");
        glEnableVertexAttribArray(posLoc);
        glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 0, gLeftMeshVertices);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mLeftTextureY);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mLeftTextureUV);
    } else {
        eyeIndex = 1;
        GLuint posLoc = glGetAttribLocation(mProgram, "a_position");
        glEnableVertexAttribArray(posLoc);
        glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 0, gRightMeshVertices);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mRightTextureY);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mRightTextureUV);
    }
    GLuint texcoordLoc = glGetAttribLocation(mProgram, "a_texcoord");
    glEnableVertexAttribArray(texcoordLoc);
    glVertexAttribPointer(texcoordLoc, 2, GL_FLOAT, GL_FALSE, 0, gMeshTexcoords);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
