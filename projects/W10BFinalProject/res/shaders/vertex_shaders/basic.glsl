#version 440

// Include our common vertex shader attributes and uniforms
#include "../fragments/vs_common.glsl"

void main() {

	gl_Position = u_ModelViewProjection * vec4(inPosition, 1.0);

	// Lecture 5
	// Pass vertex pos in world space to frag shader
	outWorldPos = (u_Model * vec4(inPosition, 1.0)).xyz;

	// Normals
	outNormal = mat3(u_NormalMatrix) * inNormal;

	// Pass our UV coords to the fragment shader
	outUV = inUV;

	///////////
	outColor = inColor;

}

