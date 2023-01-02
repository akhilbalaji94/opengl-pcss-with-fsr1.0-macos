#version 330 core

layout(location = 0) in vec3 pos;

uniform mat4 mvp;
uniform mat4 model[18];

void main(){
	gl_Position = mvp * model[gl_InstanceID] * vec4(pos, 1.0);
}