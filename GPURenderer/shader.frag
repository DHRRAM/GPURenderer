#version 120

varying vec3 vNormal;
varying vec3 vPositionView;

uniform vec3 uLightPosView;
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;
uniform vec3 uDiffuseColor;
uniform vec3 uSpecularColor;
uniform vec3 uMarkerColor;
uniform float uShininess;
uniform int uShadeMode;

void main() {
	if (uShadeMode == 1) {
		gl_FragColor = vec4(clamp(vNormal, 0.0, 1.0), 1.0);
		return;
	}
	if (uShadeMode == 2) {
		gl_FragColor = vec4(uMarkerColor, 1.0);
		return;
	}

	vec3 n = normalize(vNormal);
	vec3 lightDir = normalize(uLightPosView - vPositionView);
	float diff = max(dot(n, lightDir), 0.0);

	vec3 viewDir = normalize(-vPositionView);
	vec3 halfDir = normalize(lightDir + viewDir);
	float spec = pow(max(dot(n, halfDir), 0.0), uShininess);

	vec3 ambient = uAmbientColor * uLightColor;
	vec3 diffuse = uDiffuseColor * diff * uLightColor;
	vec3 specular = uSpecularColor * spec * uLightColor;

	gl_FragColor = vec4(ambient + diffuse + specular, 1.0);
}
