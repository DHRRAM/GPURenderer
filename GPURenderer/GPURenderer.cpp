#include <GLFW/glfw3.h>

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
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
#ifndef GL_LUMINANCE
#define GL_LUMINANCE 0x1909
#endif
#ifndef GL_LUMINANCE_ALPHA
#define GL_LUMINANCE_ALPHA 0x190A
#endif

namespace {
	constexpr int kDefaultWindowWidth = 800;
	constexpr int kDefaultWindowHeight = 600;
	constexpr float kRotationSpeedDegPerPixel = 0.5f;
	constexpr float kLightRotationSpeedDegPerPixel = 0.6f;
	constexpr float kZoomSpeed = 1.0f;
	constexpr float kMinCameraDistance = 0.1f;
	constexpr float kDefaultFovDeg = 45.0f;
	constexpr float kNearPlane = 0.1f;
	constexpr float kFarPlane = 500.0f;
	constexpr float kLightMarkerPointSize = 8.0f;

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

	int gWindowWidth = kDefaultWindowWidth;
	int gWindowHeight = kDefaultWindowHeight;
	bool gUsePerspective = true;
	bool gLeftDown = false;
	bool gRightDown = false;
	bool gMiddleDown = false;
	double gLastMouseX = 0.0;
	double gLastMouseY = 0.0;
	float gYawDeg = 0.0f;
	float gPitchDeg = 0.0f;
	float gCameraDistance = 4.0f;
	float gPanX = 0.0f;
	float gPanY = 0.0f;
	float gLightYawDeg = 45.0f;
	float gLightPitchDeg = 20.0f;
	float gLightDistance = 6.0f;
	bool gShowNormals = false;
	float gNearPlane = kNearPlane;
	float gFarPlane = kFarPlane;

	std::string gObjPath;
	std::string gVertexShaderPath;
	std::string gFragmentShaderPath;

	std::vector<Vertex> gVertices;
	std::vector<unsigned int> gIndices;
	std::vector<Submesh> gSubmeshes;
	std::vector<Material> gMaterials;
	std::unordered_map<std::string, GLuint> gTextureCache;
	int gIndexCount = 0;
	Bounds gBounds;
	glm::vec3 gCenter(0.0f);

