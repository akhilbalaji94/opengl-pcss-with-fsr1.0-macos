#version 330 core
layout(location=0) in vec3 pos;
layout(location=1) in vec3 norm;
layout(location=2) in vec2 txc;
uniform mat4 mvp;
uniform mat4 mv;
uniform mat4 m;
uniform mat4 mvp_light;
uniform mat4 model[18];
uniform mat4 mv_3x3_it[18];

out vec3 view_pos;
out vec3 view_normal;
out vec3 world_pos;
out vec3 world_normal;
out vec4 lightview_pos;
out vec2 tex_coord;

void main()
{
    gl_Position = mvp * model[gl_InstanceID] * vec4(pos, 1.0);
    lightview_pos = mvp_light * model[gl_InstanceID] * vec4(pos, 1.0);
    view_pos = vec3(mv * model[gl_InstanceID] * vec4(pos, 1.0));
    view_normal = vec3(mv_3x3_it[gl_InstanceID] * vec4(norm, 1.0));
    tex_coord = txc;
    world_pos = vec3(m * model[gl_InstanceID] * vec4(pos, 1.0));
    world_normal = vec3(m * vec4(norm, 1.0));
}