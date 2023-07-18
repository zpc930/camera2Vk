//
// Created by ts on 2023/7/18.
//
#include "Geometry.h"
#include "VulkanCommon.h"

bool Geometry::bUseIndexDraw() const {
    return !indices.empty();
}

uint32_t Geometry::getVertexCount() const {
    return vertices.size();
}

uint32_t Geometry::geIndexCount() const {
    return indices.size();
}

void Geometry::destroy(VkDevice device) {
    if(vertexBuffer != VK_NULL_HANDLE){
        vkDestroyBuffer(device, vertexBuffer, VK_ALLOC);
    }
    if(vertexMemory != VK_NULL_HANDLE){
        vkFreeMemory(device, vertexMemory, VK_ALLOC);
    }
    if(indexBuffer != VK_NULL_HANDLE){
        vkDestroyBuffer(device, indexBuffer, VK_ALLOC);
    }
    if(indexMemory != VK_NULL_HANDLE){
        vkFreeMemory(device, indexMemory, VK_ALLOC);
    }
    vertices.clear();
    indices.clear();

    for(int i = 0; i < uniformBuffers.size(); i++){
        vkDestroyBuffer(device, uniformBuffers[i], VK_ALLOC);
        vkFreeMemory(device, uniformMemorys[i], VK_ALLOC);
    }
}