	GLFWwindow* gWindow = nullptr;
	GLuint gVao = 0;
	GLuint gVbo = 0;
	GLuint gEbo = 0;
	GLuint gLightVao = 0;
	GLuint gLightVbo = 0;
	GLuint gProgram = 0;
	GLint gMvpLocation = -1;
	GLint gModelLocation = -1;
	GLint gViewLocation = -1;
	GLint gNormalMatrixLocation = -1;
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
		LoadOptionalGlFunction(pglDeleteBuffers, "glDeleteBuffers");
		LoadOptionalGlFunction(pglDeleteVertexArrays, "glDeleteVertexArrays", "glDeleteVertexArraysARB");
		return ok;
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
		} else {
			materials.resize(scene->mNumMaterials);
			for (unsigned int matIndex = 0; matIndex < scene->mNumMaterials; ++matIndex) {
				const aiMaterial* material = scene->mMaterials[matIndex];
				if (!material) {
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

				Material mat;
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

		pglUseProgram(gProgram);
		if (gDiffuseMapLocation >= 0) {
			pglUniform1i(gDiffuseMapLocation, 0);
		}
		if (gSpecularMapLocation >= 0) {
			pglUniform1i(gSpecularMapLocation, 1);
		}
		pglUseProgram(0);
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

	void CreateLightBuffers() {
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
	}

	glm::mat4 BuildModelMatrix() {
		glm::mat4 model(1.0f);
		model = glm::translate(model, -gCenter);
		if (!gUsePerspective) {
			const float scale = 1.0f / gCameraDistance;
			model = glm::scale(glm::mat4(1.0f), glm::vec3(scale)) * model;
		}
		return model;
	}

	glm::mat4 BuildViewMatrix() {
		glm::mat4 view(1.0f);
		view = glm::translate(view, glm::vec3(gPanX, gPanY, 0.0f));
		view = glm::translate(view, glm::vec3(0.0f, 0.0f, -gCameraDistance));
		view = glm::rotate(view, glm::radians(gPitchDeg), glm::vec3(1.0f, 0.0f, 0.0f));
		view = glm::rotate(view, glm::radians(gYawDeg), glm::vec3(0.0f, 1.0f, 0.0f));
		return view;
	}

	glm::mat4 BuildProjectionMatrix() {
		const float aspect = (gWindowHeight > 0)
			? static_cast<float>(gWindowWidth) / static_cast<float>(gWindowHeight)
			: 1.0f;
		if (gUsePerspective) {
			return glm::perspective(glm::radians(kDefaultFovDeg), aspect, gNearPlane, gFarPlane);
		}

		const float size = 1.0f;
		return glm::ortho(-size * aspect, size * aspect, -size, size, -gFarPlane, gFarPlane);
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

	void UpdateWindowTitle() {
		if (!gWindow) {
			return;
		}
		const char* mode = gUsePerspective ? "Perspective" : "Orthographic";
		const char* shade = gShowNormals ? "Normals" : "Blinn";
		char title[160];
		std::snprintf(title, sizeof(title), "GPURenderer - Project 4 | %s | %s", mode, shade);
		glfwSetWindowTitle(gWindow, title);
	}

	void Display() {
		glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (gProgram == 0 || gVbo == 0 || gEbo == 0) {
			return;
		}

		pglUseProgram(gProgram);

		const glm::mat4 model = BuildModelMatrix();
		const glm::mat4 view = BuildViewMatrix();
		const glm::mat4 projection = BuildProjectionMatrix();
		const glm::mat4 mvp = projection * view * model;
		const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(view * model)));

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

		const glm::vec3 lightPosWorld = ComputeLightPosition(model);
		const glm::vec3 lightPosView = glm::vec3(view * glm::vec4(lightPosWorld, 1.0f));
		const glm::vec3 lightColor(1.0f, 1.0f, 1.0f);

		if (gLightPosLocation >= 0) {
			pglUniform3fv(gLightPosLocation, 1, glm::value_ptr(lightPosView));
		}
		if (gLightColorLocation >= 0) {
			pglUniform3fv(gLightColorLocation, 1, glm::value_ptr(lightColor));
		}
		if (gShadeModeLocation >= 0) {
			pglUniform1i(gShadeModeLocation, gShowNormals ? 1 : 0);
		}

		if (gUseVao && gVao != 0) {
			pglBindVertexArray(gVao);
		} else {
			pglBindBuffer(GL_ARRAY_BUFFER, gVbo);
			pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gEbo);
			pglEnableVertexAttribArray(0);
			pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
			pglEnableVertexAttribArray(1);
			pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
			pglEnableVertexAttribArray(2);
			pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texcoord)));
		}

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

		if (gUseVao && gVao != 0) {
			pglBindVertexArray(0);
		}

		if (gLightVbo != 0) {
			const glm::mat4 modelLight = glm::translate(glm::mat4(1.0f), lightPosWorld);
			const glm::mat4 mvpLight = projection * view * modelLight;
			const glm::mat3 normalMatrixLight = glm::transpose(glm::inverse(glm::mat3(view * modelLight)));

			if (gMvpLocation >= 0) {
				pglUniformMatrix4fv(gMvpLocation, 1, GL_FALSE, glm::value_ptr(mvpLight));
			}
			if (gModelLocation >= 0) {
				pglUniformMatrix4fv(gModelLocation, 1, GL_FALSE, glm::value_ptr(modelLight));
			}
			if (gNormalMatrixLocation >= 0) {
				pglUniformMatrix3fv(gNormalMatrixLocation, 1, GL_FALSE, glm::value_ptr(normalMatrixLight));
			}
			if (gShadeModeLocation >= 0) {
				pglUniform1i(gShadeModeLocation, 2);
			}
			if (gMarkerColorLocation >= 0) {
				const glm::vec3 markerColor(1.0f, 0.9f, 0.1f);
				pglUniform3fv(gMarkerColorLocation, 1, glm::value_ptr(markerColor));
			}

			glPointSize(kLightMarkerPointSize);
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

		pglUseProgram(0);
	}

	void Reshape(GLFWwindow*, int width, int height) {
		gWindowWidth = width > 0 ? width : 1;
		gWindowHeight = height > 0 ? height : 1;
		glViewport(0, 0, gWindowWidth, gWindowHeight);
	}

	bool IsCtrlDown(GLFWwindow* window) {
		return glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
			glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
	}

	void MouseButton(GLFWwindow* window, int button, int action, int) {
		if (button == GLFW_MOUSE_BUTTON_LEFT) {
			gLeftDown = (action == GLFW_PRESS);
		} else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
			gRightDown = (action == GLFW_PRESS);
		} else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
			gMiddleDown = (action == GLFW_PRESS);
		}
		glfwGetCursorPos(window, &gLastMouseX, &gLastMouseY);
	}

	void MouseMotion(GLFWwindow* window, double x, double y) {
		const float dx = static_cast<float>(x - gLastMouseX);
		const float dy = static_cast<float>(y - gLastMouseY);
		const float viewHeight = gUsePerspective
			? (2.0f * gCameraDistance * std::tan(0.5f * DegreesToRadians(kDefaultFovDeg)))
			: 2.0f;
		const float panScale = (gWindowHeight > 0) ? (viewHeight / static_cast<float>(gWindowHeight)) : 0.0f;

		if (gLeftDown) {
			if (IsCtrlDown(window)) {
				gLightYawDeg += dx * kLightRotationSpeedDegPerPixel;
				gLightPitchDeg += dy * kLightRotationSpeedDegPerPixel;
				gLightPitchDeg = std::clamp(gLightPitchDeg, -89.0f, 89.0f);
			} else {
				gYawDeg += dx * kRotationSpeedDegPerPixel;
				gPitchDeg += dy * kRotationSpeedDegPerPixel;
				gPitchDeg = std::clamp(gPitchDeg, -89.0f, 89.0f);
			}
		}
		if (gRightDown) {
			gCameraDistance += dy * kZoomSpeed;
			gCameraDistance = std::max(gCameraDistance, kMinCameraDistance);
		}
		if (gMiddleDown) {
			gPanX += dx * panScale;
			gPanY -= dy * panScale;
		}

		gLastMouseX = x;
		gLastMouseY = y;
	}

	void Scroll(GLFWwindow*, double, double yoffset) {
		if (yoffset != 0.0) {
			gCameraDistance -= static_cast<float>(yoffset) * 0.2f;
			gCameraDistance = std::max(gCameraDistance, kMinCameraDistance);
		}
	}

	void KeyCallback(GLFWwindow* window, int key, int, int action, int) {
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
			gYawDeg = 0.0f;
			gPitchDeg = 0.0f;
			gPanX = 0.0f;
			gPanY = 0.0f;
			return;
		}
		if (key == GLFW_KEY_N) {
			gShowNormals = !gShowNormals;
			UpdateWindowTitle();
			return;
		}
		if (key == GLFW_KEY_F6) {
			if (!ReloadShaders()) {
				std::fprintf(stderr, "F6 shader reload failed.\n");
			} else {
				std::fprintf(stdout, "Shaders reloaded.\n");
			}
			return;
		}
	}

	void ParseArgs(int argc, char** argv) {
		if (argc < 2) {
			std::fprintf(stderr, "Usage: %s <model.obj> [width height]\n", argv[0]);
			std::exit(1);
		}
		gObjPath = argv[1];

		if (argc > 2) {
			const int width = std::atoi(argv[2]);
			if (width > 0) {
				gWindowWidth = width;
			}
		}
		if (argc > 3) {
			const int height = std::atoi(argv[3]);
			if (height > 0) {
				gWindowHeight = height;
			}
		}
	}
}

