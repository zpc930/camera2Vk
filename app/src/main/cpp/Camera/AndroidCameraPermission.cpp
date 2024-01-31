#include "AndroidCameraPermission.h"
#include <stdexcept>
#include <string>

static jclass load_java_class(JNIEnv* a_env, jobject a_instance, const std::string& a_class_name)
{
    jclass NativeActivity = a_env->FindClass("android/app/NativeActivity");
    jclass ClassLoader = a_env->FindClass("java/lang/ClassLoader");

    jmethodID NativeActivity_getClassLoader =
            a_env->GetMethodID(NativeActivity, "getClassLoader", "()Ljava/lang/ClassLoader;");

    jobject ClassLoaderObj = a_env->CallObjectMethod(a_instance, NativeActivity_getClassLoader);

    jmethodID loadClass =
            a_env->GetMethodID(ClassLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

    jstring j_class_name = a_env->NewStringUTF(a_class_name.c_str());

    return static_cast<jclass>(a_env->CallObjectMethod(ClassLoaderObj, loadClass, j_class_name));
}

bool AndroidCameraPermission::isCameraPermitted(android_app *app) {
    auto instance = app->activity->clazz;
    auto env = app->activity->env;
    auto vm = app->activity->vm;

    if(vm->AttachCurrentThread(&env, nullptr) != JNI_OK)
        throw std::runtime_error("Could not attach thread to running VM.");

    jclass ContextCompat = load_java_class(env, instance, "androidx/core/content/ContextCompat");
    jmethodID ContextCompat_checkSelfPermission =
            env->GetStaticMethodID(ContextCompat, "checkSelfPermission",
                                   "(Landroid/content/Context;Ljava/lang/String;)I");
    jstring camera_permission = env->NewStringUTF("android.permission.CAMERA");
    jint result = env->CallStaticIntMethod(ContextCompat, ContextCompat_checkSelfPermission,
                                           instance, camera_permission);
    vm->DetachCurrentThread();
    return result == 0;
}

void AndroidCameraPermission::requestCameraPermission(android_app *app) {
    auto instance = app->activity->clazz;
    auto env = app->activity->env;
    auto vm = app->activity->vm;

    if(vm->AttachCurrentThread(&env, nullptr) != JNI_OK)
        throw std::runtime_error("Could not attach thread to running VM.");

    jclass ActivityCompat = load_java_class(env, instance, "androidx/core/app/ActivityCompat");

    jmethodID ActivityCompat_requestPermissions =
            env->GetStaticMethodID(ActivityCompat, "requestPermissions",
                                   "(Landroid/app/Activity;[Ljava/lang/String;I)V");

    jstring camera_permission = env->NewStringUTF("android.permission.CAMERA");
    jobjectArray perms = env->NewObjectArray(1, env->FindClass("java/lang/String"), nullptr);
    env->SetObjectArrayElement(perms, 0, camera_permission);
    env->CallStaticVoidMethod(ActivityCompat, ActivityCompat_requestPermissions, instance, perms, 1);

    vm->DetachCurrentThread();
}
