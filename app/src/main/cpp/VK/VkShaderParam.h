#ifndef CAMERA2VK_VKSHADERPARAM_H
#define CAMERA2VK_VKSHADERPARAM_H

#include <array>
#include "glm/glm.hpp"
#include "glm/ext.hpp"

struct Vertex {
    glm::vec2 pos;
    glm::vec2 texcoord;

    static VkVertexInputBindingDescription getBindingDescription(){
        VkVertexInputBindingDescription bindingDescription = {
                .binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        };
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions(){
        std::array<VkVertexInputAttributeDescription, 2> attributeDesc{};
        attributeDesc[0] = {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(Vertex, pos)
        };
        attributeDesc[1] = {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(Vertex, texcoord)
        };
        return attributeDesc;
    }
};

#endif //CAMERA2VK_VKSHADERPARAM_H
