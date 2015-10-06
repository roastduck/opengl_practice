#version 330

layout(location = 0) in vec4 vert;
layout(location = 1) in vec4 norm;

smooth out vec4 color;

uniform mat4 convert; // include projection.
uniform int lightCnt;
uniform vec4 lightDir[5]; // the length of it is the intensity.
uniform vec4 lightCol[5];

void main()
{
	gl_Position = vert * convert; // glsl matrices are column first stored.
	vec4 norm4 = norm * convert; // so these are left-multiply
	vec3 norm3 = vec3(norm4.x, norm4.y, norm4.z);
	color = vec4(0.0, 0.0, 0.0, 1.0);
	for (int i=0; i<lightCnt; i++)
	{
		vec3 light3 = vec3(lightDir[i].x, lightDir[i].y, lightDir[i].z);
		float ref = dot(norm3, light3) / length(norm3);
		color.x += ref*lightCol[i].x;
		color.y += ref*lightCol[i].y;
		color.z += ref*lightCol[i].z;
	}
}

