#include "GLRenderer.h"
#include <string>
#include <android/choreographer.h>
#include "GLShaderUtil.h"
#include "../Common.h"
#include "../Camera/AndroidCameraPermission.h"

const int64_t gFramePeriodNs = (int64_t)(1e9 / 90);  //90FPS
const float gTimeWarpWaitFramePercentage = 0.5f;
const bool gTimeWarpDelayBetweenEyes = true;
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

    glGenTextures(1, &yTexture);
    glGenTextures(1, &uvTexture);
    bRunning = true;
}

void GLRenderer::Destroy() {
    bRunning = false;
    glDeleteTextures(1, &yTexture);
    glDeleteTextures(1, &uvTexture);
    glDeleteShader(m_VertexShader);
    glDeleteShader(m_FragShader);
    glDeleteProgram(m_Program);
    DestroyEGLEnv();
    CloseCameras();
}

bool GLRenderer::IsRunning() {
    return bRunning;
}

void GLRenderer::ProcessFrame(uint64_t frameIndex) {
    LOG_D("Frame start : %lu", frameIndex);

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
    LOG_D("Left wait:%.1f:%.1f", waitTimeNs * 1.f / U_TIME_1MS_IN_NS, vsyncDiffTimeNs * 1.f / U_TIME_1MS_IN_NS);
    NanoSleep(waitTimeNs);
    uint64_t postLeftWaitTimeStamp = getTimeNano(CLOCK_MONOTONIC);

    AHardwareBuffer *hwbufLeft = mImageReaderLeft->getLatestBuffer();
    LOG_D("Left Render...: %p", hwbufLeft);
    if(!hwbufLeft){
        return;
    }
    glClearColor(0.1f, 0.2f, 0.3f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(m_Program);
    {
        GLfloat vertices[] = {
                -0.9f, -0.8f,
                -0.1f, -0.8f,
                -0.9f, 0.8f,
                -0.1f, 0.8f,
        };
        GLfloat colors[] = {
                1.f, 0.f, 0.f, 1.f,
                1.f, 0.f, 0.f, 1.f,
                1.f, 0.f, 0.f, 1.f,
                1.f, 0.f, 0.f, 1.f
        };
        GLfloat texCoords[] = {
                0.0f, 0.0f,
                1.0f, 0.0f,
                0.0f, 1.0f,
                1.0f, 1.0f
        };
        GLuint posLoc = glGetAttribLocation(m_Program, "a_position");
        glEnableVertexAttribArray(posLoc);
        glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 0, vertices);

        GLuint colorLoc = glGetAttribLocation(m_Program, "a_color");
        glEnableVertexAttribArray(colorLoc);
        glVertexAttribPointer(colorLoc, 4, GL_FLOAT, GL_FALSE, 0, colors);

        GLuint texcoordLoc = glGetAttribLocation(m_Program, "a_texcoord");
        glEnableVertexAttribArray(texcoordLoc);
        glVertexAttribPointer(texcoordLoc, 2, GL_FLOAT, GL_FALSE, 0, texCoords);
    }
    {
        uint8_t outputPixels[1600 * 1600 * 3 / 2];
        void *hwPtr;
        AHardwareBuffer_lock(hwbufLeft, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN, -1, nullptr, (void**)&hwPtr);
        memcpy(outputPixels, hwPtr, sizeof(outputPixels));
        AHardwareBuffer_unlock(hwbufLeft, nullptr);

        glUniform1i(glGetUniformLocation(m_Program, "y_texture"), 0);
        glUniform1i(glGetUniformLocation(m_Program, "uv_texture"), 1);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, yTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1600, 1600, 0, GL_RED, GL_UNSIGNED_BYTE, outputPixels);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, uvTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, 1600 / 2, 1600 / 2, 0, GL_RG, GL_UNSIGNED_BYTE, outputPixels + 1600 * 1600);
    }
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    //glFlush(); // its important

    if (gTimeWarpDelayBetweenEyes) {
        uint64_t rightTimestamp = getTimeNano(CLOCK_MONOTONIC);
        //We started the left eye half way through vsync, figure out how long
        //we have left in the half frame until the raster starts over so we can render the right eye
        uint64_t delta = rightTimestamp - postLeftWaitTimeStamp;
        uint64_t waitTimeNs = 0;
        if (delta < ((uint64_t)(gFramePeriodNs / 2.0))) {
            waitTimeNs = ((uint64_t)(gFramePeriodNs / 2.0)) - delta;
            LOG_D("%lu: Right EyeBuffer Wait : %.2f ms, Left take : %.2f ms", frameIndex, waitTimeNs * 1.f / U_TIME_1MS_IN_NS, delta * 1.f / U_TIME_1MS_IN_NS);
            NanoSleep(waitTimeNs + gFramePeriodNs / 8);
        } else {
            //The left eye took longer than 1/2 the refresh so the raster has already wrapped around and is
            //in the left half of the screen.  Skip the wait and get right on rendering the right eye.
            LOG_D("Left Eye took too long!!! ( %.2f ms )", delta * 1.f / U_TIME_1MS_IN_NS);
        }
    }
