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

    struct android_app *mApp;
    bool bRunning = false;
    CameraImageReader *mImageReaderLeft;     // camera image reader
    CameraImageReader *mImageReaderRight;    // camera image reader
    CameraManager *mCameraLeft;              // camera left
    CameraManager *mCameraRight;             // camera right

    EGLDisplay m_EglDisplay = EGL_NO_DISPLAY;
    EGLSurface m_EglSurface = EGL_NO_SURFACE;
    EGLContext m_EglContext = EGL_NO_CONTEXT;
    EGLConfig m_EglConfig;
    GLint m_VertexShader;
    GLint m_FragShader;
    GLuint m_Program;
    GLuint yTexture;
    GLuint uvTexture;

    uint64_t mLastVsyncTimeNs = 0;
    uint64_t mVsyncCount = 0;
    uint64_t mLastFrameTime = 0;

    const char *vertexShader = "#version 300 es\n"
                         "layout(location=0) in vec2 a_position;\n"
                         "layout(location=1) in vec4 a_color;\n"
                         "layout(location=2) in vec2 a_texcoord;\n"
                         "out vec4 v_color;\n"
                         "out vec2 v_texcoord;\n"
                         "void main(){\n"
                         "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
                         "    v_color = a_color;\n"
                         "    v_texcoord = a_texcoord;\n"
                         "}";
    const char *fragYUV420P = "#version 300 es\n"
                              "precision mediump float;\n"
                              "in vec4 v_color;\n"
                              "in vec2 v_texcoord;\n"
                              "uniform sampler2D y_texture;\n"
                              "uniform sampler2D uv_texture;\n"
                              "\n"
                              "out vec4 FragColor;\n"
                              "\n"
                              "vec3 tex_yuv(vec2 tex_coord){\n"
                              "    vec3 yuv;\n"
                              "    yuv.x = texture(y_texture, tex_coord).r;\n"
                              "    yuv.y = texture(uv_texture, tex_coord).g - 0.5;\n"
                              "    yuv.z = texture(uv_texture, tex_coord).r - 0.5;\n"
                              "    return yuv;\n"
                              "}\n"
                              "\n"
                              "vec3 bt_8bit_yuv_to_rgb(vec3 yuv){\n"
                              "    return mat3(\n"
                              "        1.000000, 1.0000000, 1.00000,\n"
                              "        0.000000, -0.344000, 1.77200,\n"
                              "        1.402000, -0.714000, 0.00000\n"
                              "    ) * yuv;\n"
                              "}\n"
                              "\n"
                              "void main() {\n"
                              "    vec3 srcYuv = tex_yuv(v_texcoord);\n"
                              "    vec3 srcRgb = bt_8bit_yuv_to_rgb(srcYuv);\n"
                              "    FragColor = vec4(srcRgb, 1.0);\n"
                              "}";
};