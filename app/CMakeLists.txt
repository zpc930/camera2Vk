cmake_minimum_required(VERSION 3.22.1)

project("camera2vk")

add_definitions(-DVK_USE_PLATFORM_ANDROID_KHR)

# build native_app_glue as a static lib
set(APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
include_directories(${APP_GLUE_DIR})
add_library(app-glue STATIC ${APP_GLUE_DIR}/android_native_app_glue.c)

# Export ANativeActivity_onCreate(),
# Refer to: https://github.com/android-ndk/ndk/issues/381.
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

# build vulkan app
set(SRC_JNI_DIR src/main/cpp)
set(WRAPPER_DIR src/vulkan_wrapper)

add_subdirectory(external/glm)

add_library(camera2vk SHARED
        ${WRAPPER_DIR}/vulkan_wrapper.h
        ${WRAPPER_DIR}/vulkan_wrapper.c
        ${SRC_JNI_DIR}/Camera/AndroidCameraPermission.cpp
        ${SRC_JNI_DIR}/Camera/AndroidCameraPermission.h
        ${SRC_JNI_DIR}/Camera/CameraManager.cpp
        ${SRC_JNI_DIR}/Camera/CameraManager.h
        ${SRC_JNI_DIR}/Camera/CameraImageReader.cpp
        ${SRC_JNI_DIR}/Camera/CameraImageReader.h

        ${SRC_JNI_DIR}/VK/VulkanCommon.h
        ${SRC_JNI_DIR}/VK/VkBundle.h
        ${SRC_JNI_DIR}/VK/VkHelper.cpp
        ${SRC_JNI_DIR}/VK/VkHelper.h
        ${SRC_JNI_DIR}/VK/VkShaderParam.h
        ${SRC_JNI_DIR}/VK/VkCameraImage.cpp
        ${SRC_JNI_DIR}/VK/VkCameraImage.h
        ${SRC_JNI_DIR}/VK/Geometry.cpp
        ${SRC_JNI_DIR}/VK/Geometry.h
        ${SRC_JNI_DIR}/VK/Texture.cpp
        ${SRC_JNI_DIR}/VK/Texture.h

        ${SRC_JNI_DIR}/GL/GLRenderer.cpp
        ${SRC_JNI_DIR}/GL/GLRenderer.h

        ${SRC_JNI_DIR}/Common.h
        ${SRC_JNI_DIR}/FpsCollector.cpp
        ${SRC_JNI_DIR}/FpsCollector.h
        ${SRC_JNI_DIR}/Main.cpp
        )
include_directories(${WRAPPER_DIR} external/stb_image)

target_link_libraries(camera2vk app-glue log android dl mediandk camera2ndk glm EGL GLESv3)