int main(int argc, char** argv) {
	ParseArgs(argc, argv);
	stbi_set_flip_vertically_on_load(true);
	ResolveShaderPaths();

	if (!glfwInit()) {
		std::fprintf(stderr, "Failed to initialize GLFW.\n");
		return 1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

	gWindow = glfwCreateWindow(gWindowWidth, gWindowHeight, "GPURenderer - Project 4", nullptr, nullptr);
	if (!gWindow) {
		std::fprintf(stderr, "Failed to create GLFW window.\n");
		glfwTerminate();
		return 1;
	}
	glfwMakeContextCurrent(gWindow);
	glfwSwapInterval(1);

	if (!LoadGlFunctions()) {
		glfwDestroyWindow(gWindow);
		glfwTerminate();
		return 1;
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);

	std::printf("Loading mesh...\n");
	std::fflush(stdout);
	if (!LoadMesh(gObjPath, gVertices, gIndices, gBounds, gSubmeshes, gMaterials)) {
		glfwDestroyWindow(gWindow);
		glfwTerminate();
		return 1;
	}
	gIndexCount = static_cast<int>(gIndices.size());

	gCenter = gBounds.Center();
	const float extent = std::max(gBounds.MaxExtent(), 1.0f);
	gCameraDistance = extent * 2.5f;
	gLightDistance = extent * 3.0f;
	gNearPlane = std::max(0.1f, extent * 0.001f);
	gFarPlane = std::max(kFarPlane, gCameraDistance + extent * 3.0f);

	if (!ReloadShaders()) {
		glfwDestroyWindow(gWindow);
		glfwTerminate();
		return 1;
	}

	CreateBuffers();
	CreateLightBuffers();

	glfwSetFramebufferSizeCallback(gWindow, Reshape);
	glfwSetMouseButtonCallback(gWindow, MouseButton);
	glfwSetCursorPosCallback(gWindow, MouseMotion);
	glfwSetScrollCallback(gWindow, Scroll);
	glfwSetKeyCallback(gWindow, KeyCallback);

	int fbWidth = 0;
	int fbHeight = 0;
	glfwGetFramebufferSize(gWindow, &fbWidth, &fbHeight);
	Reshape(gWindow, fbWidth, fbHeight);
	UpdateWindowTitle();
	glfwShowWindow(gWindow);
	glfwFocusWindow(gWindow);

	std::printf("Controls: Left drag = rotate, CTRL+left drag = light rotate, middle drag = pan, right drag/wheel = zoom, P = toggle projection, N = normals, F6 = reload shaders.\n");
	std::printf("Loaded %d triangles (%zu vertices) from %s\n", gIndexCount / 3, gVertices.size(), gObjPath.c_str());

	while (!glfwWindowShouldClose(gWindow)) {
		Display();
		glfwSwapBuffers(gWindow);
		glfwPollEvents();
	}

	if (pglDeleteBuffers) {
		if (gEbo != 0) {
			pglDeleteBuffers(1, &gEbo);
		}
		if (gVbo != 0) {
			pglDeleteBuffers(1, &gVbo);
		}
		if (gLightVbo != 0) {
			pglDeleteBuffers(1, &gLightVbo);
		}
	}
	if (gUseVao && pglDeleteVertexArrays) {
		if (gVao != 0) {
			pglDeleteVertexArrays(1, &gVao);
		}
		if (gLightVao != 0) {
			pglDeleteVertexArrays(1, &gLightVao);
		}
	}
	if (gProgram != 0) {
		pglDeleteProgram(gProgram);
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

	glfwDestroyWindow(gWindow);
	glfwTerminate();
	return 0;
}
