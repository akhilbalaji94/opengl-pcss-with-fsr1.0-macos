#version 330 core

layout(location = 0) out vec3 color;

void main()
{
    vec3 light_object = vec3(1, 1, 1);
    vec3 light_color = vec3(1, 1, 1);
    color = light_color * light_object;
}