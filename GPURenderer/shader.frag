#version 120

varying vec3 vNormal;
varying vec3 vPositionView;
varying vec2 vTexCoord;

uniform vec3 uLightPosView;
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;
uniform vec3 uDiffuseColor;
uniform vec3 uSpecularColor;
uniform vec3 uMarkerColor;
uniform float uShininess;
uniform int uShadeMode;
uniform sampler2D uDiffuseMap;
uniform sampler2D uSpecularMap;
uniform int uUseDiffuseMap;
uniform int uUseSpecularMap;
uniform vec3 uPlaneColorBias;

void main() {
	if (uShadeMode == 1) {
		gl_FragColor = vec4(clamp(vNormal, 0.0, 1.0), 1.0);
		return;
	}
	if (uShadeMode == 2) {
		gl_FragColor = vec4(uMarkerColor, 1.0);
		return;
	}
	if (uShadeMode == 3) {
		vec3 planeColor = texture2D(uDiffuseMap, vTexCoord).rgb + uPlaneColorBias;
		gl_FragColor = vec4(clamp(planeColor, 0.0, 1.0), 1.0);
		return;
	}

	vec3 n = normalize(vNormal);
	vec3 lightDir = normalize(uLightPosView - vPositionView);
	float cosTheta = dot(n, lightDir);
	float diff = max(cosTheta, 0.0);
	float spec = 0.0;
	if (diff > 0.0) {
		vec3 viewDir = normalize(-vPositionView);
		vec3 halfDir = normalize(lightDir + viewDir);
		spec = pow(max(dot(n, halfDir), 0.0), uShininess);
	}

	vec3 diffuseTex = (uUseDiffuseMap == 1)
		? texture2D(uDiffuseMap, vTexCoord).rgb
		: vec3(1.0);
	vec3 specularTex = (uUseSpecularMap == 1)
		? texture2D(uSpecularMap, vTexCoord).rgb
		: vec3(1.0);

	vec3 diffuseColor = (uUseDiffuseMap == 1) ? diffuseTex : uDiffuseColor;
	vec3 specularColor = uSpecularColor * specularTex;

	vec3 ambientColor = (uUseDiffuseMap == 1) ? (diffuseTex * uAmbientColor) : uAmbientColor;
	vec3 ambient = ambientColor * uLightColor;
	vec3 diffuse = diffuseColor * diff * uLightColor;
	vec3 specular = specularColor * spec * uLightColor;

	gl_FragColor = vec4(ambient + diffuse + specular, 1.0);
}
