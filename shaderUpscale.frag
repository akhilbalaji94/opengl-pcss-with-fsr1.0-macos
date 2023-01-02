#version 330 core

in vec2 tex_coord;

layout(location = 0) out vec4 color;

uniform sampler2D texture_sampler;

void main()
{
    color = texture(texture_sampler, tex_coord);
}