#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location=1) in vec2 v_texcoord;
layout(location=2) in vec3 v_normal;
layout(binding=1) uniform sampler2D tex_sampler_camera;

layout(location=0) out vec4 outColor;

void main(){
    outColor = texture(tex_sampler_camera, v_texcoord);
}