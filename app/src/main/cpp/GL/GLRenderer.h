#pragma once

#include <cstdint>
#include <EGL/egl.h>
#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2platform.h>
#include "../Camera/CameraImageReader.h"
#include "../Camera/CameraManager.h"

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

class GLRenderer{
public:
    void Init(struct android_app *app);
    void Destroy();
    bool IsRunning();
    void ProcessFrame(uint64_t frameIndex);
private:
    void OpenCameras();
    void CloseCameras();
    int InitEGLEnv();
    void DestroyEGLEnv();
    std::vector<char> ReadFileFromAndroidRes(const std::string& filePath);
    void CreateProgram();
    void UpdateTextures(uint32_t eyeIndex, const AImage *image);
    void RenderSubArea(const AImage *image, RenderMeshArea area);

    struct android_app *mApp;
    bool bRunning = false;
    CameraImageReader *mImageReaderLeft;     // camera image reader
    CameraImageReader *mImageReaderRight;    // camera image reader
    CameraManager *mCameraLeft;              // camera left
    CameraManager *mCameraRight;             // camera right

    EGLDisplay m_EglDisplay = EGL_NO_DISPLAY;
    EGLSurface m_EglSurface = EGL_NO_SURFACE;
    EGLContext m_EglContext = EGL_NO_CONTEXT;
    EGLConfig mEglConfig;
    GLint mVertexShader;
    GLint mFragShader;
    GLuint mProgram;
    GLuint mLeftTextureY;
    GLuint mLeftTextureUV;
    GLuint mRightTextureY;
    GLuint mRightTextureUV;

    uint64_t mLastVsyncTimeNs = 0;
    uint64_t mVsyncCount = 0;
    uint64_t mLastFrameTime = 0;

    const char *vertexShader = "#version 300 es\n"
                         "layout(location=0) in vec2 a_position;\n"
                         "layout(location=2) in vec2 a_texcoord;\n"
                         "out vec2 v_texcoord;\n"
                         "void main(){\n"
                         "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
                         "    v_texcoord = a_texcoord;\n"
                         "}";
    const char *fragYUV420P = "#version 300 es\n"
                              "precision mediump float;\n"
                              "in vec2 v_texcoord;\n"
                              "uniform sampler2D y_texture;\n"
                              "uniform sampler2D uv_texture;\n"
                              "out vec4 FragColor;\n"
                              "void main() {\n"
                              "    vec3 yuv;\n"
                              "    yuv.x = texture(y_texture, v_texcoord).r;\n"
                              "    yuv.y = texture(uv_texture, v_texcoord).g - 0.5;\n"
                              "    yuv.z = texture(uv_texture, v_texcoord).r - 0.5;\n"
                              "    highp vec3 rgb = mat3( 1,       1,      1,        \n"
                              "                           0,     -0.3455,  1.779,    \n"
                              "                           1.4075, -0.7169,  0) * yuv;\n"
                              "    FragColor = vec4(rgb, 1.0);\n"
                              "}";
};