#version 330 core
layout(location=0) in vec3 pos;
layout(location=1) in vec3 norm;
uniform mat4 mvp;
uniform mat4 mv;
uniform mat4 mv_3x3_it;
uniform mat4 m;
uniform mat4 mvp_light;

out vec3 view_pos;
out vec3 view_normal;
out vec3 world_pos;
out vec4 lightview_pos;
out vec3 world_normal;

void main()
{
    gl_Position = mvp * vec4(pos, 1.0);
    lightview_pos = mvp_light * vec4(pos, 1.0);
    view_pos = vec3(mv * vec4(pos, 1.0));
    view_normal = vec3(mv_3x3_it * vec4(norm, 1.0));
    world_pos = vec3(m * vec4(pos, 1.0));
    world_normal = vec3(m * vec4(norm, 1.0));
}