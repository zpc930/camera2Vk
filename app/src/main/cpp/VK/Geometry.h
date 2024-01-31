/*!
 * @brief  几何体
 * @author cxh
 * @date 2023/7/18
 */
#ifndef CAMERA2VK_GEOMETRY_H
#define CAMERA2VK_GEOMETRY_H

#include <cstdint>
#include <vector>
#include "vulkan_wrapper.h"
#include "VkShaderParam.h"

class Geometry {
public:
    bool bUseIndexDraw() const;
    uint32_t getVertexCount() const;
    uint32_t geIndexCount() const;
    void destroy(VkDevice device);
public:
    std::vector<Vertex>  vertices;
    std::vector<uint32_t> indices;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexMemory;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformMemorys;
    std::vector<void*> uniformBuffersMapped;
};


#endif //CAMERA2VK_GEOMETRY_H
