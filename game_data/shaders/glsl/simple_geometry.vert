#version 310 es

precision highp float;

layout(std430) buffer;	

layout (location = 0) in vec3 inPos;

layout (std140, binding=0) uniform CameraData
{
	mat4 projection;
	mat4 view;
};

layout(std430, binding=1) buffer InstancedMatriciesData
{
	mat4 transform[];
};


void main()
{
	gl_Position = projection * view * transform[gl_InstanceID] * vec4(inPos.x, inPos.y, inPos.z, 1.0);
}