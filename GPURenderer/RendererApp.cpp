#include <GLFW/glfw3.h>

#include "RendererApp.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl2.h>
#include <backends/imgui_impl_opengl3.h>

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH 0x8B84
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif
#ifndef GL_TEXTURE2
#define GL_TEXTURE2 0x84C2
#endif
#ifndef GL_TEXTURE3
#define GL_TEXTURE3 0x84C3
#endif
#ifndef GL_TEXTURE_CUBE_MAP
#define GL_TEXTURE_CUBE_MAP 0x8513
#endif
#ifndef GL_TEXTURE_CUBE_MAP_POSITIVE_X
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515
#endif
#ifndef GL_TEXTURE_CUBE_MAP_NEGATIVE_X
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X 0x8516
#endif
#ifndef GL_TEXTURE_CUBE_MAP_POSITIVE_Y
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y 0x8517
#endif
#ifndef GL_TEXTURE_CUBE_MAP_NEGATIVE_Y
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y 0x8518
#endif
#ifndef GL_TEXTURE_CUBE_MAP_POSITIVE_Z
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z 0x8519
#endif
#ifndef GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z 0x851A
#endif
#ifndef GL_LUMINANCE
#define GL_LUMINANCE 0x1909
#endif
#ifndef GL_LUMINANCE_ALPHA
#define GL_LUMINANCE_ALPHA 0x190A
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER 0x8D41
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT 0x8D00
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24 0x81A6
#endif
#ifndef GL_DEPTH_COMPONENT
#define GL_DEPTH_COMPONENT 0x1902
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_TEXTURE_WRAP_R
#define GL_TEXTURE_WRAP_R 0x8072
#endif
#ifndef GL_LINEAR_MIPMAP_LINEAR
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#endif
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif
#ifndef GL_NONE
#define GL_NONE 0
#endif
#ifndef GL_POLYGON_OFFSET_FILL
#define GL_POLYGON_OFFSET_FILL 0x8037
#endif

namespace {
	constexpr float kRotationSpeedDegPerPixel = 0.5f;
	constexpr float kLightRotationSpeedDegPerPixel = 0.6f;
	constexpr float kZoomSpeed = 1.0f;
	constexpr float kMinCameraDistance = 0.1f;
	constexpr float kDefaultFovDeg = 45.0f;
	constexpr float kNearPlane = 0.1f;
	constexpr float kFarPlane = 500.0f;
	constexpr float kLightMarkerPointSize = 8.0f;
	constexpr float kLightMarkerMeshScaleRatio = 0.08f;
	constexpr float kLightMarkerMeshScaleMin = 0.08f;
	constexpr float kPlaneColorBias = 0.015f;
	constexpr float kPlaneAmbientStrength = 0.12f;
	constexpr float kPlaneSpecularStrength = 0.04f;
	constexpr float kPlaneShininess = 24.0f;
	constexpr float kPlaneDiffuseR = 0.40f;
	constexpr float kPlaneDiffuseG = 0.42f;
	constexpr float kPlaneDiffuseB = 0.45f;
	constexpr float kPlanePaddingRatio = 0.005f;
	constexpr float kPlaneSizeMultiplier = 1.0f;
	constexpr float kObjectEnvReflectionStrength = 1.0f;
	constexpr float kPlaneEnvReflectionStrength = 1.0f;
	constexpr float kPlaneRttReflectionStrength = 1.0f;
	constexpr float kPlaneRttReflectionBrightness = 0.25f;
	constexpr float kSkyboxDepth = 1.0f;
	constexpr float kGuiPanelViewportPaddingFraction = 0.01f;
	constexpr float kGuiPanelMinWidth = 280.0f;
	constexpr float kGuiPanelMinHeight = 200.0f;
	constexpr float kGuiPanelWidth = 460.0f;
	constexpr float kGuiPanelHeight = 500.0f;
	constexpr int kShadowMapResolution = 2048;
	constexpr float kShadowBias = 0.00008f;
	constexpr float kSpotLightInnerDeg = 18.0f;
	constexpr float kSpotLightOuterDeg = 28.0f;
	constexpr size_t kMaxObjPathLength = 1024;
	const std::array<const char*, 6> kCubemapFaceFiles{
		"cubemap_posx.png",
		"cubemap_negx.png",
		"cubemap_posy.png",
		"cubemap_negy.png",
		"cubemap_posz.png",
		"cubemap_negz.png"
	};
	const std::array<GLenum, 6> kCubemapFaceTargets{
		GL_TEXTURE_CUBE_MAP_POSITIVE_X,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
	};

	struct Bounds {
		glm::vec3 min{0.0f};
		glm::vec3 max{0.0f};
		bool valid = false;

		void Expand(const glm::vec3& v) {
			if (!valid) {
				min = v;
				max = v;
				valid = true;
				return;
			}
			min.x = std::min(min.x, v.x);
			min.y = std::min(min.y, v.y);
			min.z = std::min(min.z, v.z);
			max.x = std::max(max.x, v.x);
			max.y = std::max(max.y, v.y);
			max.z = std::max(max.z, v.z);
		}

		glm::vec3 Center() const {
			return glm::vec3(
				0.5f * (min.x + max.x),
				0.5f * (min.y + max.y),
				0.5f * (min.z + max.z));
		}

		float MaxExtent() const {
			const glm::vec3 size = max - min;
			return std::max(size.x, std::max(size.y, size.z));
		}
	};

	struct Vertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 texcoord;
	};

	struct Material {
		std::string name;
		glm::vec3 ambient{0.15f, 0.15f, 0.18f};
		glm::vec3 diffuse{0.8f, 0.7f, 0.65f};
		glm::vec3 specular{0.9f, 0.9f, 0.9f};
		float shininess = 64.0f;
		GLuint diffuseTexture = 0;
		GLuint specularTexture = 0;
		bool hasDiffuseTexture = false;
		bool hasSpecularTexture = false;
	};

	struct Submesh {
		int indexOffset = 0;
		int indexCount = 0;
		int materialIndex = 0;
	};

	struct OrbitCamera {
		float yawDeg = 0.0f;
		float pitchDeg = 0.0f;
		float distance = 4.0f;
		float panX = 0.0f;
		float panY = 0.0f;
	};

	struct GLRenderTexture {
		GLuint framebuffer = 0;
		GLuint colorTexture = 0;
		GLuint depthRenderbuffer = 0;
		int width = 0;
		int height = 0;
	};

	struct GLShadowMap {
		GLuint framebuffer = 0;
		GLuint depthTexture = 0;
		int width = 0;
		int height = 0;
	};

	struct GLMeshBuffers {
		GLuint vao = 0;
		GLuint vbo = 0;
		GLuint ebo = 0;
		int indexCount = 0;
	};

	enum class LightMarkerShape {
		Point = 0,
		Cube = 1,
		Sphere = 2,
		LightObject = 3
	};

	const char* kFallbackVertexShader = R"(#version 120

attribute vec3 aPosition;
attribute vec3 aNormal;
attribute vec2 aTexCoord;

uniform mat4 uMvp;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat3 uNormalMatrix;

varying vec3 vNormal;
varying vec3 vPositionView;
varying vec2 vTexCoord;

void main() {
	vec4 worldPos = uModel * vec4(aPosition, 1.0);
	vec4 viewPos = uView * worldPos;
	vPositionView = viewPos.xyz;
	vNormal = normalize(uNormalMatrix * aNormal);
	vTexCoord = aTexCoord;
	gl_Position = uMvp * vec4(aPosition, 1.0);
}
)";

const char* kFallbackFragmentShader = R"(#version 120

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
)";

	const char* kDepthVertexShader = R"(#version 120

attribute vec3 aPosition;
uniform mat4 uDepthMvp;

void main() {
	gl_Position = uDepthMvp * vec4(aPosition, 1.0);
}
)";

	const char* kDepthFragmentShader = R"(#version 120

