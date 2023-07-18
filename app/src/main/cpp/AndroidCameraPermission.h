//
// Created by ts on 2023/7/18.
//

#ifndef CAMERA2VK_ANDROIDCAMERAPERMISSION_H
#define CAMERA2VK_ANDROIDCAMERAPERMISSION_H

#include <android_native_app_glue.h>

class AndroidCameraPermission{
public:
    static bool isCameraPermitted(android_app *app);
    static void requestCameraPermission(android_app *app);
};

#endif //CAMERA2VK_ANDROIDCAMERAPERMISSION_H
