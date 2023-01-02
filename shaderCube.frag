#version 330 core

in vec3 dir;
uniform samplerCube env;

layout(location = 0) out vec3 color;

void main()
{
    color = texture(env, dir).rgb;
}