void main() {
}
)";

	int gWindowWidth = gpurenderer::config::kDefaultWindowWidth;
	int gWindowHeight = gpurenderer::config::kDefaultWindowHeight;
	bool gUsePerspective = true;
	bool gLeftDown = false;
	bool gRightDown = false;
	bool gMiddleDown = false;
	double gLastMouseX = 0.0;
	double gLastMouseY = 0.0;
	OrbitCamera gObjectCamera;
	float gLightYawDeg = 45.0f;
	float gLightPitchDeg = 20.0f;
	float gLightDistance = 6.0f;
	bool gShowNormals = false;
	float gNearPlane = kNearPlane;
	float gFarPlane = kFarPlane;
	float gPlaneHeight = -1.0f;
	float gPlaneWidth = 12.0f;
	float gPlaneLength = 12.0f;
	float gPlaneScale = 1.0f;
	glm::vec3 gObjectScale(1.0f, 1.0f, 1.0f);
	float gLightMarkerScale = 1.0f;

	std::string gObjPath;
	std::string gVertexShaderPath;
	std::string gFragmentShaderPath;
	std::filesystem::path gAssetRoot;
	std::string gEnvironmentLoadStatus;

	std::vector<Vertex> gVertices;
	std::vector<unsigned int> gIndices;
	std::vector<Submesh> gSubmeshes;
	std::vector<Material> gMaterials;
	std::unordered_map<std::string, GLuint> gTextureCache;
	int gIndexCount = 0;
	Bounds gBounds;
	glm::vec3 gCenter(0.0f);

	GLFWwindow* gWindow = nullptr;
	enum class GuiBackend {
		None,
		OpenGL2,
		OpenGL3
	};
	GuiBackend gGuiBackend = GuiBackend::None;
	bool gGuiInitialized = false;
	bool gRenderToPlane = true;
	glm::vec3 gSceneBackgroundColor(0.03f, 0.03f, 0.05f);
	glm::vec3 gOffscreenBackgroundColor(0.05f, 0.05f, 0.08f);
	glm::vec3 gLightColor(1.0f, 1.0f, 1.0f);
	glm::vec3 gLightMarkerColor(1.0f, 0.9f, 0.1f);
	float gLightIntensity = 1.0f;
	glm::vec3 gPlaneAmbientColor(kPlaneAmbientStrength, kPlaneAmbientStrength, kPlaneAmbientStrength);
	glm::vec3 gPlaneDiffuseColor(kPlaneDiffuseR, kPlaneDiffuseG, kPlaneDiffuseB);
	glm::vec3 gPlaneSpecularColor(kPlaneSpecularStrength, kPlaneSpecularStrength, kPlaneSpecularStrength);
	float gPlaneShininess = kPlaneShininess;
	glm::vec3 gPlaneColorBias(kPlaneColorBias, kPlaneColorBias, kPlaneColorBias);
	float gPlaneEnvReflectionStrength = kPlaneEnvReflectionStrength;
	float gPlaneRttReflectionStrength = kPlaneRttReflectionStrength;
	float gPlaneRttReflectionBrightness = kPlaneRttReflectionBrightness;
	int gSelectedMaterialIndex = 0;
	char gObjPathInput[kMaxObjPathLength]{};
	std::vector<std::string> gAvailableObjFiles;
	int gSelectedObjIndex = -1;
	std::string gModelLoadStatus;
	GLuint gVao = 0;
	GLuint gVbo = 0;
	GLuint gEbo = 0;
	GLuint gPlaneVao = 0;
	GLuint gPlaneVbo = 0;
	GLuint gPlaneEbo = 0;
	int gPlaneIndexCount = 0;
	GLuint gBackgroundVao = 0;
	GLuint gBackgroundVbo = 0;
	GLuint gBackgroundEbo = 0;
	int gBackgroundIndexCount = 0;
	GLuint gLightVao = 0;
	GLuint gLightVbo = 0;
	GLMeshBuffers gLightCubeMesh;
	GLMeshBuffers gLightSphereMesh;
	GLMeshBuffers gLightObjectMesh;
	bool gShowLightMarker = true;
	int gLightMarkerShape = static_cast<int>(LightMarkerShape::Point);
	bool gHasLightObjectMesh = false;
	std::string gLightObjectStatus;
	GLRenderTexture gRenderTexture;
	GLShadowMap gShadowMap;
	GLuint gEnvironmentCubemap = 0;
	bool gHasAnisotropicFiltering = false;
	float gMaxAnisotropy = 1.0f;
	GLuint gProgram = 0;
	GLuint gDepthProgram = 0;
	GLint gMvpLocation = -1;
	GLint gModelLocation = -1;
	GLint gViewLocation = -1;
	GLint gNormalMatrixLocation = -1;
	GLint gWorldNormalMatrixLocation = -1;
	GLint gLightPosLocation = -1;
	GLint gLightColorLocation = -1;
	GLint gAmbientLocation = -1;
	GLint gDiffuseLocation = -1;
	GLint gSpecularLocation = -1;
	GLint gMarkerColorLocation = -1;
	GLint gShininessLocation = -1;
	GLint gShadeModeLocation = -1;
	GLint gDiffuseMapLocation = -1;
	GLint gSpecularMapLocation = -1;
	GLint gUseDiffuseMapLocation = -1;
	GLint gUseSpecularMapLocation = -1;
	GLint gPlaneColorBiasLocation = -1;
	GLint gEnvMapLocation = -1;
	GLint gUseEnvMapLocation = -1;
	GLint gInvViewRotationLocation = -1;
	GLint gEnvReflectionStrengthLocation = -1;
	GLint gPlaneReflectionStrengthLocation = -1;
	GLint gPlaneEnvStrengthLocation = -1;
	GLint gPlaneReflectionBrightnessLocation = -1;
	GLint gReflectionViewProjLocation = -1;
	GLint gLightDirLocation = -1;
	GLint gLightViewProjLocation = -1;
	GLint gShadowMapLocation = -1;
	GLint gReceiveShadowsLocation = -1;
	GLint gShadowBiasLocation = -1;
	GLint gUseSpotLightLocation = -1;
	GLint gSpotCosInnerLocation = -1;
	GLint gSpotCosOuterLocation = -1;
	GLint gDepthMvpLocation = -1;
	glm::mat4 gLightViewProjection(1.0f);
	glm::vec3 gLightWorldPosition(0.0f, 0.0f, 0.0f);
	glm::vec3 gLightWorldDirection(0.0f, -1.0f, 0.0f);
	float gShadowBias = kShadowBias;
	float gSpotInnerDeg = kSpotLightInnerDeg;
	float gSpotOuterDeg = kSpotLightOuterDeg;
	bool gUseVao = false;

	using GlGenVertexArraysProc = void (APIENTRYP)(GLsizei, GLuint*);
	using GlBindVertexArrayProc = void (APIENTRYP)(GLuint);
	using GlGenBuffersProc = void (APIENTRYP)(GLsizei, GLuint*);
	using GlBindBufferProc = void (APIENTRYP)(GLenum, GLuint);
	using GlBufferDataProc = void (APIENTRYP)(GLenum, std::ptrdiff_t, const void*, GLenum);
	using GlEnableVertexAttribArrayProc = void (APIENTRYP)(GLuint);
	using GlVertexAttribPointerProc = void (APIENTRYP)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
	using GlCreateShaderProc = GLuint (APIENTRYP)(GLenum);
	using GlShaderSourceProc = void (APIENTRYP)(GLuint, GLsizei, const char* const*, const GLint*);
	using GlCompileShaderProc = void (APIENTRYP)(GLuint);
	using GlGetShaderivProc = void (APIENTRYP)(GLuint, GLenum, GLint*);
	using GlGetShaderInfoLogProc = void (APIENTRYP)(GLuint, GLsizei, GLsizei*, char*);
	using GlDeleteShaderProc = void (APIENTRYP)(GLuint);
	using GlCreateProgramProc = GLuint (APIENTRYP)(void);
	using GlAttachShaderProc = void (APIENTRYP)(GLuint, GLuint);
	using GlBindAttribLocationProc = void (APIENTRYP)(GLuint, GLuint, const char*);
	using GlLinkProgramProc = void (APIENTRYP)(GLuint);
	using GlGetProgramivProc = void (APIENTRYP)(GLuint, GLenum, GLint*);
	using GlGetProgramInfoLogProc = void (APIENTRYP)(GLuint, GLsizei, GLsizei*, char*);
	using GlDeleteProgramProc = void (APIENTRYP)(GLuint);
	using GlUseProgramProc = void (APIENTRYP)(GLuint);
	using GlGetUniformLocationProc = GLint (APIENTRYP)(GLuint, const char*);
	using GlUniformMatrix4fvProc = void (APIENTRYP)(GLint, GLsizei, GLboolean, const GLfloat*);
	using GlUniformMatrix3fvProc = void (APIENTRYP)(GLint, GLsizei, GLboolean, const GLfloat*);
	using GlUniform3fvProc = void (APIENTRYP)(GLint, GLsizei, const GLfloat*);
	using GlUniform1fProc = void (APIENTRYP)(GLint, GLfloat);
	using GlUniform1iProc = void (APIENTRYP)(GLint, GLint);
	using GlActiveTextureProc = void (APIENTRYP)(GLenum);
	using GlDeleteBuffersProc = void (APIENTRYP)(GLsizei, const GLuint*);
	using GlDeleteVertexArraysProc = void (APIENTRYP)(GLsizei, const GLuint*);
	using GlGenFramebuffersProc = void (APIENTRYP)(GLsizei, GLuint*);
	using GlBindFramebufferProc = void (APIENTRYP)(GLenum, GLuint);
	using GlFramebufferTexture2DProc = void (APIENTRYP)(GLenum, GLenum, GLenum, GLuint, GLint);
	using GlCheckFramebufferStatusProc = GLenum (APIENTRYP)(GLenum);
	using GlDeleteFramebuffersProc = void (APIENTRYP)(GLsizei, const GLuint*);
	using GlGenRenderbuffersProc = void (APIENTRYP)(GLsizei, GLuint*);
	using GlBindRenderbufferProc = void (APIENTRYP)(GLenum, GLuint);
	using GlRenderbufferStorageProc = void (APIENTRYP)(GLenum, GLenum, GLsizei, GLsizei);
	using GlFramebufferRenderbufferProc = void (APIENTRYP)(GLenum, GLenum, GLenum, GLuint);
	using GlDeleteRenderbuffersProc = void (APIENTRYP)(GLsizei, const GLuint*);
	using GlGenerateMipmapProc = void (APIENTRYP)(GLenum);

	GlGenVertexArraysProc pglGenVertexArrays = nullptr;
	GlBindVertexArrayProc pglBindVertexArray = nullptr;
	GlGenBuffersProc pglGenBuffers = nullptr;
	GlBindBufferProc pglBindBuffer = nullptr;
	GlBufferDataProc pglBufferData = nullptr;
	GlEnableVertexAttribArrayProc pglEnableVertexAttribArray = nullptr;
	GlVertexAttribPointerProc pglVertexAttribPointer = nullptr;
	GlCreateShaderProc pglCreateShader = nullptr;
	GlShaderSourceProc pglShaderSource = nullptr;
	GlCompileShaderProc pglCompileShader = nullptr;
	GlGetShaderivProc pglGetShaderiv = nullptr;
	GlGetShaderInfoLogProc pglGetShaderInfoLog = nullptr;
	GlDeleteShaderProc pglDeleteShader = nullptr;
	GlCreateProgramProc pglCreateProgram = nullptr;
	GlAttachShaderProc pglAttachShader = nullptr;
	GlBindAttribLocationProc pglBindAttribLocation = nullptr;
	GlLinkProgramProc pglLinkProgram = nullptr;
	GlGetProgramivProc pglGetProgramiv = nullptr;
	GlGetProgramInfoLogProc pglGetProgramInfoLog = nullptr;
	GlDeleteProgramProc pglDeleteProgram = nullptr;
	GlUseProgramProc pglUseProgram = nullptr;
	GlGetUniformLocationProc pglGetUniformLocation = nullptr;
	GlUniformMatrix4fvProc pglUniformMatrix4fv = nullptr;
	GlUniformMatrix3fvProc pglUniformMatrix3fv = nullptr;
	GlUniform3fvProc pglUniform3fv = nullptr;
	GlUniform1fProc pglUniform1f = nullptr;
	GlUniform1iProc pglUniform1i = nullptr;
	GlActiveTextureProc pglActiveTexture = nullptr;
	GlDeleteBuffersProc pglDeleteBuffers = nullptr;
	GlDeleteVertexArraysProc pglDeleteVertexArrays = nullptr;
	GlGenFramebuffersProc pglGenFramebuffers = nullptr;
	GlBindFramebufferProc pglBindFramebuffer = nullptr;
	GlFramebufferTexture2DProc pglFramebufferTexture2D = nullptr;
	GlCheckFramebufferStatusProc pglCheckFramebufferStatus = nullptr;
	GlDeleteFramebuffersProc pglDeleteFramebuffers = nullptr;
	GlGenRenderbuffersProc pglGenRenderbuffers = nullptr;
	GlBindRenderbufferProc pglBindRenderbuffer = nullptr;
	GlRenderbufferStorageProc pglRenderbufferStorage = nullptr;
	GlFramebufferRenderbufferProc pglFramebufferRenderbuffer = nullptr;
	GlDeleteRenderbuffersProc pglDeleteRenderbuffers = nullptr;
	GlGenerateMipmapProc pglGenerateMipmap = nullptr;

	float DegreesToRadians(float degrees) {
		return degrees * 3.14159265358979323846f / 180.0f;
	}

	std::string ReadFileText(const std::string& path) {
		std::ifstream file(path);
		if (!file) {
			return {};
		}
		std::ostringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}

	void ResolveShaderPaths() {
		std::filesystem::path baseDir;
#ifdef GPURENDERER_SHADER_DIR
		baseDir = std::filesystem::path(GPURENDERER_SHADER_DIR);
#else
		baseDir = std::filesystem::current_path();
#endif
		gVertexShaderPath = (baseDir / "shader.vert").string();
		gFragmentShaderPath = (baseDir / "shader.frag").string();
	}

	template <typename T>
	bool LoadGlFunction(T& func, const char* name, const char* fallback = nullptr) {
		func = reinterpret_cast<T>(glfwGetProcAddress(name));
		if (!func && fallback) {
			func = reinterpret_cast<T>(glfwGetProcAddress(fallback));
		}
		if (!func) {
			std::fprintf(stderr, "Missing OpenGL function: %s\n", name);
			return false;
		}
		return true;
	}

	template <typename T>
	bool LoadOptionalGlFunction(T& func, const char* name, const char* fallback = nullptr) {
		func = reinterpret_cast<T>(glfwGetProcAddress(name));
		if (!func && fallback) {
			func = reinterpret_cast<T>(glfwGetProcAddress(fallback));
		}
		return func != nullptr;
	}

	bool LoadGlFunctions() {
		gUseVao = LoadOptionalGlFunction(pglGenVertexArrays, "glGenVertexArrays", "glGenVertexArraysARB") &&
			LoadOptionalGlFunction(pglBindVertexArray, "glBindVertexArray", "glBindVertexArrayARB");

		bool ok = true;
		ok &= LoadGlFunction(pglGenBuffers, "glGenBuffers", "glGenBuffersARB");
		ok &= LoadGlFunction(pglBindBuffer, "glBindBuffer", "glBindBufferARB");
		ok &= LoadGlFunction(pglBufferData, "glBufferData", "glBufferDataARB");
		ok &= LoadGlFunction(pglEnableVertexAttribArray, "glEnableVertexAttribArray");
		ok &= LoadGlFunction(pglVertexAttribPointer, "glVertexAttribPointer");
		ok &= LoadGlFunction(pglCreateShader, "glCreateShader");
		ok &= LoadGlFunction(pglShaderSource, "glShaderSource");
		ok &= LoadGlFunction(pglCompileShader, "glCompileShader");
		ok &= LoadGlFunction(pglGetShaderiv, "glGetShaderiv");
		ok &= LoadGlFunction(pglGetShaderInfoLog, "glGetShaderInfoLog");
		ok &= LoadGlFunction(pglDeleteShader, "glDeleteShader");
		ok &= LoadGlFunction(pglCreateProgram, "glCreateProgram");
		ok &= LoadGlFunction(pglAttachShader, "glAttachShader");
		ok &= LoadGlFunction(pglBindAttribLocation, "glBindAttribLocation");
		ok &= LoadGlFunction(pglLinkProgram, "glLinkProgram");
		ok &= LoadGlFunction(pglGetProgramiv, "glGetProgramiv");
		ok &= LoadGlFunction(pglGetProgramInfoLog, "glGetProgramInfoLog");
		ok &= LoadGlFunction(pglDeleteProgram, "glDeleteProgram");
		ok &= LoadGlFunction(pglUseProgram, "glUseProgram");
		ok &= LoadGlFunction(pglGetUniformLocation, "glGetUniformLocation");
		ok &= LoadGlFunction(pglUniformMatrix4fv, "glUniformMatrix4fv");
		ok &= LoadGlFunction(pglUniformMatrix3fv, "glUniformMatrix3fv");
		ok &= LoadGlFunction(pglUniform3fv, "glUniform3fv");
		ok &= LoadGlFunction(pglUniform1f, "glUniform1f");
		ok &= LoadGlFunction(pglUniform1i, "glUniform1i");
		ok &= LoadGlFunction(pglActiveTexture, "glActiveTexture", "glActiveTextureARB");
		ok &= LoadGlFunction(pglGenFramebuffers, "glGenFramebuffers", "glGenFramebuffersEXT");
		ok &= LoadGlFunction(pglBindFramebuffer, "glBindFramebuffer", "glBindFramebufferEXT");
		ok &= LoadGlFunction(pglFramebufferTexture2D, "glFramebufferTexture2D", "glFramebufferTexture2DEXT");
		ok &= LoadGlFunction(pglCheckFramebufferStatus, "glCheckFramebufferStatus", "glCheckFramebufferStatusEXT");
		ok &= LoadGlFunction(pglGenRenderbuffers, "glGenRenderbuffers", "glGenRenderbuffersEXT");
		ok &= LoadGlFunction(pglBindRenderbuffer, "glBindRenderbuffer", "glBindRenderbufferEXT");
		ok &= LoadGlFunction(pglRenderbufferStorage, "glRenderbufferStorage", "glRenderbufferStorageEXT");
		ok &= LoadGlFunction(pglFramebufferRenderbuffer, "glFramebufferRenderbuffer", "glFramebufferRenderbufferEXT");
		ok &= LoadGlFunction(pglGenerateMipmap, "glGenerateMipmap", "glGenerateMipmapEXT");
		LoadOptionalGlFunction(pglDeleteBuffers, "glDeleteBuffers");
		LoadOptionalGlFunction(pglDeleteVertexArrays, "glDeleteVertexArrays", "glDeleteVertexArraysARB");
		LoadOptionalGlFunction(pglDeleteFramebuffers, "glDeleteFramebuffers", "glDeleteFramebuffersEXT");
		LoadOptionalGlFunction(pglDeleteRenderbuffers, "glDeleteRenderbuffers", "glDeleteRenderbuffersEXT");
		return ok;
	}

	bool IsExtensionSupported(const char* extensionName) {
		if (!extensionName || *extensionName == '\0') {
			return false;
		}
		const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
		if (!extensions) {
			return false;
		}
		const std::string extList(extensions);
		const std::string target(extensionName);
		size_t pos = 0;
		while ((pos = extList.find(target, pos)) != std::string::npos) {
			const bool validPrefix = (pos == 0) || (extList[pos - 1] == ' ');
			const size_t tail = pos + target.size();
			const bool validSuffix = (tail >= extList.size()) || (extList[tail] == ' ');
			if (validPrefix && validSuffix) {
				return true;
			}
			pos = tail;
		}
		return false;
	}

	void InitializeAnisotropicFiltering() {
		gHasAnisotropicFiltering = IsExtensionSupported("GL_EXT_texture_filter_anisotropic");
		gMaxAnisotropy = 1.0f;
		if (!gHasAnisotropicFiltering) {
			return;
		}

		GLfloat maxAniso = 1.0f;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
		if (maxAniso > 1.0f) {
			gMaxAnisotropy = maxAniso;
		} else {
			gHasAnisotropicFiltering = false;
		}
	}

	void ConfigureRenderTextureSampling(GLuint texture) {
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		if (gHasAnisotropicFiltering) {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gMaxAnisotropy);
		}
	}

	void DestroyRenderTexture() {
		if (gRenderTexture.depthRenderbuffer != 0 && pglDeleteRenderbuffers) {
			pglDeleteRenderbuffers(1, &gRenderTexture.depthRenderbuffer);
		}
		if (gRenderTexture.framebuffer != 0 && pglDeleteFramebuffers) {
			pglDeleteFramebuffers(1, &gRenderTexture.framebuffer);
		}
		if (gRenderTexture.colorTexture != 0) {
			glDeleteTextures(1, &gRenderTexture.colorTexture);
		}
		gRenderTexture = GLRenderTexture{};
	}

	bool CreateOrResizeRenderTexture(int width, int height) {
		width = std::max(width, 1);
		height = std::max(height, 1);

		if (gRenderTexture.framebuffer != 0 &&
			gRenderTexture.colorTexture != 0 &&
			gRenderTexture.depthRenderbuffer != 0 &&
			gRenderTexture.width == width &&
			gRenderTexture.height == height) {
			return true;
		}

		DestroyRenderTexture();
		gRenderTexture.width = width;
		gRenderTexture.height = height;

		glGenTextures(1, &gRenderTexture.colorTexture);
		if (gRenderTexture.colorTexture == 0) {
			std::fprintf(stderr, "Failed to create render texture color target.\n");
			return false;
		}
		ConfigureRenderTextureSampling(gRenderTexture.colorTexture);
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGBA,
			gRenderTexture.width,
			gRenderTexture.height,
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			nullptr);
		if (pglGenerateMipmap) {
			pglGenerateMipmap(GL_TEXTURE_2D);
		}
		glBindTexture(GL_TEXTURE_2D, 0);

		pglGenFramebuffers(1, &gRenderTexture.framebuffer);
		pglBindFramebuffer(GL_FRAMEBUFFER, gRenderTexture.framebuffer);
		pglFramebufferTexture2D(
			GL_FRAMEBUFFER,
			GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D,
			gRenderTexture.colorTexture,
			0);

		pglGenRenderbuffers(1, &gRenderTexture.depthRenderbuffer);
		pglBindRenderbuffer(GL_RENDERBUFFER, gRenderTexture.depthRenderbuffer);
		pglRenderbufferStorage(
			GL_RENDERBUFFER,
			GL_DEPTH_COMPONENT24,
			gRenderTexture.width,
			gRenderTexture.height);
		pglFramebufferRenderbuffer(
			GL_FRAMEBUFFER,
			GL_DEPTH_ATTACHMENT,
			GL_RENDERBUFFER,
			gRenderTexture.depthRenderbuffer);

		const GLenum status = pglCheckFramebufferStatus(GL_FRAMEBUFFER);
		pglBindRenderbuffer(GL_RENDERBUFFER, 0);
		pglBindFramebuffer(GL_FRAMEBUFFER, 0);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			std::fprintf(stderr, "Render texture framebuffer incomplete (0x%X).\n", status);
			DestroyRenderTexture();
			return false;
		}

		return true;
	}

	void GenerateRenderTextureMipmaps() {
		if (gRenderTexture.colorTexture == 0 || !pglGenerateMipmap) {
			return;
		}
		glBindTexture(GL_TEXTURE_2D, gRenderTexture.colorTexture);
		pglGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void DestroyShadowMap() {
		if (gShadowMap.framebuffer != 0 && pglDeleteFramebuffers) {
			pglDeleteFramebuffers(1, &gShadowMap.framebuffer);
		}
		if (gShadowMap.depthTexture != 0) {
			glDeleteTextures(1, &gShadowMap.depthTexture);
		}
		gShadowMap = GLShadowMap{};
	}

	bool CreateOrResizeShadowMap(int width, int height) {
		width = std::max(width, 1);
		height = std::max(height, 1);
		if (gShadowMap.framebuffer != 0 &&
			gShadowMap.depthTexture != 0 &&
			gShadowMap.width == width &&
			gShadowMap.height == height) {
			return true;
		}

		DestroyShadowMap();
		gShadowMap.width = width;
		gShadowMap.height = height;

		glGenTextures(1, &gShadowMap.depthTexture);
		if (gShadowMap.depthTexture == 0) {
			std::fprintf(stderr, "Failed to create shadow map depth texture.\n");
			return false;
		}
		glBindTexture(GL_TEXTURE_2D, gShadowMap.depthTexture);
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_DEPTH_COMPONENT24,
			gShadowMap.width,
			gShadowMap.height,
			0,
			GL_DEPTH_COMPONENT,
			GL_UNSIGNED_INT,
			nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);

		pglGenFramebuffers(1, &gShadowMap.framebuffer);
		pglBindFramebuffer(GL_FRAMEBUFFER, gShadowMap.framebuffer);
		pglFramebufferTexture2D(
			GL_FRAMEBUFFER,
			GL_DEPTH_ATTACHMENT,
			GL_TEXTURE_2D,
			gShadowMap.depthTexture,
			0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

		const GLenum status = pglCheckFramebufferStatus(GL_FRAMEBUFFER);
		pglBindFramebuffer(GL_FRAMEBUFFER, 0);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			std::fprintf(stderr, "Shadow map framebuffer incomplete (0x%X).\n", status);
			DestroyShadowMap();
			return false;
		}
		return true;
	}

	glm::vec3 ToVec3(const aiColor3D& color) {
		return glm::vec3(color.r, color.g, color.b);
	}

	void ComputeNormalsForMesh(std::vector<Vertex>& vertices,
		const std::vector<unsigned int>& indices,
		size_t vertexStart,
		size_t vertexCount,
		size_t indexOffset,
		size_t indexCount) {
		if (vertexCount == 0 || indexCount < 3) {
			return;
		}

		for (size_t i = 0; i < vertexCount; ++i) {
			vertices[vertexStart + i].normal = glm::vec3(0.0f);
		}

		for (size_t i = 0; i + 2 < indexCount; i += 3) {
			const unsigned int i0 = indices[indexOffset + i + 0];
			const unsigned int i1 = indices[indexOffset + i + 1];
			const unsigned int i2 = indices[indexOffset + i + 2];
			if (i0 < vertexStart || i1 < vertexStart || i2 < vertexStart) {
				continue;
			}
			if (i0 >= vertexStart + vertexCount ||
				i1 >= vertexStart + vertexCount ||
				i2 >= vertexStart + vertexCount) {
				continue;
			}
			const glm::vec3& p0 = vertices[i0].position;
			const glm::vec3& p1 = vertices[i1].position;
			const glm::vec3& p2 = vertices[i2].position;
			const glm::vec3 n = glm::cross(p1 - p0, p2 - p0);
			vertices[i0].normal += n;
			vertices[i1].normal += n;
			vertices[i2].normal += n;
		}

		for (size_t i = 0; i < vertexCount; ++i) {
			glm::vec3& n = vertices[vertexStart + i].normal;
			const float len2 = glm::dot(n, n);
			if (len2 > 0.0f) {
				n = glm::normalize(n);
			} else {
				n = glm::vec3(0.0f, 1.0f, 0.0f);
			}
		}
	}

	GLenum ChannelsToFormat(int channels) {
		switch (channels) {
		case 1:
			return GL_LUMINANCE;
		case 2:
			return GL_LUMINANCE_ALPHA;
		case 3:
			return GL_RGB;
		case 4:
			return GL_RGBA;
		default:
			return GL_RGBA;
		}
	}

	GLuint CreateTextureFromPixels(const unsigned char* pixels, int width, int height, int channels) {
		if (!pixels || width <= 0 || height <= 0) {
			return 0;
		}
		const GLenum format = ChannelsToFormat(channels);
		GLuint tex = 0;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
		glBindTexture(GL_TEXTURE_2D, 0);
		return tex;
	}

	GLuint LoadTextureFromMemory(const aiTexture* texture) {
		if (!texture) {
			return 0;
		}
		if (texture->mHeight == 0) {
			int width = 0;
			int height = 0;
			int channels = 0;
			const stbi_uc* data = reinterpret_cast<const stbi_uc*>(texture->pcData);
			stbi_uc* pixels = stbi_load_from_memory(data, static_cast<int>(texture->mWidth), &width, &height, &channels, 0);
			if (!pixels) {
				return 0;
			}
			GLuint tex = CreateTextureFromPixels(pixels, width, height, channels);
			stbi_image_free(pixels);
			return tex;
		}

		const int width = static_cast<int>(texture->mWidth);
		const int height = static_cast<int>(texture->mHeight);
		std::vector<unsigned char> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				const aiTexel& texel = texture->pcData[y * width + x];
				const size_t idx = static_cast<size_t>((y * width + x) * 4);
				pixels[idx + 0] = texel.r;
				pixels[idx + 1] = texel.g;
				pixels[idx + 2] = texel.b;
				pixels[idx + 3] = texel.a;
			}
		}
		return CreateTextureFromPixels(pixels.data(), width, height, 4);
	}

	std::filesystem::path ResolveTexturePath(const std::filesystem::path& baseDir, const aiString& texturePath) {
		if (texturePath.length == 0) {
			return {};
		}
		std::filesystem::path rawPath(texturePath.C_Str());
		if (rawPath.is_absolute()) {
			return rawPath;
		}
		std::filesystem::path candidate = baseDir / rawPath;
		if (std::filesystem::exists(candidate)) {
			return candidate;
		}
		candidate = baseDir / rawPath.filename();
		if (std::filesystem::exists(candidate)) {
			return candidate;
		}
		return baseDir / rawPath;
	}

	GLuint LoadTextureFromFile(const std::filesystem::path& path) {
		if (path.empty()) {
			return 0;
		}
		const std::string key = path.lexically_normal().string();
		auto it = gTextureCache.find(key);
		if (it != gTextureCache.end()) {
			return it->second;
		}

		if (!std::filesystem::exists(path)) {
			std::fprintf(stderr, "Texture file not found: %s\n", key.c_str());
			return 0;
		}

		int width = 0;
		int height = 0;
		int channels = 0;
		stbi_uc* pixels = stbi_load(key.c_str(), &width, &height, &channels, 0);
		if (!pixels) {
			std::fprintf(stderr, "Failed to load texture: %s\n", key.c_str());
			return 0;
		}
		GLuint tex = CreateTextureFromPixels(pixels, width, height, channels);
		stbi_image_free(pixels);
		if (tex != 0) {
			gTextureCache.emplace(key, tex);
		}
		return tex;
	}

	std::vector<std::filesystem::path> BuildAssetRootCandidates() {
		std::vector<std::filesystem::path> candidates;
		auto addCandidate = [&](const std::filesystem::path& path) {
			if (path.empty()) {
				return;
			}
			std::error_code ec;
			std::filesystem::path normalized = std::filesystem::absolute(path, ec);
			if (ec) {
				normalized = path.lexically_normal();
			} else {
				normalized = normalized.lexically_normal();
			}
			if (std::find(candidates.begin(), candidates.end(), normalized) == candidates.end()) {
				candidates.push_back(normalized);
			}
		};

		std::error_code ec;
		const std::filesystem::path cwd = std::filesystem::current_path(ec);
		if (!gObjPath.empty()) {
			std::filesystem::path objPath(gObjPath);
			if (objPath.is_relative() && !cwd.empty()) {
				objPath = cwd / objPath;
			}
			addCandidate(objPath.parent_path());
		}

		if (!ec) {
			addCandidate(cwd);
			addCandidate(cwd / "out/build/x64-release/GPURenderer");
			addCandidate(cwd / "out/build/x64-debug/GPURenderer");
			addCandidate(cwd / "out/x64-release");
			addCandidate(cwd / "out/x64-debug");
		}

		return candidates;
	}

	std::filesystem::path ResolveCubeObjPath(const std::filesystem::path& root) {
		const std::filesystem::path nested = root / "cube" / "cube.obj";
		if (std::filesystem::exists(nested)) {
			return nested;
		}
		const std::filesystem::path flat = root / "cube.obj";
		if (std::filesystem::exists(flat)) {
			return flat;
		}
		return {};
	}

	std::filesystem::path ResolveLightObjPath(const std::filesystem::path& root) {
		const std::filesystem::path nested = root / "light" / "light.obj";
		if (std::filesystem::exists(nested)) {
			return nested;
		}
		const std::filesystem::path flat = root / "light.obj";
		if (std::filesystem::exists(flat)) {
			return flat;
		}
		return {};
	}

	std::array<std::filesystem::path, 6> BuildCubemapFacePaths(const std::filesystem::path& root) {
		std::array<std::filesystem::path, 6> faces{};
		for (size_t i = 0; i < faces.size(); ++i) {
			faces[i] = root / "cubemap" / kCubemapFaceFiles[i];
		}
		return faces;
	}

	std::filesystem::path ResolveEnvironmentAssetRoot() {
		const std::vector<std::filesystem::path> candidates = BuildAssetRootCandidates();
		for (const std::filesystem::path& root : candidates) {
			const std::filesystem::path cubePath = ResolveCubeObjPath(root);
			if (cubePath.empty()) {
				continue;
			}
			const auto cubemapFaces = BuildCubemapFacePaths(root);
			bool allFacesFound = true;
			for (const std::filesystem::path& face : cubemapFaces) {
				if (!std::filesystem::exists(face)) {
					allFacesFound = false;
					break;
				}
			}
			if (allFacesFound) {
				return root;
			}
		}
		return {};
	}

	bool LoadCubemapTexture(const std::filesystem::path& root) {
		const auto facePaths = BuildCubemapFacePaths(root);
		for (const std::filesystem::path& face : facePaths) {
			if (!std::filesystem::exists(face)) {
				std::fprintf(stderr, "Cubemap face missing: %s\n", face.string().c_str());
				return false;
			}
		}

		GLuint cubemap = 0;
		glGenTextures(1, &cubemap);
		if (cubemap == 0) {
			return false;
		}

		glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
		stbi_set_flip_vertically_on_load(false);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		bool success = true;
		for (size_t i = 0; i < facePaths.size(); ++i) {
			int width = 0;
			int height = 0;
			int channels = 0;
			const std::string facePath = facePaths[i].string();
			stbi_uc* pixels = stbi_load(facePath.c_str(), &width, &height, &channels, 0);
			if (!pixels) {
				std::fprintf(stderr, "Failed to load cubemap face: %s\n", facePath.c_str());
				success = false;
				break;
			}

			const GLenum format = ChannelsToFormat(channels);
			glTexImage2D(
				kCubemapFaceTargets[i],
				0,
				format,
				width,
				height,
				0,
				format,
				GL_UNSIGNED_BYTE,
				pixels);
			stbi_image_free(pixels);
		}
		stbi_set_flip_vertically_on_load(true);

		if (!success) {
			glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
			glDeleteTextures(1, &cubemap);
			return false;
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		if (gHasAnisotropicFiltering) {
			glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_ANISOTROPY_EXT, gMaxAnisotropy);
		}
		if (pglGenerateMipmap) {
			pglGenerateMipmap(GL_TEXTURE_CUBE_MAP);
		}
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

		if (gEnvironmentCubemap != 0) {
			glDeleteTextures(1, &gEnvironmentCubemap);
		}
		gEnvironmentCubemap = cubemap;
		return true;
	}

	bool LoadMesh(const std::string& path,
		std::vector<Vertex>& vertices,
		std::vector<unsigned int>& indices,
		Bounds& bounds,
		std::vector<Submesh>& submeshes,
		std::vector<Material>& materials) {
		Assimp::Importer importer;
#ifdef AI_CONFIG_IMPORT_OBJ_FAST_VERTICES
		importer.SetPropertyInteger(AI_CONFIG_IMPORT_OBJ_FAST_VERTICES, 1);
#endif
#ifdef AI_CONFIG_IMPORT_OBJ_FAST_MATERIAL
		importer.SetPropertyInteger(AI_CONFIG_IMPORT_OBJ_FAST_MATERIAL, 1);
#endif
		const unsigned int flags =
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices;
		const auto importStart = std::chrono::steady_clock::now();
		std::printf("Assimp import starting...\n");
		std::fflush(stdout);
		const aiScene* scene = importer.ReadFile(
			path,
			flags);
		const auto importEnd = std::chrono::steady_clock::now();
		const double importSeconds = std::chrono::duration<double>(importEnd - importStart).count();
		std::printf("Assimp import finished in %.2f seconds.\n", importSeconds);
		std::fflush(stdout);

		if (!scene || !scene->HasMeshes()) {
			std::fprintf(stderr, "Assimp failed to load mesh: %s\n", importer.GetErrorString());
			return false;
		}

		vertices.clear();
		indices.clear();
		submeshes.clear();
		materials.clear();
		bounds = Bounds{};

		std::filesystem::path objPath(path);
		if (objPath.is_relative()) {
			objPath = std::filesystem::absolute(objPath);
		}
		const std::filesystem::path baseDir = objPath.parent_path();

		if (scene->mNumMaterials == 0) {
			materials.emplace_back();
			materials.front().name = "Default";
		} else {
			materials.resize(scene->mNumMaterials);
			for (unsigned int matIndex = 0; matIndex < scene->mNumMaterials; ++matIndex) {
				Material mat;
				mat.name = "Material " + std::to_string(matIndex);
				const aiMaterial* material = scene->mMaterials[matIndex];
				if (!material) {
					materials[matIndex] = mat;
					continue;
				}

				auto loadTexture = [&](aiTextureType type, const char* label, GLuint& outTex, bool& hasTex) -> bool {
					aiString texPath;
					if (material->GetTextureCount(type) == 0 ||
						material->GetTexture(type, 0, &texPath) != AI_SUCCESS) {
						return false;
					}

					if (texPath.length > 0 && texPath.C_Str()[0] == '*') {
						const int texIndex = std::atoi(texPath.C_Str() + 1);
						if (texIndex >= 0 && static_cast<unsigned int>(texIndex) < scene->mNumTextures) {
							const std::string key = "*" + std::to_string(texIndex);
							auto it = gTextureCache.find(key);
							if (it != gTextureCache.end()) {
								outTex = it->second;
							} else {
								outTex = LoadTextureFromMemory(scene->mTextures[texIndex]);
								if (outTex != 0) {
									gTextureCache.emplace(key, outTex);
								}
							}
						}
					} else {
						const std::filesystem::path resolved = ResolveTexturePath(baseDir, texPath);
						outTex = LoadTextureFromFile(resolved);
					}

					hasTex = outTex != 0;
					if (!hasTex) {
						std::fprintf(stderr, "Material %u %s texture failed: %s\n",
							matIndex,
							label,
							texPath.C_Str());
					}
					return hasTex;
				};

				aiString materialName;
				if (material->Get(AI_MATKEY_NAME, materialName) == AI_SUCCESS && materialName.length > 0) {
					mat.name = materialName.C_Str();
				}
				aiColor3D color(0.0f, 0.0f, 0.0f);
				if (material->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) {
					mat.ambient = ToVec3(color);
				}
				if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
					mat.diffuse = ToVec3(color);
				}
				if (material->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) {
					mat.specular = ToVec3(color);
				}
				float shininess = 0.0f;
				if (material->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS && shininess > 0.0f) {
					mat.shininess = shininess;
				}

				if (!loadTexture(aiTextureType_DIFFUSE, "diffuse", mat.diffuseTexture, mat.hasDiffuseTexture)) {
					if (!loadTexture(aiTextureType_BASE_COLOR, "base_color", mat.diffuseTexture, mat.hasDiffuseTexture)) {
						loadTexture(aiTextureType_AMBIENT, "ambient", mat.diffuseTexture, mat.hasDiffuseTexture);
					}
				}

				if (!loadTexture(aiTextureType_SPECULAR, "specular", mat.specularTexture, mat.hasSpecularTexture)) {
					if (!loadTexture(aiTextureType_SHININESS, "shininess", mat.specularTexture, mat.hasSpecularTexture)) {
						if (!loadTexture(aiTextureType_METALNESS, "metalness", mat.specularTexture, mat.hasSpecularTexture)) {
							loadTexture(aiTextureType_REFLECTION, "reflection", mat.specularTexture, mat.hasSpecularTexture);
						}
					}
				}
				if (mat.hasSpecularTexture) {
					const float maxSpec = std::max(mat.specular.x, std::max(mat.specular.y, mat.specular.z));
					if (maxSpec <= 0.001f) {
						mat.specular = glm::vec3(1.0f);
					}
				}

				materials[matIndex] = mat;
			}
		}

		const auto buildStart = std::chrono::steady_clock::now();
		for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
			const aiMesh* mesh = scene->mMeshes[meshIndex];
			if (!mesh || mesh->mNumVertices == 0) {
				continue;
			}
			if (!mesh->HasTextureCoords(0)) {
				std::fprintf(stderr, "Mesh %u has no texture coordinates.\n", meshIndex);
			}
			const bool hasNormals = mesh->HasNormals();

			const unsigned int baseIndex = static_cast<unsigned int>(vertices.size());
			vertices.reserve(vertices.size() + mesh->mNumVertices);

			for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
				const aiVector3D pos = mesh->mVertices[i];
				aiVector3D normal(0.0f, 0.0f, 0.0f);
				if (hasNormals) {
					normal = mesh->mNormals[i];
				}
				aiVector3D uv(0.0f, 0.0f, 0.0f);
				if (mesh->HasTextureCoords(0)) {
					uv = mesh->mTextureCoords[0][i];
				}
				Vertex vertex;
				vertex.position = glm::vec3(pos.x, pos.y, pos.z);
				vertex.normal = glm::vec3(normal.x, normal.y, normal.z);
				vertex.texcoord = glm::vec2(uv.x, uv.y);
				vertices.push_back(vertex);
				bounds.Expand(vertex.position);
			}

			const unsigned int indexOffset = static_cast<unsigned int>(indices.size());
			for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
				const aiFace& face = mesh->mFaces[i];
				if (face.mNumIndices != 3) {
					continue;
				}
				indices.push_back(baseIndex + face.mIndices[0]);
				indices.push_back(baseIndex + face.mIndices[1]);
				indices.push_back(baseIndex + face.mIndices[2]);
			}

			const unsigned int indexCount = static_cast<unsigned int>(indices.size()) - indexOffset;
			if (!hasNormals && indexCount > 0) {
				ComputeNormalsForMesh(
					vertices,
					indices,
					baseIndex,
					mesh->mNumVertices,
					indexOffset,
					indexCount);
			}
			if (indexCount > 0) {
				Submesh submesh;
				submesh.indexOffset = static_cast<int>(indexOffset);
				submesh.indexCount = static_cast<int>(indexCount);
				if (mesh->mMaterialIndex < materials.size()) {
					submesh.materialIndex = static_cast<int>(mesh->mMaterialIndex);
				} else {
					submesh.materialIndex = 0;
				}
				submeshes.push_back(submesh);
			}
		}

		if (!bounds.valid || vertices.empty() || indices.empty()) {
			std::fprintf(stderr, "Mesh contained no valid triangles: %s\n", path.c_str());
			return false;
		}
		const auto buildEnd = std::chrono::steady_clock::now();
		const double buildSeconds = std::chrono::duration<double>(buildEnd - buildStart).count();
		std::printf("Mesh build finished in %.2f seconds.\n", buildSeconds);
		std::fflush(stdout);

		return true;
	}

	void DestroyBackgroundBuffers() {
		if (pglDeleteBuffers) {
			if (gBackgroundEbo != 0) {
				pglDeleteBuffers(1, &gBackgroundEbo);
				gBackgroundEbo = 0;
			}
			if (gBackgroundVbo != 0) {
				pglDeleteBuffers(1, &gBackgroundVbo);
				gBackgroundVbo = 0;
			}
		}
		if (gUseVao && pglDeleteVertexArrays && gBackgroundVao != 0) {
			pglDeleteVertexArrays(1, &gBackgroundVao);
			gBackgroundVao = 0;
		}
		gBackgroundIndexCount = 0;
	}

	void CreateBackgroundBuffers(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
		DestroyBackgroundBuffers();

		if (gUseVao) {
			pglGenVertexArrays(1, &gBackgroundVao);
			pglBindVertexArray(gBackgroundVao);
		}

		pglGenBuffers(1, &gBackgroundVbo);
		pglBindBuffer(GL_ARRAY_BUFFER, gBackgroundVbo);
		pglBufferData(
			GL_ARRAY_BUFFER,
			static_cast<std::ptrdiff_t>(vertices.size() * sizeof(Vertex)),
			vertices.data(),
			GL_STATIC_DRAW);

		pglGenBuffers(1, &gBackgroundEbo);
		pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gBackgroundEbo);
		pglBufferData(
			GL_ELEMENT_ARRAY_BUFFER,
			static_cast<std::ptrdiff_t>(indices.size() * sizeof(unsigned int)),
			indices.data(),
			GL_STATIC_DRAW);

		pglEnableVertexAttribArray(0);
		pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
		pglEnableVertexAttribArray(1);
		pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
		pglEnableVertexAttribArray(2);
		pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texcoord)));

		if (gUseVao) {
			pglBindVertexArray(0);
		}
		gBackgroundIndexCount = static_cast<int>(indices.size());
	}

	bool InitializeEnvironmentAssets() {
		gAssetRoot = ResolveEnvironmentAssetRoot();
		if (gAssetRoot.empty()) {
			gEnvironmentLoadStatus = "Environment assets not found (expected cube/ and cubemap/ folders).";
			return false;
		}

		const std::filesystem::path cubePath = ResolveCubeObjPath(gAssetRoot);
		if (cubePath.empty()) {
			gEnvironmentLoadStatus = "Environment asset root found, but cube.obj is missing.";
			return false;
		}

		std::vector<Vertex> backgroundVertices;
		std::vector<unsigned int> backgroundIndices;
		std::vector<Submesh> backgroundSubmeshes;
		std::vector<Material> backgroundMaterials;
		Bounds backgroundBounds;
		if (!LoadMesh(
			cubePath.string(),
			backgroundVertices,
			backgroundIndices,
			backgroundBounds,
			backgroundSubmeshes,
			backgroundMaterials)) {
			gEnvironmentLoadStatus = "Failed to load cube background mesh: " + cubePath.string();
			return false;
		}
		CreateBackgroundBuffers(backgroundVertices, backgroundIndices);

		if (!LoadCubemapTexture(gAssetRoot)) {
			gEnvironmentLoadStatus = "Failed to load cubemap texture set from: " + (gAssetRoot / "cubemap").string();
			return false;
		}

		gEnvironmentLoadStatus = "Loaded environment assets from: " + gAssetRoot.string();
		return true;
	}

	GLuint CompileShader(GLenum type, const std::string& source, const char* label) {
		const char* src = source.c_str();
		GLuint shader = pglCreateShader(type);
		pglShaderSource(shader, 1, &src, nullptr);
		pglCompileShader(shader);

		GLint status = 0;
		pglGetShaderiv(shader, GL_COMPILE_STATUS, &status);
		if (status == GL_TRUE) {
			return shader;
		}

		GLint logLength = 0;
		pglGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
		std::string log(static_cast<size_t>(logLength), '\0');
		if (logLength > 0) {
			pglGetShaderInfoLog(shader, logLength, nullptr, log.data());
		}
		std::fprintf(stderr, "Shader compile failed (%s):\n%s\n", label, log.c_str());
		pglDeleteShader(shader);
		return 0;
	}

	GLuint LinkProgram(GLuint vertexShader, GLuint fragmentShader) {
		GLuint program = pglCreateProgram();
		pglAttachShader(program, vertexShader);
		pglAttachShader(program, fragmentShader);
		pglBindAttribLocation(program, 0, "aPosition");
		pglBindAttribLocation(program, 1, "aNormal");
		pglBindAttribLocation(program, 2, "aTexCoord");
		pglLinkProgram(program);

		GLint status = 0;
		pglGetProgramiv(program, GL_LINK_STATUS, &status);
		if (status == GL_TRUE) {
			return program;
		}

		GLint logLength = 0;
		pglGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
		std::string log(static_cast<size_t>(logLength), '\0');
		if (logLength > 0) {
			pglGetProgramInfoLog(program, logLength, nullptr, log.data());
		}
		std::fprintf(stderr, "Program link failed:\n%s\n", log.c_str());
		pglDeleteProgram(program);
		return 0;
	}

	bool ReloadShaders() {
		std::string vertexSource = ReadFileText(gVertexShaderPath);
		std::string fragmentSource = ReadFileText(gFragmentShaderPath);

		if (vertexSource.empty()) {
			std::fprintf(stderr, "Vertex shader missing, using fallback: %s\n", gVertexShaderPath.c_str());
			vertexSource = kFallbackVertexShader;
		}
		if (fragmentSource.empty()) {
			std::fprintf(stderr, "Fragment shader missing, using fallback: %s\n", gFragmentShaderPath.c_str());
			fragmentSource = kFallbackFragmentShader;
		}

		GLuint newVertex = CompileShader(GL_VERTEX_SHADER, vertexSource, "vertex");
		if (!newVertex) {
			return false;
		}
		GLuint newFragment = CompileShader(GL_FRAGMENT_SHADER, fragmentSource, "fragment");
		if (!newFragment) {
			pglDeleteShader(newVertex);
			return false;
		}

		GLuint newProgram = LinkProgram(newVertex, newFragment);
		pglDeleteShader(newVertex);
		pglDeleteShader(newFragment);
		if (!newProgram) {
			return false;
		}

		if (gProgram != 0) {
			pglDeleteProgram(gProgram);
		}
		gProgram = newProgram;
		gMvpLocation = pglGetUniformLocation(gProgram, "uMvp");
		gModelLocation = pglGetUniformLocation(gProgram, "uModel");
		gViewLocation = pglGetUniformLocation(gProgram, "uView");
		gNormalMatrixLocation = pglGetUniformLocation(gProgram, "uNormalMatrix");
		gWorldNormalMatrixLocation = pglGetUniformLocation(gProgram, "uWorldNormalMatrix");
		gLightPosLocation = pglGetUniformLocation(gProgram, "uLightPosView");
		gLightColorLocation = pglGetUniformLocation(gProgram, "uLightColor");
		gAmbientLocation = pglGetUniformLocation(gProgram, "uAmbientColor");
		gDiffuseLocation = pglGetUniformLocation(gProgram, "uDiffuseColor");
		gSpecularLocation = pglGetUniformLocation(gProgram, "uSpecularColor");
		gMarkerColorLocation = pglGetUniformLocation(gProgram, "uMarkerColor");
		gShininessLocation = pglGetUniformLocation(gProgram, "uShininess");
		gShadeModeLocation = pglGetUniformLocation(gProgram, "uShadeMode");
		gDiffuseMapLocation = pglGetUniformLocation(gProgram, "uDiffuseMap");
		gSpecularMapLocation = pglGetUniformLocation(gProgram, "uSpecularMap");
		gUseDiffuseMapLocation = pglGetUniformLocation(gProgram, "uUseDiffuseMap");
		gUseSpecularMapLocation = pglGetUniformLocation(gProgram, "uUseSpecularMap");
		gPlaneColorBiasLocation = pglGetUniformLocation(gProgram, "uPlaneColorBias");
		gEnvMapLocation = pglGetUniformLocation(gProgram, "uEnvMap");
		gUseEnvMapLocation = pglGetUniformLocation(gProgram, "uUseEnvMap");
		gInvViewRotationLocation = pglGetUniformLocation(gProgram, "uInvViewRotation");
		gEnvReflectionStrengthLocation = pglGetUniformLocation(gProgram, "uEnvReflectionStrength");
		gPlaneReflectionStrengthLocation = pglGetUniformLocation(gProgram, "uPlaneReflectionStrength");
		gPlaneEnvStrengthLocation = pglGetUniformLocation(gProgram, "uPlaneEnvStrength");
		gPlaneReflectionBrightnessLocation = pglGetUniformLocation(gProgram, "uPlaneReflectionBrightness");
		gReflectionViewProjLocation = pglGetUniformLocation(gProgram, "uReflectionViewProj");
		gLightDirLocation = pglGetUniformLocation(gProgram, "uLightDirView");
		gLightViewProjLocation = pglGetUniformLocation(gProgram, "uLightViewProj");
		gShadowMapLocation = pglGetUniformLocation(gProgram, "uShadowMap");
		gReceiveShadowsLocation = pglGetUniformLocation(gProgram, "uReceiveShadows");
		gShadowBiasLocation = pglGetUniformLocation(gProgram, "uShadowBias");
		gUseSpotLightLocation = pglGetUniformLocation(gProgram, "uUseSpotLight");
		gSpotCosInnerLocation = pglGetUniformLocation(gProgram, "uSpotCosInner");
		gSpotCosOuterLocation = pglGetUniformLocation(gProgram, "uSpotCosOuter");

		pglUseProgram(gProgram);
		if (gDiffuseMapLocation >= 0) {
			pglUniform1i(gDiffuseMapLocation, 0);
		}
		if (gSpecularMapLocation >= 0) {
			pglUniform1i(gSpecularMapLocation, 1);
		}
		if (gEnvMapLocation >= 0) {
			pglUniform1i(gEnvMapLocation, 2);
		}
		if (gShadowMapLocation >= 0) {
			pglUniform1i(gShadowMapLocation, 3);
		}
		pglUseProgram(0);
		return true;
	}

	bool ReloadDepthShader() {
		GLuint depthVertex = CompileShader(GL_VERTEX_SHADER, kDepthVertexShader, "depth vertex");
		if (!depthVertex) {
			return false;
		}
		GLuint depthFragment = CompileShader(GL_FRAGMENT_SHADER, kDepthFragmentShader, "depth fragment");
		if (!depthFragment) {
			pglDeleteShader(depthVertex);
			return false;
		}

		GLuint newDepthProgram = LinkProgram(depthVertex, depthFragment);
		pglDeleteShader(depthVertex);
		pglDeleteShader(depthFragment);
		if (!newDepthProgram) {
			return false;
		}

		if (gDepthProgram != 0) {
			pglDeleteProgram(gDepthProgram);
		}
		gDepthProgram = newDepthProgram;
		gDepthMvpLocation = pglGetUniformLocation(gDepthProgram, "uDepthMvp");
		return true;
	}

	void CreateBuffers() {
		if (gUseVao) {
			pglGenVertexArrays(1, &gVao);
			pglBindVertexArray(gVao);
		}

		pglGenBuffers(1, &gVbo);
		pglBindBuffer(GL_ARRAY_BUFFER, gVbo);
		pglBufferData(
			GL_ARRAY_BUFFER,
			static_cast<std::ptrdiff_t>(gVertices.size() * sizeof(Vertex)),
			gVertices.data(),
			GL_STATIC_DRAW);

		pglGenBuffers(1, &gEbo);
		pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gEbo);
		pglBufferData(
			GL_ELEMENT_ARRAY_BUFFER,
			static_cast<std::ptrdiff_t>(gIndices.size() * sizeof(unsigned int)),
			gIndices.data(),
			GL_STATIC_DRAW);

		pglEnableVertexAttribArray(0);
		pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
		pglEnableVertexAttribArray(1);
		pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
		pglEnableVertexAttribArray(2);
		pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texcoord)));

		if (gUseVao) {
			pglBindVertexArray(0);
		}
	}

	void CreatePlaneBuffers() {
		const std::array<Vertex, 4> planeVertices{
			Vertex{glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f)},
			Vertex{glm::vec3( 1.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)},
			Vertex{glm::vec3( 1.0f, 0.0f,  1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f)},
			Vertex{glm::vec3(-1.0f, 0.0f,  1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f)}
		};
		const std::array<unsigned int, 6> planeIndices{0, 1, 2, 0, 2, 3};
		gPlaneIndexCount = static_cast<int>(planeIndices.size());

		if (gUseVao) {
			pglGenVertexArrays(1, &gPlaneVao);
			pglBindVertexArray(gPlaneVao);
		}

		pglGenBuffers(1, &gPlaneVbo);
		pglBindBuffer(GL_ARRAY_BUFFER, gPlaneVbo);
		pglBufferData(
			GL_ARRAY_BUFFER,
			static_cast<std::ptrdiff_t>(planeVertices.size() * sizeof(Vertex)),
			planeVertices.data(),
			GL_STATIC_DRAW);

		pglGenBuffers(1, &gPlaneEbo);
		pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gPlaneEbo);
		pglBufferData(
			GL_ELEMENT_ARRAY_BUFFER,
			static_cast<std::ptrdiff_t>(planeIndices.size() * sizeof(unsigned int)),
			planeIndices.data(),
			GL_STATIC_DRAW);

		pglEnableVertexAttribArray(0);
		pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
		pglEnableVertexAttribArray(1);
		pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
		pglEnableVertexAttribArray(2);
		pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texcoord)));

		if (gUseVao) {
			pglBindVertexArray(0);
		}
	}

	void DestroyMeshBuffers(GLMeshBuffers& mesh) {
		if (pglDeleteBuffers) {
			if (mesh.ebo != 0) {
				pglDeleteBuffers(1, &mesh.ebo);
				mesh.ebo = 0;
			}
			if (mesh.vbo != 0) {
				pglDeleteBuffers(1, &mesh.vbo);
				mesh.vbo = 0;
			}
		}
		if (gUseVao && pglDeleteVertexArrays && mesh.vao != 0) {
			pglDeleteVertexArrays(1, &mesh.vao);
			mesh.vao = 0;
		}
		mesh.indexCount = 0;
	}

	void CreateMeshBuffers(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, GLMeshBuffers& mesh) {
		DestroyMeshBuffers(mesh);
		if (vertices.empty() || indices.empty()) {
			return;
		}

		if (gUseVao) {
			pglGenVertexArrays(1, &mesh.vao);
			pglBindVertexArray(mesh.vao);
		}

		pglGenBuffers(1, &mesh.vbo);
		pglBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
		pglBufferData(
			GL_ARRAY_BUFFER,
			static_cast<std::ptrdiff_t>(vertices.size() * sizeof(Vertex)),
			vertices.data(),
			GL_STATIC_DRAW);

		pglGenBuffers(1, &mesh.ebo);
		pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
		pglBufferData(
			GL_ELEMENT_ARRAY_BUFFER,
			static_cast<std::ptrdiff_t>(indices.size() * sizeof(unsigned int)),
			indices.data(),
			GL_STATIC_DRAW);

		pglEnableVertexAttribArray(0);
		pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
		pglEnableVertexAttribArray(1);
		pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
		pglEnableVertexAttribArray(2);
		pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texcoord)));

		if (gUseVao) {
			pglBindVertexArray(0);
		}
		mesh.indexCount = static_cast<int>(indices.size());
	}

	void BindMeshBuffers(const GLMeshBuffers& mesh) {
		if (gUseVao && mesh.vao != 0) {
			pglBindVertexArray(mesh.vao);
			return;
		}
		pglBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
		pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
		pglEnableVertexAttribArray(0);
		pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
		pglEnableVertexAttribArray(1);
		pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
		pglEnableVertexAttribArray(2);
		pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texcoord)));
	}

	void BuildLightCube(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices) {
		vertices = {
			{{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
			{{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
			{{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
			{{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
			{{-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
			{{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
			{{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
			{{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}
		};
		indices = {
			0, 1, 2, 0, 2, 3,
			4, 6, 5, 4, 7, 6,
			4, 5, 1, 4, 1, 0,
			3, 2, 6, 3, 6, 7,
			1, 5, 6, 1, 6, 2,
			4, 0, 3, 4, 3, 7
		};
	}

	void BuildLightSphere(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices) {
		const int latSegments = 14;
		const int lonSegments = 20;
		vertices.clear();
		indices.clear();
		vertices.reserve(static_cast<size_t>((latSegments + 1) * (lonSegments + 1)));
		for (int lat = 0; lat <= latSegments; ++lat) {
			const float v = static_cast<float>(lat) / static_cast<float>(latSegments);
			const float theta = v * 3.14159265358979323846f;
			const float y = std::cos(theta);
			const float r = std::sin(theta);
			for (int lon = 0; lon <= lonSegments; ++lon) {
				const float u = static_cast<float>(lon) / static_cast<float>(lonSegments);
				const float phi = u * (2.0f * 3.14159265358979323846f);
				const float x = r * std::cos(phi);
				const float z = r * std::sin(phi);
				const glm::vec3 normal = glm::normalize(glm::vec3(x, y, z));
				vertices.push_back(Vertex{normal * 0.5f, normal, glm::vec2(u, v)});
			}
		}

		for (int lat = 0; lat < latSegments; ++lat) {
			for (int lon = 0; lon < lonSegments; ++lon) {
				const int row0 = lat * (lonSegments + 1);
				const int row1 = (lat + 1) * (lonSegments + 1);
				const unsigned int a = static_cast<unsigned int>(row0 + lon);
				const unsigned int b = static_cast<unsigned int>(row1 + lon);
				const unsigned int c = static_cast<unsigned int>(row1 + lon + 1);
				const unsigned int d = static_cast<unsigned int>(row0 + lon + 1);
				indices.push_back(a);
				indices.push_back(b);
				indices.push_back(c);
				indices.push_back(a);
				indices.push_back(c);
				indices.push_back(d);
			}
		}
	}

	bool LoadLightObjectMesh() {
		DestroyMeshBuffers(gLightObjectMesh);
		gHasLightObjectMesh = false;

		std::filesystem::path lightPath;
		if (!gAssetRoot.empty()) {
			lightPath = ResolveLightObjPath(gAssetRoot);
		}
		if (lightPath.empty()) {
			for (const std::filesystem::path& root : BuildAssetRootCandidates()) {
				lightPath = ResolveLightObjPath(root);
				if (!lightPath.empty()) {
					break;
				}
			}
		}
		if (lightPath.empty()) {
			gLightObjectStatus = "Light object not found (expected light/light.obj).";
			return false;
		}

		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;
		std::vector<Submesh> submeshes;
		std::vector<Material> materials;
		Bounds bounds;
		if (!LoadMesh(lightPath.string(), vertices, indices, bounds, submeshes, materials)) {
			gLightObjectStatus = "Failed to load light object: " + lightPath.string();
			return false;
		}

		CreateMeshBuffers(vertices, indices, gLightObjectMesh);
		if (gLightObjectMesh.indexCount <= 0) {
			gLightObjectStatus = "Light object has no drawable triangles: " + lightPath.string();
			return false;
		}

		gHasLightObjectMesh = true;
		gLightObjectStatus = "Loaded light object: " + lightPath.string();
		return true;
	}

	void DestroyLightBuffers() {
		if (pglDeleteBuffers && gLightVbo != 0) {
			pglDeleteBuffers(1, &gLightVbo);
			gLightVbo = 0;
		}
		if (gUseVao && pglDeleteVertexArrays && gLightVao != 0) {
			pglDeleteVertexArrays(1, &gLightVao);
			gLightVao = 0;
		}
		DestroyMeshBuffers(gLightCubeMesh);
		DestroyMeshBuffers(gLightSphereMesh);
		DestroyMeshBuffers(gLightObjectMesh);
		gHasLightObjectMesh = false;
	}

	void CreateLightBuffers() {
		DestroyLightBuffers();

		const Vertex lightVertex{
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 1.0f, 0.0f),
			glm::vec2(0.0f, 0.0f)
		};

		if (gUseVao) {
			pglGenVertexArrays(1, &gLightVao);
			pglBindVertexArray(gLightVao);
		}

		pglGenBuffers(1, &gLightVbo);
		pglBindBuffer(GL_ARRAY_BUFFER, gLightVbo);
		pglBufferData(GL_ARRAY_BUFFER, static_cast<std::ptrdiff_t>(sizeof(Vertex)), &lightVertex, GL_STATIC_DRAW);

		pglEnableVertexAttribArray(0);
		pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
		pglEnableVertexAttribArray(1);
		pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
		pglEnableVertexAttribArray(2);
		pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texcoord)));

		if (gUseVao) {
			pglBindVertexArray(0);
		}

		std::vector<Vertex> primitiveVertices;
		std::vector<unsigned int> primitiveIndices;

		BuildLightCube(primitiveVertices, primitiveIndices);
		CreateMeshBuffers(primitiveVertices, primitiveIndices, gLightCubeMesh);

		BuildLightSphere(primitiveVertices, primitiveIndices);
		CreateMeshBuffers(primitiveVertices, primitiveIndices, gLightSphereMesh);

		if (!LoadLightObjectMesh()) {
			std::fprintf(stderr, "%s\n", gLightObjectStatus.c_str());
		}
	}

	glm::mat4 BuildObjectModelMatrix(const OrbitCamera& camera) {
		glm::mat4 model(1.0f);
		model = glm::translate(model, -gCenter);
		if (!gUsePerspective) {
			const float scale = 1.0f / std::max(camera.distance, kMinCameraDistance);
			model = glm::scale(glm::mat4(1.0f), glm::vec3(scale)) * model;
		}
		return model;
	}

	glm::mat4 BuildViewMatrix(const OrbitCamera& camera) {
		glm::mat4 view(1.0f);
		view = glm::translate(view, glm::vec3(camera.panX, camera.panY, 0.0f));
		view = glm::translate(view, glm::vec3(0.0f, 0.0f, -camera.distance));
		view = glm::rotate(view, glm::radians(camera.pitchDeg), glm::vec3(1.0f, 0.0f, 0.0f));
		view = glm::rotate(view, glm::radians(camera.yawDeg), glm::vec3(0.0f, 1.0f, 0.0f));
		return view;
	}

	glm::mat4 BuildProjectionMatrix(int width, int height, float nearPlane, float farPlane) {
		const float aspect = (height > 0)
			? static_cast<float>(width) / static_cast<float>(height)
			: 1.0f;
		if (gUsePerspective) {
			return glm::perspective(glm::radians(kDefaultFovDeg), aspect, nearPlane, farPlane);
		}

		const float size = 1.0f;
		return glm::ortho(-size * aspect, size * aspect, -size, size, -farPlane, farPlane);
	}

	glm::vec3 ComputeLightPosition(const glm::mat4& model) {
		const float yaw = glm::radians(gLightYawDeg);
		const float pitch = glm::radians(gLightPitchDeg);
		glm::vec3 direction(
			std::sin(yaw) * std::cos(pitch),
			std::sin(pitch),
			std::cos(yaw) * std::cos(pitch));
		const glm::vec3 lightModel = gCenter + direction * gLightDistance;
		return glm::vec3(model * glm::vec4(lightModel, 1.0f));
	}

	void BindModelBuffers() {
		if (gUseVao && gVao != 0) {
			pglBindVertexArray(gVao);
			return;
		}
		pglBindBuffer(GL_ARRAY_BUFFER, gVbo);
		pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gEbo);
		pglEnableVertexAttribArray(0);
		pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
		pglEnableVertexAttribArray(1);
		pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
		pglEnableVertexAttribArray(2);
		pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texcoord)));
	}

	void BindPlaneBuffers() {
		if (gUseVao && gPlaneVao != 0) {
			pglBindVertexArray(gPlaneVao);
			return;
		}
		pglBindBuffer(GL_ARRAY_BUFFER, gPlaneVbo);
		pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gPlaneEbo);
		pglEnableVertexAttribArray(0);
		pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
		pglEnableVertexAttribArray(1);
		pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
		pglEnableVertexAttribArray(2);
		pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texcoord)));
	}

	void BindBackgroundBuffers() {
		if (gUseVao && gBackgroundVao != 0) {
			pglBindVertexArray(gBackgroundVao);
			return;
		}
		pglBindBuffer(GL_ARRAY_BUFFER, gBackgroundVbo);
		pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gBackgroundEbo);
		pglEnableVertexAttribArray(0);
		pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
		pglEnableVertexAttribArray(1);
		pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
		pglEnableVertexAttribArray(2);
		pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texcoord)));
	}

	glm::mat4 BuildPlaneModelMatrix() {
		glm::mat4 model(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, gPlaneHeight, 0.0f));
		const float halfWidth = 0.5f * gPlaneWidth * gPlaneScale;
		const float halfLength = 0.5f * gPlaneLength * gPlaneScale;
		model = glm::scale(model, glm::vec3(halfWidth, 1.0f, halfLength));
		return model;
	}

	glm::mat4 BuildPlanarReflectionMatrix(float planeHeight) {
		glm::mat4 reflection(1.0f);
		reflection = glm::translate(reflection, glm::vec3(0.0f, 2.0f * planeHeight, 0.0f));
		reflection = glm::scale(reflection, glm::vec3(1.0f, -1.0f, 1.0f));
		return reflection;
	}

	glm::mat4 BuildOrientationFromForward(const glm::vec3& forwardDirection) {
		glm::vec3 forward = forwardDirection;
		const float forwardLenSq = glm::dot(forward, forward);
		if (forwardLenSq < 1e-8f) {
			forward = glm::vec3(0.0f, -1.0f, 0.0f);
		} else {
			forward = glm::normalize(forward);
		}

		glm::vec3 upReference(0.0f, 1.0f, 0.0f);
		if (std::abs(glm::dot(forward, upReference)) > 0.98f) {
			upReference = glm::vec3(1.0f, 0.0f, 0.0f);
		}

		// The light object in light.obj points along local -Z, so local +Z maps to -forward.
		const glm::vec3 zAxis = -forward;
		const glm::vec3 xAxis = glm::normalize(glm::cross(upReference, zAxis));
		const glm::vec3 yAxis = glm::normalize(glm::cross(zAxis, xAxis));

		glm::mat4 orientation(1.0f);
		orientation[0] = glm::vec4(xAxis, 0.0f);
		orientation[1] = glm::vec4(yAxis, 0.0f);
		orientation[2] = glm::vec4(zAxis, 0.0f);
		return orientation;
	}

	void DrawModelGeometry() {
		if (gSubmeshes.empty()) {
			glDrawElements(GL_TRIANGLES, gIndexCount, GL_UNSIGNED_INT, nullptr);
			return;
		}
		for (const Submesh& submesh : gSubmeshes) {
			const void* offset = reinterpret_cast<const void*>(static_cast<size_t>(submesh.indexOffset) * sizeof(unsigned int));
			glDrawElements(GL_TRIANGLES, submesh.indexCount, GL_UNSIGNED_INT, offset);
		}
	}

	void UpdateLightShadowState(const glm::mat4& model) {
		gLightWorldPosition = ComputeLightPosition(model);
		glm::vec3 lightTarget(0.0f, std::min(0.0f, gPlaneHeight * 0.5f), 0.0f);
		glm::vec3 lightDirection = lightTarget - gLightWorldPosition;
		const float directionLengthSq = glm::dot(lightDirection, lightDirection);
		if (directionLengthSq < 1e-6f) {
			lightDirection = glm::vec3(0.0f, -1.0f, 0.0f);
		}
		gLightWorldDirection = glm::normalize(lightDirection);

		glm::vec3 upAxis(0.0f, 1.0f, 0.0f);
		if (std::abs(gLightWorldDirection.y) > 0.98f) {
			upAxis = glm::vec3(0.0f, 0.0f, 1.0f);
		}
		const glm::mat4 lightView = glm::lookAt(gLightWorldPosition, lightTarget, upAxis);
		const float objectScaleMax = std::max(gObjectScale.x, std::max(gObjectScale.y, gObjectScale.z));
		const float planeHalfWidth = 0.5f * gPlaneWidth * gPlaneScale;
		const float planeHalfLength = 0.5f * gPlaneLength * gPlaneScale;

		const float sceneRadius =
			std::max(
				std::max(gBounds.MaxExtent(), 1.0f) * objectScaleMax * 1.6f,
				std::max(planeHalfWidth, planeHalfLength) * 1.3f) +
			std::abs(gPlaneHeight) +
			1.0f;
		const float distanceToTarget = glm::length(gLightWorldPosition - lightTarget);
		const float nearPlane = 0.1f;
		const float farPlane = std::max(nearPlane + 1.0f, distanceToTarget + sceneRadius);
		const float clampedInner = std::clamp(gSpotInnerDeg, 1.0f, 69.0f);
		const float clampedOuter = std::clamp(gSpotOuterDeg, clampedInner + 1.0f, 70.0f);
		const float fov = std::clamp(clampedOuter * 2.0f + 6.0f, 20.0f, 150.0f);
		const glm::mat4 lightProjection = glm::perspective(glm::radians(fov), 1.0f, nearPlane, farPlane);
		gLightViewProjection = lightProjection * lightView;
	}

	void RenderShadowDepthPass(const glm::mat4& model) {
		if (gDepthProgram == 0 || gShadowMap.framebuffer == 0 || gVbo == 0 || gEbo == 0) {
			return;
		}

		pglBindFramebuffer(GL_FRAMEBUFFER, gShadowMap.framebuffer);
		glViewport(0, 0, gShadowMap.width, gShadowMap.height);
		glClear(GL_DEPTH_BUFFER_BIT);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(2.0f, 4.0f);

		pglUseProgram(gDepthProgram);
		const glm::mat4 objectModel = model * glm::scale(glm::mat4(1.0f), gObjectScale);
		const glm::mat4 depthMvp = gLightViewProjection * objectModel;
		if (gDepthMvpLocation >= 0) {
			pglUniformMatrix4fv(gDepthMvpLocation, 1, GL_FALSE, glm::value_ptr(depthMvp));
		}
		BindModelBuffers();
		DrawModelGeometry();

		if (gUseVao && gVao != 0) {
			pglBindVertexArray(0);
		}
		pglUseProgram(0);
		glDisable(GL_POLYGON_OFFSET_FILL);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		pglBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void RenderObjectToCurrentTarget(
		const glm::mat4& model,
		const glm::mat4& view,
		const glm::mat4& projection,
		bool drawLightMarker,
		bool forceBlinn,
		bool enableEnvReflection,
		float envReflectionStrength,
		const glm::mat4* lightViewOverride) {
		if (gProgram == 0 || gVbo == 0 || gEbo == 0) {
			return;
		}

		pglUseProgram(gProgram);

		const glm::mat4 objectModel = model * glm::scale(glm::mat4(1.0f), gObjectScale);
		const glm::mat4 mvp = projection * view * objectModel;
		glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(view * objectModel)));
		glm::mat3 worldNormalMatrix = glm::transpose(glm::inverse(glm::mat3(objectModel)));
		const glm::mat3 invViewRotation = glm::transpose(glm::mat3(view));
		const glm::mat4 identity(1.0f);
		const float handedness = glm::determinant(glm::mat3(view * objectModel));
		if (handedness < 0.0f) {
			normalMatrix = -normalMatrix;
			worldNormalMatrix = -worldNormalMatrix;
		}

		if (gMvpLocation >= 0) {
			pglUniformMatrix4fv(gMvpLocation, 1, GL_FALSE, glm::value_ptr(mvp));
		}
		if (gModelLocation >= 0) {
			pglUniformMatrix4fv(gModelLocation, 1, GL_FALSE, glm::value_ptr(objectModel));
		}
		if (gViewLocation >= 0) {
			pglUniformMatrix4fv(gViewLocation, 1, GL_FALSE, glm::value_ptr(view));
		}
		if (gNormalMatrixLocation >= 0) {
			pglUniformMatrix3fv(gNormalMatrixLocation, 1, GL_FALSE, glm::value_ptr(normalMatrix));
		}
		if (gWorldNormalMatrixLocation >= 0) {
			pglUniformMatrix3fv(gWorldNormalMatrixLocation, 1, GL_FALSE, glm::value_ptr(worldNormalMatrix));
		}
		if (gInvViewRotationLocation >= 0) {
			pglUniformMatrix3fv(gInvViewRotationLocation, 1, GL_FALSE, glm::value_ptr(invViewRotation));
		}
		if (gReflectionViewProjLocation >= 0) {
			pglUniformMatrix4fv(gReflectionViewProjLocation, 1, GL_FALSE, glm::value_ptr(identity));
		}
		if (gUseEnvMapLocation >= 0) {
			pglUniform1i(gUseEnvMapLocation, (enableEnvReflection && gEnvironmentCubemap != 0) ? 1 : 0);
		}
		if (gEnvReflectionStrengthLocation >= 0) {
			pglUniform1f(gEnvReflectionStrengthLocation, enableEnvReflection ? envReflectionStrength : 0.0f);
		}
		if (gPlaneReflectionStrengthLocation >= 0) {
			pglUniform1f(gPlaneReflectionStrengthLocation, 0.0f);
		}
		if (gPlaneEnvStrengthLocation >= 0) {
			pglUniform1f(gPlaneEnvStrengthLocation, 0.0f);
		}
		if (gPlaneReflectionBrightnessLocation >= 0) {
			pglUniform1f(gPlaneReflectionBrightnessLocation, 1.0f);
		}
		if (gPlaneColorBiasLocation >= 0) {
			const glm::vec3 zeroBias(0.0f);
			pglUniform3fv(gPlaneColorBiasLocation, 1, glm::value_ptr(zeroBias));
		}

		const glm::mat4& lightView = (lightViewOverride != nullptr) ? *lightViewOverride : view;
		const glm::vec3 lightPosWorld = gLightWorldPosition;
		const glm::vec3 lightPosView = glm::vec3(lightView * glm::vec4(lightPosWorld, 1.0f));
		const glm::vec3 lightDirView = glm::normalize(glm::mat3(lightView) * gLightWorldDirection);
		const glm::vec3 lightColor = gLightColor * gLightIntensity;
		if (gLightPosLocation >= 0) {
			pglUniform3fv(gLightPosLocation, 1, glm::value_ptr(lightPosView));
		}
		if (gLightDirLocation >= 0) {
			pglUniform3fv(gLightDirLocation, 1, glm::value_ptr(lightDirView));
		}
		if (gLightColorLocation >= 0) {
			pglUniform3fv(gLightColorLocation, 1, glm::value_ptr(lightColor));
		}
		if (gLightViewProjLocation >= 0) {
			pglUniformMatrix4fv(gLightViewProjLocation, 1, GL_FALSE, glm::value_ptr(gLightViewProjection));
		}
		if (gReceiveShadowsLocation >= 0) {
			pglUniform1i(gReceiveShadowsLocation, gShadowMap.depthTexture != 0 ? 1 : 0);
		}
		if (gShadowBiasLocation >= 0) {
			pglUniform1f(gShadowBiasLocation, gShadowBias);
		}
		if (gUseSpotLightLocation >= 0) {
			pglUniform1i(gUseSpotLightLocation, 1);
		}
		const float clampedInner = std::clamp(gSpotInnerDeg, 1.0f, 89.0f);
		const float clampedOuter = std::clamp(gSpotOuterDeg, clampedInner + 1.0f, 89.9f);
		const float spotInnerCos = std::cos(glm::radians(clampedInner));
		const float spotOuterCos = std::cos(glm::radians(clampedOuter));
		if (gSpotCosInnerLocation >= 0) {
			pglUniform1f(gSpotCosInnerLocation, spotInnerCos);
		}
		if (gSpotCosOuterLocation >= 0) {
			pglUniform1f(gSpotCosOuterLocation, spotOuterCos);
		}
		if (gShadeModeLocation >= 0) {
			pglUniform1i(gShadeModeLocation, (!forceBlinn && gShowNormals) ? 1 : 0);
		}

		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE3);
		}
		glBindTexture(GL_TEXTURE_2D, gShadowMap.depthTexture);
		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE2);
		}
		glBindTexture(GL_TEXTURE_CUBE_MAP, enableEnvReflection ? gEnvironmentCubemap : 0);
		BindModelBuffers();

		auto applyMaterial = [&](const Material& material) {
			if (gAmbientLocation >= 0) {
				pglUniform3fv(gAmbientLocation, 1, glm::value_ptr(material.ambient));
			}
			if (gDiffuseLocation >= 0) {
				pglUniform3fv(gDiffuseLocation, 1, glm::value_ptr(material.diffuse));
			}
			if (gSpecularLocation >= 0) {
				pglUniform3fv(gSpecularLocation, 1, glm::value_ptr(material.specular));
			}
			if (gShininessLocation >= 0) {
				pglUniform1f(gShininessLocation, material.shininess);
			}
			if (gUseDiffuseMapLocation >= 0) {
				pglUniform1i(gUseDiffuseMapLocation, material.hasDiffuseTexture ? 1 : 0);
			}
			if (gUseSpecularMapLocation >= 0) {
				pglUniform1i(gUseSpecularMapLocation, material.hasSpecularTexture ? 1 : 0);
			}
			if (pglActiveTexture) {
				pglActiveTexture(GL_TEXTURE0);
			}
			glBindTexture(GL_TEXTURE_2D, material.hasDiffuseTexture ? material.diffuseTexture : 0);
			if (pglActiveTexture) {
				pglActiveTexture(GL_TEXTURE1);
			}
			glBindTexture(GL_TEXTURE_2D, material.hasSpecularTexture ? material.specularTexture : 0);
		};

		int lastMaterial = -1;
		if (gSubmeshes.empty()) {
			const Material& fallback = gMaterials.empty() ? Material{} : gMaterials.front();
			applyMaterial(fallback);
			glDrawElements(GL_TRIANGLES, gIndexCount, GL_UNSIGNED_INT, nullptr);
		} else {
			for (const Submesh& submesh : gSubmeshes) {
				const int matIndex = (submesh.materialIndex >= 0 && submesh.materialIndex < static_cast<int>(gMaterials.size()))
					? submesh.materialIndex
					: 0;
				if (matIndex != lastMaterial) {
					const Material& material = gMaterials.empty() ? Material{} : gMaterials[matIndex];
					applyMaterial(material);
					lastMaterial = matIndex;
				}
				const void* offset = reinterpret_cast<const void*>(static_cast<size_t>(submesh.indexOffset) * sizeof(unsigned int));
				glDrawElements(GL_TRIANGLES, submesh.indexCount, GL_UNSIGNED_INT, offset);
			}
		}

		if (drawLightMarker && gShowLightMarker && gLightVbo != 0) {
			const LightMarkerShape selectedShape = static_cast<LightMarkerShape>(
				std::clamp(gLightMarkerShape, 0, static_cast<int>(LightMarkerShape::LightObject)));

			const GLMeshBuffers* selectedMesh = nullptr;
			switch (selectedShape) {
			case LightMarkerShape::Cube:
				selectedMesh = &gLightCubeMesh;
				break;
			case LightMarkerShape::Sphere:
				selectedMesh = &gLightSphereMesh;
				break;
			case LightMarkerShape::LightObject:
				selectedMesh = gHasLightObjectMesh ? &gLightObjectMesh : &gLightSphereMesh;
				break;
			case LightMarkerShape::Point:
			default:
				selectedMesh = nullptr;
				break;
			}

			glm::mat4 modelLight = glm::translate(glm::mat4(1.0f), lightPosWorld);
			if (selectedShape == LightMarkerShape::LightObject) {
				modelLight = modelLight * BuildOrientationFromForward(gLightWorldDirection);
			}
			if (selectedMesh != nullptr && selectedMesh->indexCount > 0) {
				const float markerScale = std::max(
					kLightMarkerMeshScaleMin,
					gBounds.MaxExtent() * kLightMarkerMeshScaleRatio * gLightMarkerScale);
				modelLight = modelLight * glm::scale(glm::mat4(1.0f), glm::vec3(markerScale));
			}

			const glm::mat4 mvpLight = projection * view * modelLight;
			const glm::mat3 normalMatrixLight = glm::transpose(glm::inverse(glm::mat3(view * modelLight)));
			const glm::mat3 worldNormalMatrixLight = glm::transpose(glm::inverse(glm::mat3(modelLight)));
			if (gMvpLocation >= 0) {
				pglUniformMatrix4fv(gMvpLocation, 1, GL_FALSE, glm::value_ptr(mvpLight));
			}
			if (gModelLocation >= 0) {
				pglUniformMatrix4fv(gModelLocation, 1, GL_FALSE, glm::value_ptr(modelLight));
			}
			if (gNormalMatrixLocation >= 0) {
				pglUniformMatrix3fv(gNormalMatrixLocation, 1, GL_FALSE, glm::value_ptr(normalMatrixLight));
			}
			if (gWorldNormalMatrixLocation >= 0) {
				pglUniformMatrix3fv(gWorldNormalMatrixLocation, 1, GL_FALSE, glm::value_ptr(worldNormalMatrixLight));
			}
			if (gShadeModeLocation >= 0) {
				pglUniform1i(gShadeModeLocation, 2);
			}
			if (gMarkerColorLocation >= 0) {
				pglUniform3fv(gMarkerColorLocation, 1, glm::value_ptr(gLightMarkerColor));
			}

			if (selectedMesh != nullptr && selectedMesh->indexCount > 0) {
				BindMeshBuffers(*selectedMesh);
				glDrawElements(GL_TRIANGLES, selectedMesh->indexCount, GL_UNSIGNED_INT, nullptr);
				if (gUseVao && selectedMesh->vao != 0) {
					pglBindVertexArray(0);
				}
			} else {
				glPointSize(kLightMarkerPointSize * std::max(0.1f, gLightMarkerScale));
				if (gUseVao && gLightVao != 0) {
					pglBindVertexArray(gLightVao);
				} else {
					pglBindBuffer(GL_ARRAY_BUFFER, gLightVbo);
					pglEnableVertexAttribArray(0);
					pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
					pglEnableVertexAttribArray(1);
					pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
					pglEnableVertexAttribArray(2);
					pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texcoord)));
				}
				glDrawArrays(GL_POINTS, 0, 1);
				if (gUseVao && gLightVao != 0) {
					pglBindVertexArray(0);
				}
				glPointSize(1.0f);
			}
		}

		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE3);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE2);
		}
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE1);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE0);
		}
		glBindTexture(GL_TEXTURE_2D, 0);

		if (gUseVao && gVao != 0) {
			pglBindVertexArray(0);
		}
		pglUseProgram(0);
	}

	void RenderBackgroundToCurrentTarget(const glm::mat4& view, const glm::mat4& projection) {
		if (gProgram == 0 || gBackgroundVbo == 0 || gBackgroundEbo == 0 || gBackgroundIndexCount <= 0 || gEnvironmentCubemap == 0) {
			return;
		}

		pglUseProgram(gProgram);

		const glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(kSkyboxDepth));
		const glm::mat4 skyboxView = glm::mat4(glm::mat3(view));
		const glm::mat4 mvp = projection * skyboxView * model;
		const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(skyboxView * model)));
		const glm::mat3 worldNormalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
		const glm::mat3 invViewRotation = glm::transpose(glm::mat3(view));
		const glm::mat4 identity(1.0f);

		if (gMvpLocation >= 0) {
			pglUniformMatrix4fv(gMvpLocation, 1, GL_FALSE, glm::value_ptr(mvp));
		}
		if (gModelLocation >= 0) {
			pglUniformMatrix4fv(gModelLocation, 1, GL_FALSE, glm::value_ptr(model));
		}
		if (gViewLocation >= 0) {
			pglUniformMatrix4fv(gViewLocation, 1, GL_FALSE, glm::value_ptr(skyboxView));
		}
		if (gNormalMatrixLocation >= 0) {
			pglUniformMatrix3fv(gNormalMatrixLocation, 1, GL_FALSE, glm::value_ptr(normalMatrix));
		}
		if (gWorldNormalMatrixLocation >= 0) {
			pglUniformMatrix3fv(gWorldNormalMatrixLocation, 1, GL_FALSE, glm::value_ptr(worldNormalMatrix));
		}
		if (gInvViewRotationLocation >= 0) {
			pglUniformMatrix3fv(gInvViewRotationLocation, 1, GL_FALSE, glm::value_ptr(invViewRotation));
		}
		if (gReflectionViewProjLocation >= 0) {
			pglUniformMatrix4fv(gReflectionViewProjLocation, 1, GL_FALSE, glm::value_ptr(identity));
		}
		if (gLightViewProjLocation >= 0) {
			pglUniformMatrix4fv(gLightViewProjLocation, 1, GL_FALSE, glm::value_ptr(identity));
		}
		if (gReceiveShadowsLocation >= 0) {
			pglUniform1i(gReceiveShadowsLocation, 0);
		}
		if (gUseSpotLightLocation >= 0) {
			pglUniform1i(gUseSpotLightLocation, 0);
		}
		if (gShadeModeLocation >= 0) {
			pglUniform1i(gShadeModeLocation, 4);
		}
		if (gUseEnvMapLocation >= 0) {
			pglUniform1i(gUseEnvMapLocation, 1);
		}
		if (gUseDiffuseMapLocation >= 0) {
			pglUniform1i(gUseDiffuseMapLocation, 0);
		}
		if (gUseSpecularMapLocation >= 0) {
			pglUniform1i(gUseSpecularMapLocation, 0);
		}
		if (gPlaneColorBiasLocation >= 0) {
			const glm::vec3 zeroBias(0.0f);
			pglUniform3fv(gPlaneColorBiasLocation, 1, glm::value_ptr(zeroBias));
		}

		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE3);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE0);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE1);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE2);
		}
		glBindTexture(GL_TEXTURE_CUBE_MAP, gEnvironmentCubemap);

		glDepthMask(GL_FALSE);
		glDepthFunc(GL_LEQUAL);
		BindBackgroundBuffers();
		glDrawElements(GL_TRIANGLES, gBackgroundIndexCount, GL_UNSIGNED_INT, nullptr);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);

		if (gUseVao && gBackgroundVao != 0) {
			pglBindVertexArray(0);
		}
		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE3);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE2);
		}
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE1);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE0);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		pglUseProgram(0);
	}

	void RenderPlaneToCurrentTarget(
		const glm::mat4& view,
		const glm::mat4& projection,
		const glm::mat4& reflectionViewProj) {
		if (gProgram == 0 || gPlaneVbo == 0 || gPlaneEbo == 0) {
			return;
		}

		pglUseProgram(gProgram);

		const glm::mat4 model = BuildPlaneModelMatrix();
		const glm::mat4 mvp = projection * view * model;
		const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(view * model)));
		const glm::mat3 worldNormalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
		const glm::mat3 invViewRotation = glm::transpose(glm::mat3(view));
		const glm::vec3 lightPosView = glm::vec3(view * glm::vec4(gLightWorldPosition, 1.0f));
		const glm::vec3 lightDirView = glm::normalize(glm::mat3(view) * gLightWorldDirection);
		const glm::vec3 lightColor = gLightColor * gLightIntensity;
		const bool useReflectionTexture = gRenderToPlane && gRenderTexture.colorTexture != 0;

		if (gMvpLocation >= 0) {
			pglUniformMatrix4fv(gMvpLocation, 1, GL_FALSE, glm::value_ptr(mvp));
		}
		if (gModelLocation >= 0) {
			pglUniformMatrix4fv(gModelLocation, 1, GL_FALSE, glm::value_ptr(model));
		}
		if (gViewLocation >= 0) {
			pglUniformMatrix4fv(gViewLocation, 1, GL_FALSE, glm::value_ptr(view));
		}
		if (gNormalMatrixLocation >= 0) {
			pglUniformMatrix3fv(gNormalMatrixLocation, 1, GL_FALSE, glm::value_ptr(normalMatrix));
		}
		if (gWorldNormalMatrixLocation >= 0) {
			pglUniformMatrix3fv(gWorldNormalMatrixLocation, 1, GL_FALSE, glm::value_ptr(worldNormalMatrix));
		}
		if (gInvViewRotationLocation >= 0) {
			pglUniformMatrix3fv(gInvViewRotationLocation, 1, GL_FALSE, glm::value_ptr(invViewRotation));
		}
		if (gReflectionViewProjLocation >= 0) {
			pglUniformMatrix4fv(gReflectionViewProjLocation, 1, GL_FALSE, glm::value_ptr(reflectionViewProj));
		}
		if (gLightPosLocation >= 0) {
			pglUniform3fv(gLightPosLocation, 1, glm::value_ptr(lightPosView));
		}
		if (gLightDirLocation >= 0) {
			pglUniform3fv(gLightDirLocation, 1, glm::value_ptr(lightDirView));
		}
		if (gLightColorLocation >= 0) {
			pglUniform3fv(gLightColorLocation, 1, glm::value_ptr(lightColor));
		}
		if (gLightViewProjLocation >= 0) {
			pglUniformMatrix4fv(gLightViewProjLocation, 1, GL_FALSE, glm::value_ptr(gLightViewProjection));
		}
		if (gReceiveShadowsLocation >= 0) {
			pglUniform1i(gReceiveShadowsLocation, gShadowMap.depthTexture != 0 ? 1 : 0);
		}
		if (gShadowBiasLocation >= 0) {
			pglUniform1f(gShadowBiasLocation, gShadowBias);
		}
		if (gUseSpotLightLocation >= 0) {
			pglUniform1i(gUseSpotLightLocation, 1);
		}
		const float clampedInner = std::clamp(gSpotInnerDeg, 1.0f, 89.0f);
		const float clampedOuter = std::clamp(gSpotOuterDeg, clampedInner + 1.0f, 89.9f);
		const float spotInnerCos = std::cos(glm::radians(clampedInner));
		const float spotOuterCos = std::cos(glm::radians(clampedOuter));
		if (gSpotCosInnerLocation >= 0) {
			pglUniform1f(gSpotCosInnerLocation, spotInnerCos);
		}
		if (gSpotCosOuterLocation >= 0) {
			pglUniform1f(gSpotCosOuterLocation, spotOuterCos);
		}
		if (gShadeModeLocation >= 0) {
			pglUniform1i(gShadeModeLocation, 3);
		}
		if (gUseDiffuseMapLocation >= 0) {
			pglUniform1i(gUseDiffuseMapLocation, useReflectionTexture ? 1 : 0);
		}
		if (gUseSpecularMapLocation >= 0) {
			pglUniform1i(gUseSpecularMapLocation, 0);
		}
		if (gUseEnvMapLocation >= 0) {
			pglUniform1i(gUseEnvMapLocation, gEnvironmentCubemap != 0 ? 1 : 0);
		}
		if (gAmbientLocation >= 0) {
			pglUniform3fv(gAmbientLocation, 1, glm::value_ptr(gPlaneAmbientColor));
		}
		if (gDiffuseLocation >= 0) {
			pglUniform3fv(gDiffuseLocation, 1, glm::value_ptr(gPlaneDiffuseColor));
		}
		if (gSpecularLocation >= 0) {
			pglUniform3fv(gSpecularLocation, 1, glm::value_ptr(gPlaneSpecularColor));
		}
		if (gShininessLocation >= 0) {
			pglUniform1f(gShininessLocation, gPlaneShininess);
		}
		if (gPlaneColorBiasLocation >= 0) {
			pglUniform3fv(gPlaneColorBiasLocation, 1, glm::value_ptr(gPlaneColorBias));
		}
		if (gEnvReflectionStrengthLocation >= 0) {
			pglUniform1f(gEnvReflectionStrengthLocation, 0.0f);
		}
		if (gPlaneReflectionStrengthLocation >= 0) {
			pglUniform1f(gPlaneReflectionStrengthLocation, gPlaneRttReflectionStrength);
		}
		if (gPlaneEnvStrengthLocation >= 0) {
			pglUniform1f(gPlaneEnvStrengthLocation, gPlaneEnvReflectionStrength);
		}
		if (gPlaneReflectionBrightnessLocation >= 0) {
			pglUniform1f(gPlaneReflectionBrightnessLocation, gPlaneRttReflectionBrightness);
		}

		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE0);
		}
		glBindTexture(GL_TEXTURE_2D, useReflectionTexture ? gRenderTexture.colorTexture : 0);
		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE1);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE2);
		}
		glBindTexture(GL_TEXTURE_CUBE_MAP, gEnvironmentCubemap);
		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE3);
		}
		glBindTexture(GL_TEXTURE_2D, gShadowMap.depthTexture);

		BindPlaneBuffers();
		glDrawElements(GL_TRIANGLES, gPlaneIndexCount, GL_UNSIGNED_INT, nullptr);

		if (gUseVao && gPlaneVao != 0) {
			pglBindVertexArray(0);
		}
		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE3);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE2);
		}
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE1);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		if (pglActiveTexture) {
			pglActiveTexture(GL_TEXTURE0);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		pglUseProgram(0);
	}

	void UpdateWindowTitle() {
		if (!gWindow) {
			return;
		}
		const char* mode = gUsePerspective ? "Perspective" : "Orthographic";
		const char* shade = gShowNormals ? "Normals" : "Blinn+Env";
		char title[160];
		std::snprintf(title, sizeof(title), "%s | %s | %s", gpurenderer::config::kWindowTitleBase, mode, shade);
		glfwSetWindowTitle(gWindow, title);
	}

	void Display() {
		if (gProgram == 0 || gVbo == 0 || gEbo == 0) {
			return;
		}

		const glm::mat4 model = BuildObjectModelMatrix(gObjectCamera);
		const glm::mat4 view = BuildViewMatrix(gObjectCamera);
		const glm::mat4 projection = BuildProjectionMatrix(gWindowWidth, gWindowHeight, gNearPlane, gFarPlane);
		const glm::mat4 reflectionView = view * BuildPlanarReflectionMatrix(gPlaneHeight);
		const glm::mat4 reflectionViewProj = projection * reflectionView;
		UpdateLightShadowState(model);
		if (CreateOrResizeShadowMap(kShadowMapResolution, kShadowMapResolution)) {
			RenderShadowDepthPass(model);
		}

		if (gRenderToPlane && gPlaneVbo != 0 && gPlaneEbo != 0) {
			if (CreateOrResizeRenderTexture(gWindowWidth, gWindowHeight)) {
				pglBindFramebuffer(GL_FRAMEBUFFER, gRenderTexture.framebuffer);
				glViewport(0, 0, gRenderTexture.width, gRenderTexture.height);
				glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				RenderObjectToCurrentTarget(
					model,
					reflectionView,
					projection,
					false,
					true,
					true,
					kObjectEnvReflectionStrength,
					&view);
				pglBindFramebuffer(GL_FRAMEBUFFER, 0);
				GenerateRenderTextureMipmaps();
			}
		}

		pglBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, gWindowWidth, gWindowHeight);
		glClearColor(gSceneBackgroundColor.x, gSceneBackgroundColor.y, gSceneBackgroundColor.z, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		RenderBackgroundToCurrentTarget(view, projection);
		if (gPlaneVbo != 0 && gPlaneEbo != 0) {
			RenderPlaneToCurrentTarget(view, projection, reflectionViewProj);
		}
		RenderObjectToCurrentTarget(
			model,
			view,
			projection,
			true,
			false,
			true,
			kObjectEnvReflectionStrength,
			nullptr);
	}

	void Reshape(GLFWwindow*, int width, int height) {
		gWindowWidth = width > 0 ? width : 1;
		gWindowHeight = height > 0 ? height : 1;
		glViewport(0, 0, gWindowWidth, gWindowHeight);
		CreateOrResizeRenderTexture(gWindowWidth, gWindowHeight);
		CreateOrResizeShadowMap(kShadowMapResolution, kShadowMapResolution);
	}

	bool IsCtrlDown(GLFWwindow* window) {
		return glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
			glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
	}

	bool IsGuiCapturingMouse() {
		return gGuiInitialized && ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
	}

	bool IsGuiCapturingKeyboard() {
		return gGuiInitialized && ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureKeyboard;
	}

	void MouseButton(GLFWwindow* window, int button, int action, int mods) {
		if (gGuiInitialized) {
			ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
		}
		const bool captured = IsGuiCapturingMouse();
		const bool pressed = (action == GLFW_PRESS);

		if (button == GLFW_MOUSE_BUTTON_LEFT) {
			gLeftDown = !captured && pressed;
		} else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
			gRightDown = !captured && pressed;
		} else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
			gMiddleDown = !captured && pressed;
		}
		glfwGetCursorPos(window, &gLastMouseX, &gLastMouseY);
		if (captured) {
			return;
		}
	}

	void MouseMotion(GLFWwindow* window, double x, double y) {
		if (gGuiInitialized) {
			ImGui_ImplGlfw_CursorPosCallback(window, x, y);
		}
		if (IsGuiCapturingMouse()) {
			gLastMouseX = x;
			gLastMouseY = y;
			return;
		}

		const float dx = static_cast<float>(x - gLastMouseX);
		const float dy = static_cast<float>(y - gLastMouseY);
		OrbitCamera& targetCamera = gObjectCamera;
		const float viewHeight = gUsePerspective
			? (2.0f * targetCamera.distance * std::tan(0.5f * DegreesToRadians(kDefaultFovDeg)))
			: 2.0f;
		const float panScale = (gWindowHeight > 0) ? (viewHeight / static_cast<float>(gWindowHeight)) : 0.0f;

		if (gLeftDown) {
			if (IsCtrlDown(window)) {
				gLightYawDeg += dx * kLightRotationSpeedDegPerPixel;
				gLightPitchDeg += dy * kLightRotationSpeedDegPerPixel;
				gLightPitchDeg = std::clamp(gLightPitchDeg, -89.0f, 89.0f);
			} else {
				targetCamera.yawDeg += dx * kRotationSpeedDegPerPixel;
				targetCamera.pitchDeg += dy * kRotationSpeedDegPerPixel;
				targetCamera.pitchDeg = std::clamp(targetCamera.pitchDeg, -89.0f, 89.0f);
			}
		}
		if (gRightDown) {
			targetCamera.distance += dy * kZoomSpeed;
			targetCamera.distance = std::max(targetCamera.distance, kMinCameraDistance);
		}
		if (gMiddleDown) {
			targetCamera.panX += dx * panScale;
			targetCamera.panY -= dy * panScale;
		}

		gLastMouseX = x;
		gLastMouseY = y;
	}

	void Scroll(GLFWwindow* window, double xoffset, double yoffset) {
		if (gGuiInitialized) {
			ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
		}
		if (IsGuiCapturingMouse()) {
			return;
		}

		if (yoffset != 0.0) {
			OrbitCamera& targetCamera = gObjectCamera;
			targetCamera.distance -= static_cast<float>(yoffset) * 0.2f;
			targetCamera.distance = std::max(targetCamera.distance, kMinCameraDistance);
		}
	}

	void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
		if (gGuiInitialized) {
			ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
		}
		if (IsGuiCapturingKeyboard() && key != GLFW_KEY_ESCAPE) {
			return;
		}

		if (action != GLFW_PRESS && action != GLFW_REPEAT) {
			return;
		}
		if (key == GLFW_KEY_ESCAPE) {
			glfwSetWindowShouldClose(window, GLFW_TRUE);
			return;
		}
		if (key == GLFW_KEY_P) {
			gUsePerspective = !gUsePerspective;
			UpdateWindowTitle();
			return;
		}
		if (key == GLFW_KEY_R) {
			gObjectCamera.yawDeg = 0.0f;
			gObjectCamera.pitchDeg = 0.0f;
			gObjectCamera.panX = 0.0f;
			gObjectCamera.panY = 0.0f;
			return;
		}
		if (key == GLFW_KEY_N) {
			gShowNormals = !gShowNormals;
			UpdateWindowTitle();
			return;
		}
		if (key == GLFW_KEY_F6) {
			if (!ReloadShaders() || !ReloadDepthShader()) {
				std::fprintf(stderr, "F6 shader reload failed.\n");
			} else {
				std::fprintf(stdout, "Shaders reloaded.\n");
			}
			return;
		}
	}

	void CharCallback(GLFWwindow* window, unsigned int codepoint) {
		if (gGuiInitialized) {
			ImGui_ImplGlfw_CharCallback(window, codepoint);
		}
	}

	void CopyObjPathToInput(const std::string& path) {
		std::snprintf(gObjPathInput, sizeof(gObjPathInput), "%s", path.c_str());
	}

	void RefreshAvailableObjFiles() {
		gAvailableObjFiles.clear();
		std::error_code ec;
		const std::filesystem::path root = std::filesystem::current_path(ec);
		if (ec) {
			return;
		}

		size_t count = 0;
		constexpr size_t kMaxDiscoveredObjs = 512;
		for (std::filesystem::recursive_directory_iterator it(
			 root,
			 std::filesystem::directory_options::skip_permission_denied,
			 ec), end;
			 it != end && !ec;
			 it.increment(ec)) {
			if (ec || !it->is_regular_file()) {
				continue;
			}

			const std::filesystem::path ext = it->path().extension();
			std::string extLower = ext.string();
			std::transform(extLower.begin(), extLower.end(), extLower.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
				});
			if (extLower != ".obj") {
				continue;
			}

			gAvailableObjFiles.push_back(it->path().lexically_normal().string());
			++count;
			if (count >= kMaxDiscoveredObjs) {
				break;
			}
		}

		std::sort(gAvailableObjFiles.begin(), gAvailableObjFiles.end());
		gAvailableObjFiles.erase(
			std::unique(gAvailableObjFiles.begin(), gAvailableObjFiles.end()),
			gAvailableObjFiles.end());

		gSelectedObjIndex = -1;
		for (size_t i = 0; i < gAvailableObjFiles.size(); ++i) {
			if (gAvailableObjFiles[i] == gObjPath) {
				gSelectedObjIndex = static_cast<int>(i);
				break;
			}
		}
	}

	void DestroyModelBuffers() {
		if (pglDeleteBuffers) {
			if (gEbo != 0) {
				pglDeleteBuffers(1, &gEbo);
				gEbo = 0;
			}
			if (gVbo != 0) {
				pglDeleteBuffers(1, &gVbo);
				gVbo = 0;
			}
		}
		if (gUseVao && pglDeleteVertexArrays && gVao != 0) {
			pglDeleteVertexArrays(1, &gVao);
			gVao = 0;
		}
	}

	bool LoadModelFromPath(const std::string& objPath) {
		if (objPath.empty()) {
			gModelLoadStatus = "Model load failed: path is empty.";
			return false;
		}

		std::vector<Vertex> newVertices;
		std::vector<unsigned int> newIndices;
		std::vector<Submesh> newSubmeshes;
		std::vector<Material> newMaterials;
		Bounds newBounds;
		if (!LoadMesh(objPath, newVertices, newIndices, newBounds, newSubmeshes, newMaterials)) {
			gModelLoadStatus = "Model load failed: " + objPath;
			return false;
		}

		DestroyModelBuffers();

		gVertices = std::move(newVertices);
		gIndices = std::move(newIndices);
		gSubmeshes = std::move(newSubmeshes);
		gMaterials = std::move(newMaterials);
		gBounds = newBounds;
		gIndexCount = static_cast<int>(gIndices.size());

		gCenter = gBounds.Center();
		const float extent = std::max(gBounds.MaxExtent(), 1.0f);
		gObjectCamera.distance = extent * 2.5f;
		gObjectCamera.panX = 0.0f;
		gObjectCamera.panY = 0.0f;
		gPlaneHeight = (gBounds.min.y - gCenter.y) - (extent * kPlanePaddingRatio);
		gPlaneWidth = 2.0f * extent * kPlaneSizeMultiplier;
		gPlaneLength = 2.0f * extent * kPlaneSizeMultiplier;
		gPlaneScale = 1.0f;
		gObjectScale = glm::vec3(1.0f);
		gLightMarkerScale = 1.0f;
		gLightDistance = extent * 1.8f;
		gNearPlane = std::max(0.1f, extent * 0.001f);
		gFarPlane = std::max(kFarPlane, gObjectCamera.distance + extent * 3.0f);
		gSelectedMaterialIndex = 0;

		CreateBuffers();

		gObjPath = objPath;
		CopyObjPathToInput(gObjPath);
		RefreshAvailableObjFiles();
		gModelLoadStatus = "Loaded model: " + gObjPath;
		return true;
	}

	bool InitializeGui(GLFWwindow* window) {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();
		ImGuiIO& io = ImGui::GetIO();
		// Avoid stale off-screen panel positions from old imgui.ini layouts.
		io.IniFilename = nullptr;
		gGuiBackend = GuiBackend::None;

		if (!ImGui_ImplGlfw_InitForOpenGL(window, false)) {
			ImGui::DestroyContext();
			return false;
		}

		if (ImGui_ImplOpenGL3_Init("#version 130")) {
			gGuiBackend = GuiBackend::OpenGL3;
			gGuiInitialized = true;
			return true;
		}
		if (ImGui_ImplOpenGL2_Init()) {
			gGuiBackend = GuiBackend::OpenGL2;
			gGuiInitialized = true;
			return true;
		}

		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		return false;
	}

	void ShutdownGui() {
		if (!gGuiInitialized) {
			return;
		}
		if (gGuiBackend == GuiBackend::OpenGL3) {
			ImGui_ImplOpenGL3_Shutdown();
		} else if (gGuiBackend == GuiBackend::OpenGL2) {
			ImGui_ImplOpenGL2_Shutdown();
		}
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		gGuiInitialized = false;
		gGuiBackend = GuiBackend::None;
	}

	void BeginGuiFrame() {
		if (!gGuiInitialized) {
			return;
		}
		if (gGuiBackend == GuiBackend::OpenGL3) {
			ImGui_ImplOpenGL3_NewFrame();
		} else if (gGuiBackend == GuiBackend::OpenGL2) {
			ImGui_ImplOpenGL2_NewFrame();
		}
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void DrawGuiPanel() {
		if (!gGuiInitialized) {
			return;
		}

		const ImGuiIO& io = ImGui::GetIO();
		const float viewportWidth = std::max(1.0f, io.DisplaySize.x);
		const float viewportHeight = std::max(1.0f, io.DisplaySize.y);
		const float paddingX = viewportWidth * kGuiPanelViewportPaddingFraction;
		const float paddingY = viewportHeight * kGuiPanelViewportPaddingFraction;
		const float minX = paddingX;
		const float minY = paddingY;
		const float maxRight = viewportWidth - paddingX;
		const float maxBottom = viewportHeight - paddingY;
		const float availableWidth = std::max(1.0f, maxRight - minX);
		const float availableHeight = std::max(1.0f, maxBottom - minY);
		const ImVec2 minPanelSize(
			std::min(kGuiPanelMinWidth, availableWidth),
			std::min(kGuiPanelMinHeight, availableHeight));
		const ImVec2 maxPanelSize(
			availableWidth,
			availableHeight);
		const ImVec2 initialPos(
			std::max(12.0f, minX),
			std::max(12.0f, minY));

		ImGui::SetNextWindowPos(initialPos, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(kGuiPanelWidth, kGuiPanelHeight), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(minPanelSize, maxPanelSize);
		if (!ImGui::Begin("Renderer Controls")) {
			ImGui::End();
			return;
		}

		// Keep the GUI fully inside the viewport and preserve the opposite edge while resizing.
		ImVec2 windowPos = ImGui::GetWindowPos();
		ImVec2 windowSize = ImGui::GetWindowSize();
		windowSize.x = std::clamp(windowSize.x, minPanelSize.x, maxPanelSize.x);
		windowSize.y = std::clamp(windowSize.y, minPanelSize.y, maxPanelSize.y);
		const ImGuiMouseCursor mouseCursor = ImGui::GetMouseCursor();
		const bool resizeX = io.MouseDown[ImGuiMouseButton_Left] &&
			(mouseCursor == ImGuiMouseCursor_ResizeEW ||
				mouseCursor == ImGuiMouseCursor_ResizeNWSE ||
				mouseCursor == ImGuiMouseCursor_ResizeNESW);
		const bool resizeY = io.MouseDown[ImGuiMouseButton_Left] &&
			(mouseCursor == ImGuiMouseCursor_ResizeNS ||
				mouseCursor == ImGuiMouseCursor_ResizeNWSE ||
				mouseCursor == ImGuiMouseCursor_ResizeNESW);

		if (windowPos.x < minX) {
			if (resizeX) {
				windowSize.x = std::clamp(windowSize.x + (windowPos.x - minX), minPanelSize.x, maxPanelSize.x);
			}
			windowPos.x = minX;
		}
		if (windowPos.x + windowSize.x > maxRight) {
			if (resizeX) {
				windowSize.x = std::clamp(maxRight - windowPos.x, minPanelSize.x, maxPanelSize.x);
			}
			windowPos.x = std::clamp(windowPos.x, minX, std::max(minX, maxRight - windowSize.x));
		}

		if (windowPos.y < minY) {
			if (resizeY) {
				windowSize.y = std::clamp(windowSize.y + (windowPos.y - minY), minPanelSize.y, maxPanelSize.y);
			}
			windowPos.y = minY;
		}
		if (windowPos.y + windowSize.y > maxBottom) {
			if (resizeY) {
				windowSize.y = std::clamp(maxBottom - windowPos.y, minPanelSize.y, maxPanelSize.y);
			}
			windowPos.y = std::clamp(windowPos.y, minY, std::max(minY, maxBottom - windowSize.y));
		}

		if (std::abs(windowSize.x - ImGui::GetWindowSize().x) > 0.5f || std::abs(windowSize.y - ImGui::GetWindowSize().y) > 0.5f) {
			ImGui::SetWindowSize(windowSize);
		}
		if (std::abs(windowPos.x - ImGui::GetWindowPos().x) > 0.5f || std::abs(windowPos.y - ImGui::GetWindowPos().y) > 0.5f) {
			ImGui::SetWindowPos(windowPos);
		}

		ImGui::Checkbox("Enable Plane Reflection", &gRenderToPlane);
		ImGui::ColorEdit3("Viewport Background", glm::value_ptr(gSceneBackgroundColor));
		if (gRenderToPlane) {
			ImGui::ColorEdit3("Offscreen Background", glm::value_ptr(gOffscreenBackgroundColor));
		}
		if (!gEnvironmentLoadStatus.empty()) {
			ImGui::TextWrapped("%s", gEnvironmentLoadStatus.c_str());
		}

		ImGui::Separator();
		ImGui::TextUnformatted("Light");
		ImGui::ColorEdit3("Light Color", glm::value_ptr(gLightColor));
		ImGui::ColorEdit3("Light Marker Color", glm::value_ptr(gLightMarkerColor));
		ImGui::SliderFloat("Light Intensity", &gLightIntensity, 0.0f, 5.0f, "%.2f");
		ImGui::SliderFloat("Light Distance", &gLightDistance, 0.1f, 200.0f, "%.2f");
		ImGui::Checkbox("Show Light Marker", &gShowLightMarker);
		if (gShowLightMarker) {
			const char* markerModes[] = {"Point", "Cube", "Sphere", "Light Object"};
			ImGui::Combo("Light Appearance", &gLightMarkerShape, markerModes, IM_ARRAYSIZE(markerModes));
			if (gLightMarkerShape == static_cast<int>(LightMarkerShape::LightObject) && !gHasLightObjectMesh) {
				ImGui::TextWrapped("Light object mesh unavailable; using sphere fallback.");
			}
		}
		ImGui::SliderFloat("Spot Inner Angle", &gSpotInnerDeg, 5.0f, 60.0f, "%.1f deg");
		ImGui::SliderFloat("Spot Outer Angle", &gSpotOuterDeg, 6.0f, 75.0f, "%.1f deg");
		gSpotInnerDeg = std::clamp(gSpotInnerDeg, 1.0f, 69.0f);
		gSpotOuterDeg = std::clamp(gSpotOuterDeg, gSpotInnerDeg + 1.0f, 89.9f);
		ImGui::SliderFloat("Shadow Bias", &gShadowBias, 0.00001f, 0.00200f, "%.5f");
		const glm::mat4 model = BuildObjectModelMatrix(gObjectCamera);
		const glm::vec3 lightPos = ComputeLightPosition(model);
		ImGui::Text("Light Position: (%.2f, %.2f, %.2f)", lightPos.x, lightPos.y, lightPos.z);
		if (!gLightObjectStatus.empty()) {
			ImGui::TextWrapped("%s", gLightObjectStatus.c_str());
		}

		ImGui::Separator();
		ImGui::TextUnformatted("Transforms");
		ImGui::DragFloat3("Object Scale (XYZ)", glm::value_ptr(gObjectScale), 0.01f, 0.01f, 50.0f, "%.2f");
		ImGui::SliderFloat("Plane Width", &gPlaneWidth, 0.1f, 400.0f, "%.2f");
		ImGui::SliderFloat("Plane Length", &gPlaneLength, 0.1f, 400.0f, "%.2f");
		ImGui::SliderFloat("Plane Scale", &gPlaneScale, 0.05f, 20.0f, "%.2f");
		ImGui::SliderFloat("Light Marker Scale", &gLightMarkerScale, 0.1f, 10.0f, "%.2f");
		gObjectScale = glm::max(gObjectScale, glm::vec3(0.01f));
		gPlaneWidth = std::max(gPlaneWidth, 0.05f);
		gPlaneLength = std::max(gPlaneLength, 0.05f);
		gPlaneScale = std::max(gPlaneScale, 0.01f);
		gLightMarkerScale = std::max(gLightMarkerScale, 0.1f);

		ImGui::Separator();
		ImGui::TextUnformatted("Object Material");
		if (gMaterials.empty()) {
			ImGui::TextUnformatted("No materials loaded.");
		} else {
			const int maxIndex = static_cast<int>(gMaterials.size()) - 1;
			gSelectedMaterialIndex = std::clamp(gSelectedMaterialIndex, 0, maxIndex);
			std::vector<std::string> materialLabels;
			materialLabels.reserve(gMaterials.size());
			for (size_t i = 0; i < gMaterials.size(); ++i) {
				const std::string& name = gMaterials[i].name;
				if (name.empty()) {
					materialLabels.push_back("Material " + std::to_string(i));
				} else {
					materialLabels.push_back(name);
				}
			}
			std::vector<const char*> materialItems;
			materialItems.reserve(materialLabels.size());
			for (const std::string& label : materialLabels) {
				materialItems.push_back(label.c_str());
			}
			ImGui::Combo("Material", &gSelectedMaterialIndex, materialItems.data(), static_cast<int>(materialItems.size()));
			Material& mat = gMaterials[gSelectedMaterialIndex];
			ImGui::ColorEdit3("Ambient", glm::value_ptr(mat.ambient));
			ImGui::ColorEdit3("Diffuse", glm::value_ptr(mat.diffuse));
			ImGui::ColorEdit3("Specular", glm::value_ptr(mat.specular));
			ImGui::SliderFloat("Shininess", &mat.shininess, 1.0f, 256.0f, "%.1f");
			ImGui::Text("Diffuse Texture: %s", mat.hasDiffuseTexture ? "Yes" : "No");
			ImGui::Text("Specular Texture: %s", mat.hasSpecularTexture ? "Yes" : "No");
		}

		ImGui::Separator();
		ImGui::TextUnformatted("Plane Material");
		ImGui::ColorEdit3("Plane Ambient", glm::value_ptr(gPlaneAmbientColor));
		ImGui::ColorEdit3("Plane Diffuse", glm::value_ptr(gPlaneDiffuseColor));
		ImGui::ColorEdit3("Plane Specular", glm::value_ptr(gPlaneSpecularColor));
		ImGui::SliderFloat("Plane Shininess", &gPlaneShininess, 1.0f, 256.0f, "%.1f");
		ImGui::ColorEdit3("Plane Color Bias", glm::value_ptr(gPlaneColorBias));
		ImGui::SliderFloat("Plane Env Reflection", &gPlaneEnvReflectionStrength, 0.0f, 2.5f, "%.2f");
		ImGui::SliderFloat("Plane RTT Reflection", &gPlaneRttReflectionStrength, 0.0f, 2.5f, "%.2f");
		ImGui::SliderFloat("Plane Reflection Brightness", &gPlaneRttReflectionBrightness, 0.0f, 2.5f, "%.2f");
		gPlaneShininess = std::clamp(gPlaneShininess, 1.0f, 512.0f);
		gPlaneAmbientColor = glm::clamp(gPlaneAmbientColor, glm::vec3(0.0f), glm::vec3(1.0f));
		gPlaneDiffuseColor = glm::clamp(gPlaneDiffuseColor, glm::vec3(0.0f), glm::vec3(1.0f));
		gPlaneSpecularColor = glm::clamp(gPlaneSpecularColor, glm::vec3(0.0f), glm::vec3(1.0f));
		gPlaneColorBias = glm::clamp(gPlaneColorBias, glm::vec3(-0.25f), glm::vec3(0.25f));
		gPlaneEnvReflectionStrength = std::clamp(gPlaneEnvReflectionStrength, 0.0f, 5.0f);
		gPlaneRttReflectionStrength = std::clamp(gPlaneRttReflectionStrength, 0.0f, 5.0f);
		gPlaneRttReflectionBrightness = std::clamp(gPlaneRttReflectionBrightness, 0.0f, 5.0f);

		ImGui::Separator();
		ImGui::TextUnformatted("Model");
		ImGui::InputText("OBJ Path", gObjPathInput, sizeof(gObjPathInput));
		if (ImGui::Button("Load OBJ")) {
			LoadModelFromPath(std::string(gObjPathInput));
		}
		ImGui::SameLine();
		if (ImGui::Button("Refresh OBJ List")) {
			RefreshAvailableObjFiles();
		}

		if (!gAvailableObjFiles.empty()) {
			std::vector<const char*> items;
			items.reserve(gAvailableObjFiles.size());
			for (const std::string& file : gAvailableObjFiles) {
				items.push_back(file.c_str());
			}
			if (gSelectedObjIndex < 0 || gSelectedObjIndex >= static_cast<int>(gAvailableObjFiles.size())) {
				gSelectedObjIndex = 0;
			}
			ImGui::Combo("Discovered OBJs", &gSelectedObjIndex, items.data(), static_cast<int>(items.size()));
			if (ImGui::Button("Load Selected OBJ")) {
				LoadModelFromPath(gAvailableObjFiles[gSelectedObjIndex]);
			}
		} else {
			ImGui::TextUnformatted("No .obj files discovered under current directory.");
		}

		if (!gModelLoadStatus.empty()) {
			ImGui::Separator();
			ImGui::TextWrapped("%s", gModelLoadStatus.c_str());
		}

		ImGui::End();
	}

	void EndGuiFrame() {
		if (!gGuiInitialized) {
			return;
		}
		ImGui::Render();
		if (gGuiBackend == GuiBackend::OpenGL3) {
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		} else if (gGuiBackend == GuiBackend::OpenGL2) {
			ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
		}
	}

}

namespace gpurenderer {
	bool Initialize(GLFWwindow* window, const std::string& objPath) {
		gWindow = window;
		gObjPath = objPath;
		CopyObjPathToInput(gObjPath);
		RefreshAvailableObjFiles();

		if (!gWindow) {
			std::fprintf(stderr, "Initialize called with null GLFW window.\n");
			return false;
		}

		stbi_set_flip_vertically_on_load(true);
		ResolveShaderPaths();

		const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
		const char* glVendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
		if (glVersion) {
			std::printf("OpenGL version: %s\n", glVersion);
		}
		if (glVendor) {
			std::printf("OpenGL vendor: %s\n", glVendor);
		}

		if (!LoadGlFunctions()) {
			return false;
		}
		InitializeAnisotropicFiltering();
		if (gHasAnisotropicFiltering) {
			std::printf("Anisotropic filtering enabled (max %.2fx).\n", gMaxAnisotropy);
		} else {
			std::printf("Anisotropic filtering extension not available.\n");
		}

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_TEXTURE_2D);

		std::printf("Loading mesh...\n");
		std::fflush(stdout);
		if (!LoadMesh(gObjPath, gVertices, gIndices, gBounds, gSubmeshes, gMaterials)) {
			return false;
		}
		gIndexCount = static_cast<int>(gIndices.size());

		gCenter = gBounds.Center();
		const float extent = std::max(gBounds.MaxExtent(), 1.0f);
		gObjectCamera.distance = extent * 2.5f;
		gObjectCamera.yawDeg = 0.0f;
		gObjectCamera.pitchDeg = 0.0f;
		gObjectCamera.panX = 0.0f;
		gObjectCamera.panY = 0.0f;
		gPlaneHeight = (gBounds.min.y - gCenter.y) - (extent * kPlanePaddingRatio);
		gPlaneWidth = 2.0f * extent * kPlaneSizeMultiplier;
		gPlaneLength = 2.0f * extent * kPlaneSizeMultiplier;
		gPlaneScale = 1.0f;
		gObjectScale = glm::vec3(1.0f);
		gLightMarkerScale = 1.0f;
		gLightDistance = extent * 1.8f;
		gNearPlane = std::max(0.1f, extent * 0.001f);
		gFarPlane = std::max(kFarPlane, gObjectCamera.distance + extent * 3.0f);

		if (!ReloadShaders() || !ReloadDepthShader()) {
			return false;
		}

		CreateBuffers();
		CreatePlaneBuffers();
		if (!InitializeEnvironmentAssets()) {
			std::fprintf(stderr, "%s\n", gEnvironmentLoadStatus.c_str());
			return false;
		}
		CreateLightBuffers();

		if (!InitializeGui(gWindow)) {
			std::fprintf(stderr, "Failed to initialize GUI backend.\n");
			return false;
		}
		const char* guiBackend = "Unknown";
		if (gGuiBackend == GuiBackend::OpenGL3) {
			guiBackend = "OpenGL3";
		} else if (gGuiBackend == GuiBackend::OpenGL2) {
			guiBackend = "OpenGL2";
		}
		std::printf("GUI initialized (%s backend).\n", guiBackend);

		int fbWidth = 0;
		int fbHeight = 0;
		glfwGetFramebufferSize(gWindow, &fbWidth, &fbHeight);
		Reshape(gWindow, fbWidth, fbHeight);
		UpdateWindowTitle();
		CopyObjPathToInput(gObjPath);
		RefreshAvailableObjFiles();
		gModelLoadStatus = "Loaded model: " + gObjPath;

		std::printf("%s\n", gEnvironmentLoadStatus.c_str());
		std::printf("Controls: Left/right drag = object rotate/zoom, middle drag = pan, CTRL+left drag = light rotate, P = toggle projection, N = normals, F6 = reload shaders.\n");
		std::printf("Loaded %d triangles (%zu vertices) from %s\n", gIndexCount / 3, gVertices.size(), gObjPath.c_str());
		return true;
	}

	void RenderFrame() {
		BeginGuiFrame();
		DrawGuiPanel();
		Display();
		EndGuiFrame();
	}

	void Shutdown() {
		ShutdownGui();
		DestroyBackgroundBuffers();
		DestroyLightBuffers();

		if (pglDeleteBuffers) {
			if (gEbo != 0) {
				pglDeleteBuffers(1, &gEbo);
			}
			if (gVbo != 0) {
				pglDeleteBuffers(1, &gVbo);
			}
			if (gPlaneEbo != 0) {
				pglDeleteBuffers(1, &gPlaneEbo);
			}
			if (gPlaneVbo != 0) {
				pglDeleteBuffers(1, &gPlaneVbo);
			}
		}
		if (gUseVao && pglDeleteVertexArrays) {
			if (gVao != 0) {
				pglDeleteVertexArrays(1, &gVao);
			}
			if (gPlaneVao != 0) {
				pglDeleteVertexArrays(1, &gPlaneVao);
			}
		}
		if (gProgram != 0) {
			pglDeleteProgram(gProgram);
			gProgram = 0;
		}
		if (gDepthProgram != 0) {
			pglDeleteProgram(gDepthProgram);
			gDepthProgram = 0;
		}
		if (gEnvironmentCubemap != 0) {
			glDeleteTextures(1, &gEnvironmentCubemap);
			gEnvironmentCubemap = 0;
		}
		if (!gTextureCache.empty()) {
			std::vector<GLuint> textures;
			textures.reserve(gTextureCache.size());
			for (const auto& entry : gTextureCache) {
				if (entry.second != 0) {
					textures.push_back(entry.second);
				}
			}
			if (!textures.empty()) {
				glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
			}
		}
		DestroyRenderTexture();
		DestroyShadowMap();
		gTextureCache.clear();
		gWindow = nullptr;
	}

	void OnFramebufferSize(GLFWwindow* window, int width, int height) {
		Reshape(window, width, height);
	}

	void OnMouseButton(GLFWwindow* window, int button, int action, int mods) {
		MouseButton(window, button, action, mods);
	}

	void OnMouseMotion(GLFWwindow* window, double x, double y) {
		MouseMotion(window, x, y);
	}

	void OnScroll(GLFWwindow* window, double xoffset, double yoffset) {
		Scroll(window, xoffset, yoffset);
	}

	void OnKey(GLFWwindow* window, int key, int scancode, int action, int mods) {
		KeyCallback(window, key, scancode, action, mods);
	}

	void OnChar(GLFWwindow* window, unsigned int codepoint) {
		CharCallback(window, codepoint);
	}
}