//    AHardwareBuffer *hwbufRight = mImageReaderRight->getLatestBuffer();
//    LOG_D("Right Render...:%p", hwbufRight);

    {
        GLfloat vertices[] = {
                0.1f, -0.8f,
                0.9f, -0.8f,
                0.1f, 0.8f,
                0.9f, 0.8f,
        };
        GLfloat colors[] = {
                0.f, 1.f, 0.f, 1.f,
                0.f, 1.f, 0.f, 1.f,
                0.f, 1.f, 0.f, 1.f,
                0.f, 1.f, 0.f, 1.f
        };
        GLfloat texCoords[] = {
                0.0f, 0.0f,
                1.0f, 0.0f,
                0.0f, 1.0f,
                1.0f, 1.0f
        };
        GLuint posLoc = glGetAttribLocation(m_Program, "a_position");
        glEnableVertexAttribArray(posLoc);
        glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 0, vertices);

        GLuint colorLoc = glGetAttribLocation(m_Program, "a_color");
        glEnableVertexAttribArray(colorLoc);
        glVertexAttribPointer(colorLoc, 4, GL_FLOAT, GL_FALSE, 0, colors);

        GLuint texcoordLoc = glGetAttribLocation(m_Program, "a_texcoord");
        glEnableVertexAttribArray(texcoordLoc);
        glVertexAttribPointer(texcoordLoc, 2, GL_FLOAT, GL_FALSE, 0, texCoords);
    }
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    eglSwapBuffers(m_EglDisplay, m_EglSurface);
}

void GLRenderer::OpenCameras() {
    if(!AndroidCameraPermission::isCameraPermitted(mApp)){
        AndroidCameraPermission::requestCameraPermission(mApp);
    }
    mImageReaderLeft = new CameraImageReader(1600, 1600, AIMAGE_FORMAT_PRIVATE, AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE, 4);
    mCameraLeft = new CameraManager(mImageReaderLeft->getWindow(), 0);
    mCameraLeft->startCapturing();

//    mImageReaderRight= new CameraImageReader(1600, 1600, AIMAGE_FORMAT_PRIVATE, AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE, 4);
//    mCameraRight = new CameraManager(mImageReaderRight->getWindow(), 1);
//    mCameraRight->startCapturing();
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

        if(!eglChooseConfig(m_EglDisplay, confAttr, &m_EglConfig, 1, &numConfigs)) {
            LOG_E("GLRenderer::CreateGlesEnv some config is wrong");
            resultCode = -1;
            break;
        }

        //m_EglSurface = eglCreatePbufferSurface(m_EglDisplay, m_EglConfig, surfaceAttr);
        m_EglSurface = eglCreateWindowSurface(m_EglDisplay, m_EglConfig, mApp->window, surfaceAttr);
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

        m_EglContext = eglCreateContext(m_EglDisplay, m_EglConfig, EGL_NO_CONTEXT, ctxAttr);
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
//    m_VertexShader = CreateGLShader(std::string(vsSource.data(), vsSource.size()).c_str(), GL_VERTEX_SHADER);
//    m_FragShader = CreateGLShader(std::string(fsSource.data(), fsSource.size()).c_str(), GL_FRAGMENT_SHADER);
    m_VertexShader = CreateGLShader(vertexShader, GL_VERTEX_SHADER);
    m_FragShader = CreateGLShader(fragYUV420P, GL_FRAGMENT_SHADER);
    m_Program = CreateGLProgram(m_VertexShader, m_FragShader);
}