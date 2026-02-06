#include <GLFW/glfw3.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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
	};

	const char* kFallbackVertexShader = R"(#version 120

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
)";

	const char* kFallbackFragmentShader = R"(#version 120

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

	std::string gObjPath;
	std::string gVertexShaderPath;
	std::string gFragmentShaderPath;

	std::vector<Vertex> gVertices;
	std::vector<unsigned int> gIndices;
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
		LoadOptionalGlFunction(pglDeleteBuffers, "glDeleteBuffers");
		LoadOptionalGlFunction(pglDeleteVertexArrays, "glDeleteVertexArrays", "glDeleteVertexArraysARB");
		return ok;
	}

	bool LoadMesh(const std::string& path, std::vector<Vertex>& vertices, std::vector<unsigned int>& indices, Bounds& bounds) {
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(
			path,
			aiProcess_Triangulate |
			aiProcess_GenSmoothNormals |
			aiProcess_JoinIdenticalVertices |
			aiProcess_ImproveCacheLocality |
			aiProcess_PreTransformVertices);

		if (!scene || !scene->HasMeshes()) {
			std::fprintf(stderr, "Assimp failed to load mesh: %s\n", importer.GetErrorString());
			return false;
		}

		vertices.clear();
		indices.clear();
		bounds = Bounds{};

		for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
			const aiMesh* mesh = scene->mMeshes[meshIndex];
			if (!mesh || mesh->mNumVertices == 0) {
				continue;
			}

			const unsigned int baseIndex = static_cast<unsigned int>(vertices.size());
			vertices.reserve(vertices.size() + mesh->mNumVertices);

			for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
				const aiVector3D pos = mesh->mVertices[i];
				aiVector3D normal(0.0f, 1.0f, 0.0f);
				if (mesh->HasNormals()) {
					normal = mesh->mNormals[i];
				}
				Vertex vertex;
				vertex.position = glm::vec3(pos.x, pos.y, pos.z);
				vertex.normal = glm::vec3(normal.x, normal.y, normal.z);
				vertices.push_back(vertex);
				bounds.Expand(vertex.position);
			}

			for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
				const aiFace& face = mesh->mFaces[i];
				if (face.mNumIndices != 3) {
					continue;
				}
				indices.push_back(baseIndex + face.mIndices[0]);
				indices.push_back(baseIndex + face.mIndices[1]);
				indices.push_back(baseIndex + face.mIndices[2]);
			}
		}

		if (!bounds.valid || vertices.empty() || indices.empty()) {
			std::fprintf(stderr, "Mesh contained no valid triangles: %s\n", path.c_str());
			return false;
		}

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

		if (gUseVao) {
			pglBindVertexArray(0);
		}
	}

	void CreateLightBuffers() {
		const Vertex lightVertex{
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 1.0f, 0.0f)
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
			return glm::perspective(glm::radians(kDefaultFovDeg), aspect, kNearPlane, kFarPlane);
		}

		const float size = 1.0f;
		return glm::ortho(-size * aspect, size * aspect, -size, size, -kFarPlane, kFarPlane);
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
		std::snprintf(title, sizeof(title), "GPURenderer - Project 3 | %s | %s", mode, shade);
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
		const glm::vec3 ambient(0.15f, 0.15f, 0.18f);
		const glm::vec3 diffuse(0.8f, 0.7f, 0.65f);
		const glm::vec3 specular(0.9f, 0.9f, 0.9f);
		const float shininess = 64.0f;

		if (gLightPosLocation >= 0) {
			pglUniform3fv(gLightPosLocation, 1, glm::value_ptr(lightPosView));
		}
		if (gLightColorLocation >= 0) {
			pglUniform3fv(gLightColorLocation, 1, glm::value_ptr(lightColor));
		}
		if (gAmbientLocation >= 0) {
			pglUniform3fv(gAmbientLocation, 1, glm::value_ptr(ambient));
		}
		if (gDiffuseLocation >= 0) {
			pglUniform3fv(gDiffuseLocation, 1, glm::value_ptr(diffuse));
		}
		if (gSpecularLocation >= 0) {
			pglUniform3fv(gSpecularLocation, 1, glm::value_ptr(specular));
		}
		if (gShininessLocation >= 0) {
			pglUniform1f(gShininessLocation, shininess);
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
		}

		glDrawElements(GL_TRIANGLES, gIndexCount, GL_UNSIGNED_INT, nullptr);

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
	if (!LoadMesh(gObjPath, gVertices, gIndices, gBounds)) {
		return 1;
	}
	gIndexCount = static_cast<int>(gIndices.size());

	gCenter = gBounds.Center();
	const float extent = std::max(gBounds.MaxExtent(), 1.0f);
	gCameraDistance = extent * 2.5f;
	gLightDistance = extent * 3.0f;

	ResolveShaderPaths();

	if (!glfwInit()) {
		std::fprintf(stderr, "Failed to initialize GLFW.\n");
		return 1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);

	gWindow = glfwCreateWindow(gWindowWidth, gWindowHeight, "GPURenderer - Project 3", nullptr, nullptr);
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

	glfwDestroyWindow(gWindow);
	glfwTerminate();
	return 0;
}
