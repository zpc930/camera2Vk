#ifndef CAMERA2VK_VKSHADERPARAM_H
#define CAMERA2VK_VKSHADERPARAM_H

#include <array>
#include "glm/glm.hpp"
#include "glm/ext.hpp"

struct UniformObject{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec2 texcoord;
    glm::vec3 normal;

    static VkVertexInputBindingDescription getBindingDescription(){
        VkVertexInputBindingDescription bindingDescription = {
                .binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        };
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions(){
        std::array<VkVertexInputAttributeDescription, 3> attributeDesc{};
        attributeDesc[0] = {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(Vertex, pos)
        };
        attributeDesc[1] = {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(Vertex, texcoord)
        };
        attributeDesc[2] = {
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(Vertex, normal)
        };
        return attributeDesc;
    }
};

#endif //CAMERA2VK_VKSHADERPARAM_H
