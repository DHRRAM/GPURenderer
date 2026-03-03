#version 120

varying vec3 vNormal;
varying vec3 vPositionView;
varying vec2 vTexCoord;
varying vec3 vWorldNormal;
varying vec3 vWorldPosition;
varying vec4 vReflectionClip;
varying vec4 vLightClipPos;

uniform vec3 uLightPosView;
uniform vec3 uLightDirView;
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
uniform samplerCube uEnvMap;
uniform int uUseEnvMap;
uniform mat3 uInvViewRotation;
uniform float uEnvReflectionStrength;
uniform float uPlaneReflectionStrength;
uniform float uPlaneEnvStrength;
uniform float uPlaneReflectionBrightness;
uniform sampler2D uShadowMap;
uniform int uReceiveShadows;
uniform float uShadowBias;
uniform int uUseSpotLight;
uniform float uSpotCosInner;
uniform float uSpotCosOuter;

float ComputeSpotFactor() {
	if (uUseSpotLight == 0) {
		return 1.0;
	}
	vec3 lightToFrag = normalize(vPositionView - uLightPosView);
	float theta = dot(lightToFrag, normalize(uLightDirView));
	float denom = max(uSpotCosInner - uSpotCosOuter, 0.0001);
	return clamp((theta - uSpotCosOuter) / denom, 0.0, 1.0);
}

float ComputeShadowFactor(vec3 n, vec3 lightDir) {
	if (uReceiveShadows == 0) {
		return 1.0;
	}
	if (abs(vLightClipPos.w) <= 0.0001) {
		return 1.0;
	}
	vec3 projCoords = vLightClipPos.xyz / vLightClipPos.w;
	projCoords = projCoords * 0.5 + vec3(0.5);
	if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
		projCoords.y < 0.0 || projCoords.y > 1.0 ||
		projCoords.z <= 0.0 || projCoords.z >= 1.0) {
		return 1.0;
	}

	float baseBias = max(dot(n, lightDir), 0.0);
	float bias = max(uShadowBias * (2.0 - baseBias), 0.000005);
	float closestDepth = texture2D(uShadowMap, projCoords.xy).r;
	return (projCoords.z - bias <= closestDepth) ? 1.0 : 0.0;
}

void main() {
	if (uShadeMode == 1) {
		gl_FragColor = vec4(clamp(vNormal, 0.0, 1.0), 1.0);
		return;
	}
	if (uShadeMode == 2) {
		gl_FragColor = vec4(uMarkerColor, 1.0);
		return;
	}
	if (uShadeMode == 4) {
		vec3 dir = normalize(vWorldPosition);
		vec3 color = (uUseEnvMap == 1) ? textureCube(uEnvMap, dir).rgb : vec3(0.0);
		gl_FragColor = vec4(color, 1.0);
		return;
	}

	vec3 n = normalize(vNormal);
	vec3 viewDir = normalize(-vPositionView);
	vec3 lightDir = normalize(uLightPosView - vPositionView);
	float spotFactor = ComputeSpotFactor();
	float shadowFactor = ComputeShadowFactor(n, lightDir);
	float visibility = spotFactor * shadowFactor;
	float diff = max(dot(n, lightDir), 0.0);
	float spec = 0.0;
	if (diff > 0.0) {
		vec3 halfDir = normalize(lightDir + viewDir);
		spec = pow(max(dot(n, halfDir), 0.0), uShininess);
	}

	if (uShadeMode == 3) {
		vec3 planeColor = uDiffuseColor;
		if (uUseDiffuseMap == 1 && abs(vReflectionClip.w) > 0.0001) {
			vec2 projectedUv = (vReflectionClip.xy / vReflectionClip.w) * 0.5 + vec2(0.5);
			float inside =
				step(0.0, projectedUv.x) *
				step(0.0, projectedUv.y) *
				step(projectedUv.x, 1.0) *
				step(projectedUv.y, 1.0);
			vec3 projectedReflection = texture2D(uDiffuseMap, projectedUv).rgb * uPlaneReflectionBrightness;
			float reflectionMix = clamp(inside * uPlaneReflectionStrength, 0.0, 1.0);
			planeColor = mix(planeColor, projectedReflection, reflectionMix);
		}
		if (uUseEnvMap == 1) {
			vec3 incidentView = normalize(vPositionView);
			vec3 reflectedView = reflect(incidentView, n);
			vec3 reflectedWorld = normalize(uInvViewRotation * reflectedView);
			vec3 envColor = textureCube(uEnvMap, reflectedWorld).rgb;
			planeColor += envColor * uPlaneEnvStrength;
		}

		vec3 ambient = planeColor * uAmbientColor * uLightColor;
		vec3 diffuse = planeColor * diff * uLightColor * visibility;
		vec3 specular = uSpecularColor * spec * uLightColor * visibility;
		gl_FragColor = vec4(clamp(ambient + diffuse + specular + uPlaneColorBias, 0.0, 1.0), 1.0);
		return;
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
	vec3 diffuse = diffuseColor * diff * uLightColor * visibility;
	vec3 specular = specularColor * spec * uLightColor * visibility;
	vec3 color = ambient + diffuse + specular;

	if (uUseEnvMap == 1) {
		vec3 incidentView = normalize(vPositionView);
		vec3 reflectedView = reflect(incidentView, n);
		vec3 reflectedWorld = normalize(uInvViewRotation * reflectedView);
		vec3 envColor = textureCube(uEnvMap, reflectedWorld).rgb;
		float fresnel = pow(1.0 - max(dot(n, viewDir), 0.0), 5.0);
		float reflectionWeight = uEnvReflectionStrength * (0.6 + 0.4 * fresnel);
		color += envColor * reflectionWeight;
	}

	gl_FragColor = vec4(color, 1.0);
}
