//
// Created by ts on 2023/7/18.
//

#ifndef CAMERA2VK_COMMON_H
#define CAMERA2VK_COMMON_H

#include <android/log.h>
#include <android/asset_manager.h>

#define LOG_TAG "Camera2Vk"

#ifndef ALOG
#define ALOG(priority, tag, fmt...) \
    __android_log_print(ANDROID_##priority, tag, fmt)
#endif

#ifndef Print
#define Print(...) ((void)ALOG(LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#endif

#ifndef Warn
#define Warn(...) ((void)ALOG(LOG_WARN, LOG_TAG, __VA_ARGS__))
#endif

#ifndef Error
#define Error(...) ((void)ALOG(LOG_ERROR, LOG_TAG, __VA_ARGS__))
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define SAFE_DELETE(p)       { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=NULL; } }

#endif //CAMERA2VK_COMMON_H
