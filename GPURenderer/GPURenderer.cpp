#include <GL/freeglut.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cmath>
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
	constexpr float kZoomSpeed = 1.0f;
	constexpr float kMinCameraDistance = 0.1f;
	constexpr float kDefaultFovDeg = 45.0f;
	constexpr float kNearPlane = 0.1f;
	constexpr float kFarPlane = 500.0f;

	struct Vec3 {
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};

	struct Bounds {
		Vec3 min;
		Vec3 max;
		bool valid = false;

		void Expand(const Vec3& v) {
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

		Vec3 Center() const {
			return Vec3{
				0.5f * (min.x + max.x),
				0.5f * (min.y + max.y),
				0.5f * (min.z + max.z)
			};
		}

		float MaxExtent() const {
			const float dx = max.x - min.x;
			const float dy = max.y - min.y;
			const float dz = max.z - min.z;
			return std::max(dx, std::max(dy, dz));
		}
	};

	struct Mat4 {
		float m[16]{};

		static Mat4 Identity() {
			Mat4 out;
			out.m[0] = 1.0f;
			out.m[5] = 1.0f;
			out.m[10] = 1.0f;
			out.m[15] = 1.0f;
			return out;
		}

		static Mat4 Translation(float x, float y, float z) {
			Mat4 out = Identity();
			out.m[12] = x;
			out.m[13] = y;
			out.m[14] = z;
			return out;
		}

		static Mat4 Scale(float x, float y, float z) {
			Mat4 out{};
			out.m[0] = x;
			out.m[5] = y;
			out.m[10] = z;
			out.m[15] = 1.0f;
			return out;
		}

		static Mat4 RotationX(float radians) {
			Mat4 out = Identity();
			const float c = std::cos(radians);
			const float s = std::sin(radians);
			out.m[5] = c;
			out.m[6] = s;
			out.m[9] = -s;
			out.m[10] = c;
			return out;
		}

		static Mat4 RotationY(float radians) {
			Mat4 out = Identity();
			const float c = std::cos(radians);
			const float s = std::sin(radians);
			out.m[0] = c;
			out.m[2] = -s;
			out.m[8] = s;
			out.m[10] = c;
			return out;
		}

		static Mat4 Perspective(float fovYRadians, float aspect, float zNear, float zFar) {
			Mat4 out{};
			const float f = 1.0f / std::tan(0.5f * fovYRadians);
			out.m[0] = f / aspect;
			out.m[5] = f;
			out.m[10] = (zFar + zNear) / (zNear - zFar);
			out.m[11] = -1.0f;
			out.m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
			return out;
		}

		static Mat4 Ortho(float left, float right, float bottom, float top, float zNear, float zFar) {
			Mat4 out{};
			out.m[0] = 2.0f / (right - left);
			out.m[5] = 2.0f / (top - bottom);
			out.m[10] = -2.0f / (zFar - zNear);
			out.m[12] = -(right + left) / (right - left);
			out.m[13] = -(top + bottom) / (top - bottom);
			out.m[14] = -(zFar + zNear) / (zFar - zNear);
			out.m[15] = 1.0f;
			return out;
		}

		Mat4 operator*(const Mat4& rhs) const {
			Mat4 out{};
			for (int col = 0; col < 4; ++col) {
				for (int row = 0; row < 4; ++row) {
					float sum = 0.0f;
					for (int k = 0; k < 4; ++k) {
						sum += m[k * 4 + row] * rhs.m[col * 4 + k];
					}
					out.m[col * 4 + row] = sum;
				}
			}
			return out;
		}
	};

	const char* kFallbackVertexShader = R"(#version 120

attribute vec3 aPosition;
uniform mat4 uMvp;

void main() {
	gl_Position = uMvp * vec4(aPosition, 1.0);
}
)";

const char* kFallbackFragmentShader = R"(#version 120

