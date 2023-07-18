#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location=0) in vec3 in_pos;
layout(location=1) in vec2 in_texcoord;
layout(location=2) in vec3 in_normal;

out gl_PerVertex {
    vec4 gl_Position;
};

layout(location=1) out vec2 v_texcoord;
layout(location=2) out vec3 v_normal;

void main(){
    gl_Position = vec4(in_pos.x, -in_pos.y, in_pos.z, 1.0f);    //reverse Y, same as OpenGL
    v_texcoord = in_texcoord;
    v_normal = in_normal;
}