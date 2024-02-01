//
// Created by ts on 2023/7/18.
//

#ifndef CAMERA2VK_COMMON_H
#define CAMERA2VK_COMMON_H

#include <android/log.h>
#include <android/asset_manager.h>
#include <ctime>

#define LOG_TAG "Camera2Vk"
//#define GRAPHIC_API_GLES
#define GRAPHIC_API_VK
//#define RENDER_USE_SINGLE_BUFFER

#ifndef ALOG
#define ALOG(priority, tag, fmt...) \
    __android_log_print(ANDROID_##priority, tag, fmt)
#endif

#ifndef LOG_D
#define LOG_D(...) ((void)ALOG(LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#endif

#ifndef LOG_W
#define LOG_W(...) ((void)ALOG(LOG_WARN, LOG_TAG, __VA_ARGS__))
#endif

#ifndef LOG_E
#define LOG_E(...) ((void)ALOG(LOG_ERROR, LOG_TAG, __VA_ARGS__))
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define SAFE_DELETE(p)       { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=NULL; } }

#define U_TIME_1MS_IN_NS (1000 * 1000)

static uint64_t getTimeNano(clockid_t clk_id = CLOCK_BOOTTIME){
    struct timespec t;
    clock_gettime(clk_id, &t);
    uint64_t result = t.tv_sec * 1000000000LL + t.tv_nsec;
    return result;
}

static void NanoSleep(uint64_t sleepTimeNano){
    timespec t, rem;
    t.tv_sec = 0;
    t.tv_nsec = sleepTimeNano;
    nanosleep(&t, &rem);
}

#endif //CAMERA2VK_COMMON_H
