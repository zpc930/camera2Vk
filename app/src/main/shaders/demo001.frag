#version 450
#extension GL_ARB_separate_shader_objects : enable

precision mediump int;
precision highp float;
precision mediump sampler2D;

layout(location=1) in vec2 v_texcoord;
layout(binding=0) uniform sampler2D y_texture;
layout(binding=1) uniform sampler2D uv_texture;

layout(location=0) out vec4 FragColor;

void main(){
    vec3 yuv;
    yuv.x = texture(y_texture, v_texcoord).r;
    yuv.y = texture(uv_texture, v_texcoord).g - 0.5;
    yuv.z = texture(uv_texture, v_texcoord).r - 0.5;
    highp vec3 rgb = mat3( 1,       1,      1,
                           0,     -0.3455,  1.779,
                           1.4075, -0.7169,  0) * yuv;
    FragColor = vec4(rgb, 1.0);
//    FragColor = vec4(1, 0, 0, 1);
}