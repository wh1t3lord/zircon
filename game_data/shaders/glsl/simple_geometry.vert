#version 310 es

precision highp float;

layout(std430) buffer;	

layout(shared, column_major) uniform;

layout (location = 0) in vec3 inPos;

layout (std140, binding=0) uniform CameraData
{
	mat4 projection;
	mat4 view;
};

layout(std430, binding=1) buffer InstancedMatriciesData
{
	mat4 transform[];
} input_instance_transforms;

void main()
{
	gl_Position = vec4(inPos, 1.0) * view * projection;
}