void main() {
	gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
)";

	int gWindowWidth = kDefaultWindowWidth;
	int gWindowHeight = kDefaultWindowHeight;
	bool gUsePerspective = true;
	bool gLeftDown = false;
	bool gRightDown = false;
	bool gMiddleDown = false;
	int gLastMouseX = 0;
	int gLastMouseY = 0;
	float gYawDeg = 0.0f;
	float gPitchDeg = 0.0f;
	float gCameraDistance = 4.0f;
	float gPanX = 0.0f;
	float gPanY = 0.0f;

	std::string gObjPath;
	std::string gVertexShaderPath;
	std::string gFragmentShaderPath;

	std::vector<float> gVertexData;
	int gVertexCount = 0;
	Bounds gBounds;
	Vec3 gCenter;

	GLuint gVao = 0;
	GLuint gVbo = 0;
	GLuint gProgram = 0;
	GLint gMvpLocation = -1;

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

	template <typename T>
	T Clamp(T value, T minValue, T maxValue) {
		return std::max(minValue, std::min(value, maxValue));
	}

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
		func = reinterpret_cast<T>(glutGetProcAddress(name));
		if (!func && fallback) {
			func = reinterpret_cast<T>(glutGetProcAddress(fallback));
		}
		if (!func) {
			std::fprintf(stderr, "Missing OpenGL function: %s\n", name);
			return false;
		}
		return true;
	}

	bool LoadGlFunctions() {
		bool ok = true;
		ok &= LoadGlFunction(pglGenVertexArrays, "glGenVertexArrays", "glGenVertexArraysARB");
		ok &= LoadGlFunction(pglBindVertexArray, "glBindVertexArray", "glBindVertexArrayARB");
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
		return ok;
	}

	bool LoadObjVertices(const std::string& path, std::vector<float>& vertices, Bounds& bounds) {
		std::ifstream file(path);
		if (!file) {
			std::fprintf(stderr, "Failed to open OBJ file: %s\n", path.c_str());
			return false;
		}

		std::string line;
		while (std::getline(file, line)) {
			if (line.empty()) {
				continue;
			}
			if (line[0] != 'v' || (line.size() > 1 && !std::isspace(static_cast<unsigned char>(line[1])))) {
				continue;
			}
			std::istringstream stream(line);
			char prefix = 0;
			stream >> prefix;
			if (prefix != 'v') {
				continue;
			}
			Vec3 v;
			if (!(stream >> v.x >> v.y >> v.z)) {
				continue;
			}
			vertices.push_back(v.x);
			vertices.push_back(v.y);
			vertices.push_back(v.z);
			bounds.Expand(v);
		}

		if (!bounds.valid || vertices.empty()) {
			std::fprintf(stderr, "OBJ file has no vertex positions: %s\n", path.c_str());
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
		return true;
	}

	void CreateBuffers() {
		pglGenVertexArrays(1, &gVao);
		pglBindVertexArray(gVao);

		pglGenBuffers(1, &gVbo);
		pglBindBuffer(GL_ARRAY_BUFFER, gVbo);
		pglBufferData(GL_ARRAY_BUFFER,
			static_cast<std::ptrdiff_t>(gVertexData.size() * sizeof(float)),
			gVertexData.data(),
			GL_STATIC_DRAW);

		pglEnableVertexAttribArray(0);
		pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

		pglBindVertexArray(0);
	}

	Mat4 BuildMvp() {
		const float aspect = (gWindowHeight > 0) ? static_cast<float>(gWindowWidth) /
			static_cast<float>(gWindowHeight) : 1.0f;

		Mat4 model = Mat4::Translation(-gCenter.x, -gCenter.y, -gCenter.z);

		if (!gUsePerspective) {
			const float scale = 1.0f / gCameraDistance;
			model = Mat4::Scale(scale, scale, scale) * model;
		}

		const float yaw = DegreesToRadians(gYawDeg);
		const float pitch = DegreesToRadians(gPitchDeg);
		Mat4 view = Mat4::Translation(gPanX, gPanY, 0.0f) *
			Mat4::Translation(0.0f, 0.0f, -gCameraDistance) *
			Mat4::RotationX(pitch) * Mat4::RotationY(yaw);

		Mat4 projection;
		if (gUsePerspective) {
			projection = Mat4::Perspective(DegreesToRadians(kDefaultFovDeg), aspect, kNearPlane, kFarPlane);
		} else {
			const float size = 1.0f;
			projection = Mat4::Ortho(-size * aspect, size * aspect, -size, size, -kFarPlane, kFarPlane);
		}

		return projection * view * model;
	}

	void UpdateWindowTitle() {
		const char* mode = gUsePerspective ? "Perspective" : "Orthogonal";
		char title[128];
		std::snprintf(title, sizeof(title), "GPURenderer - Project 2 | %s", mode);
		glutSetWindowTitle(title);
	}

	void Display() {
		glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (gProgram == 0 || gVao == 0) {
			glutSwapBuffers();
			return;
		}

		UpdateWindowTitle();

		pglUseProgram(gProgram);
		if (gMvpLocation >= 0) {
			const Mat4 mvp = BuildMvp();
			pglUniformMatrix4fv(gMvpLocation, 1, GL_FALSE, mvp.m);
		}

		pglBindVertexArray(gVao);
		glDrawArrays(GL_POINTS, 0, gVertexCount);
		pglBindVertexArray(0);
		pglUseProgram(0);

		glutSwapBuffers();
	}

	void Reshape(int width, int height) {
		gWindowWidth = width > 0 ? width : 1;
		gWindowHeight = height > 0 ? height : 1;
		glViewport(0, 0, gWindowWidth, gWindowHeight);
	}

	void MouseButton(int button, int state, int x, int y) {
		if (button == GLUT_LEFT_BUTTON) {
			gLeftDown = (state == GLUT_DOWN);
		} else if (button == GLUT_RIGHT_BUTTON) {
			gRightDown = (state == GLUT_DOWN);
		} else if (button == GLUT_MIDDLE_BUTTON) {
			gMiddleDown = (state == GLUT_DOWN);
		}
		gLastMouseX = x;
		gLastMouseY = y;
	}

	void MouseMotion(int x, int y) {
		const int dx = x - gLastMouseX;
		const int dy = y - gLastMouseY;
		const float viewHeight = gUsePerspective
			? (2.0f * gCameraDistance * std::tan(0.5f * DegreesToRadians(kDefaultFovDeg)))
			: 2.0f;
		const float panScale = (gWindowHeight > 0) ? (viewHeight / static_cast<float>(gWindowHeight)) : 0.0f;

		if (gLeftDown) {
			gYawDeg += static_cast<float>(dx) * kRotationSpeedDegPerPixel;
			gPitchDeg += static_cast<float>(dy) * kRotationSpeedDegPerPixel;
			gPitchDeg = Clamp(gPitchDeg, -89.0f, 89.0f);
		}
		if (gRightDown) {
			gCameraDistance += static_cast<float>(dy) * kZoomSpeed;
			gCameraDistance = std::max(gCameraDistance, kMinCameraDistance);
		}
		if (gMiddleDown) {
			gPanX += static_cast<float>(dx) * panScale;
			gPanY -= static_cast<float>(dy) * panScale;
		}

		gLastMouseX = x;
		gLastMouseY = y;
		glutPostRedisplay();
	}

	void MouseWheel(int, int direction, int, int) {
		if (direction != 0) {
			gCameraDistance -= static_cast<float>(direction) * 0.1f;
			gCameraDistance = std::max(gCameraDistance, kMinCameraDistance);
			glutPostRedisplay();
		}
	}

	void Keyboard(unsigned char key, int, int) {
		if (key == 27) {
			glutLeaveMainLoop();
			return;
		}
		if (key == 'p' || key == 'P') {
			gUsePerspective = !gUsePerspective;
			UpdateWindowTitle();
			glutPostRedisplay();
			return;
		}
		if (key == 'r' || key == 'R') {
			gYawDeg = 0.0f;
			gPitchDeg = 0.0f;
			gPanX = 0.0f;
			gPanY = 0.0f;
			glutPostRedisplay();
		}
	}

	void SpecialKey(int key, int, int) {
		if (key == GLUT_KEY_F6) {
			if (!ReloadShaders()) {
				std::fprintf(stderr, "F6 shader reload failed.\n");
			} else {
				std::fprintf(stdout, "Shaders reloaded.\n");
				glutPostRedisplay();
			}
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
	if (!LoadObjVertices(gObjPath, gVertexData, gBounds)) {
		return 1;
	}
	gVertexCount = static_cast<int>(gVertexData.size() / 3);

	gCenter = gBounds.Center();
	const float extent = std::max(gBounds.MaxExtent(), 1.0f);
	gCameraDistance = extent * 2.5f;

	ResolveShaderPaths();

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	glutInitWindowSize(gWindowWidth, gWindowHeight);
	glutCreateWindow("GPURenderer - Project 2");

	if (!LoadGlFunctions()) {
		return 1;
	}

	glEnable(GL_DEPTH_TEST);
	glPointSize(2.0f);

	if (!ReloadShaders()) {
		return 1;
	}
	CreateBuffers();

	glutDisplayFunc(Display);
	glutReshapeFunc(Reshape);
	glutKeyboardFunc(Keyboard);
	glutSpecialFunc(SpecialKey);
	glutMouseFunc(MouseButton);
	glutMotionFunc(MouseMotion);
	glutMouseWheelFunc(MouseWheel);

	std::printf("Controls: Left drag = rotate, middle drag = pan, right drag/wheel = zoom, P = toggle projection, F6 = reload shaders.\n");
	std::printf("Loaded %d vertices from %s\n", gVertexCount, gObjPath.c_str());

	glutMainLoop();
	return 0;
}
