#version 310 es

precision highp float;

out vec4 FragColor;

flat in int grDrawID;
flat in int grBaseInstance;

void main()
{
	FragColor = vec4(grDrawID, float(grBaseInstance)/255.0, 0.0, 1.0);
}