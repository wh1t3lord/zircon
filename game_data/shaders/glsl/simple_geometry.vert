#version 310 es

#extension GL_ANGLE_base_vertex_base_instance_shader_builtin : require
#extension GL_ANGLE_multi_draw : require

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
} input_instance_transforms;

// retrieve actual indices 
layout(std430, binding=2) buffer MappingOffsetsInstancedMatricies
{
	uint transform_offests[];
} input_mapping_offsets_for_instance_transforms;

flat out int grBaseInstance;
flat out int grDrawID;

void main()
{
	int draw_id = -10;
	draw_id = gl_DrawID;
	int base_instance = -100;
	base_instance = gl_BaseInstance;

	grBaseInstance = base_instance;
	grDrawID = draw_id;

	gl_Position = projection * view * vec4(inPos, 1.0);
}