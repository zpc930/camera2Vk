#ifndef LEARN_VULKAN_VULKANCOMMON_H
#define LEARN_VULKAN_VULKANCOMMON_H

#include <cstdio>
#include <vector>
#include <array>
#include <fstream>
#include <sstream>
#include "vulkan_wrapper.h"
#include "Common.h"

#define VK_ALLOC nullptr
#define APP_NAME "Camera2Vk"

typedef struct
{
    const char      *name;
    bool			validationOnly;
    bool			required;
} DriverFeature;

static const char*
vk_error_string(VkResult result) {
    switch (result) {
        case VK_SUCCESS:
            return "VK_SUCCESS";
        case VK_NOT_READY:
            return "VK_NOT_READY";
        case VK_TIMEOUT:
            return "VK_TIMEOUT";
        case VK_EVENT_SET:
            return "VK_EVENT_SET";
        case VK_EVENT_RESET:
            return "VK_EVENT_RESET";
        case VK_INCOMPLETE:
            return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST:
            return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_SUBOPTIMAL_KHR:
            return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT:
            return "VK_ERROR_VALIDATION_FAILED_EXT";
        default: {
            if (result == VK_ERROR_INVALID_SHADER_NV) {
                return "VK_ERROR_INVALID_SHADER_NV";
            }
            return "unknown";
        }
    }
}

static void
vk_check_errors(VkResult result, const char *file_name, const int line_number, const char *function){
    if(VK_SUCCESS != result){
        std::stringstream ss;
        ss << "[" << file_name << "," << line_number << "]: " << function << ": " << vk_error_string(result);
        throw std::runtime_error(ss.str());
    }
}

/**
 * 层/扩展验证
 * @param label 提示名，用作日志输出使用
 * @param validationEnabled 是否启用验证，如果为否，直接跳过
 * @param extensions 是否属于扩展验证，层验证时为false，扩展验证时为true
 * @param requested 待验证列表
 * @param requestedCount 待验证数量
 * @param available 全部的功能列表，从vkEnumerateXxxLayerProperties或vkEnumerateXxxExtensionProperties获取
 * @param availableCount 全部功能数量
 * @param enabledNames 输出的可用功能名称数组
 * @param enabledCount 输出的可用功能数量
 * @return 操作是否成功
 */
static bool
checkFeatures(const char *label, const bool validationEnabled, const bool extensions,
                   const DriverFeature *requested, const uint32_t requestedCount,
                   const void *available, const uint32_t availableCount,
                   const char *enabledNames[], uint32_t *enabledCount){

#if 0
    Print("%s available: ", label);
    for(int i = 0; i < availableCount; i++){
        const char * name = extensions ? ((const VkExtensionProperties *)available)[i].extensionName : ((const VkLayerProperties *)available)[i].layerName;
        Print("%s", name);
    }
#endif
    bool foundAllRequired = true;
    *enabledCount = 0;
    for ( uint32_t i = 0; i < requestedCount; i++ )
    {
        bool found = false;
        const char *result = requested[i].required ? "(required, not found)" : "(not found)";
        for ( uint32_t j = 0; j < availableCount; j++ )
        {
            const char * name = extensions ? ((const VkExtensionProperties *)available)[j].extensionName :
                                ((const VkLayerProperties *)available)[j].layerName;
            if ( strcmp( requested[i].name, name ) == 0 )
            {
                found = true;
                if ( requested[i].validationOnly && !validationEnabled )
                {
                    result = "(not enabled)";
                    break;
                }
                enabledNames[(*enabledCount)++] = requested[i].name;
                result = requested[i].required ? "(required, enabled)" : "(enabled)";
                break;
            }
        }
        foundAllRequired &= ( found || !requested[i].required );
        Print( "%-21s%c %s %s", ( i == 0 ? label : "" ), ( i == 0 ? ':' : ' ' ), requested[i].name, result );
    }
    return foundAllRequired;
}

#define CALL_VK(func) vk_check_errors(func, __FILE_NAME__, __LINE__, #func);

#endif //LEARN_VULKAN_VULKANCOMMON_H