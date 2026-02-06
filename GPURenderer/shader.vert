#version 120

attribute vec3 aPosition;
attribute vec3 aNormal;

uniform mat4 uMvp;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat3 uNormalMatrix;

varying vec3 vNormal;
varying vec3 vPositionView;

void main() {
	vec4 worldPos = uModel * vec4(aPosition, 1.0);
	vec4 viewPos = uView * worldPos;
	vPositionView = viewPos.xyz;
	vNormal = normalize(uNormalMatrix * aNormal);
	gl_Position = uMvp * vec4(aPosition, 1.0);
}
