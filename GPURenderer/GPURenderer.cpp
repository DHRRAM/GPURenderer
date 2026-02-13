#include <GLFW/glfw3.h>

#include "RendererApp.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
	struct AppConfig {
		std::string objPath;
		int windowWidth = gpurenderer::config::kDefaultWindowWidth;
		int windowHeight = gpurenderer::config::kDefaultWindowHeight;
	};

	AppConfig ParseArgs(int argc, char** argv) {
		if (argc < 2) {
			std::fprintf(stderr, gpurenderer::config::kUsageFormat, argv[0]);
			std::exit(1);
		}

		AppConfig config;
		config.objPath = argv[1];
		if (argc > 2) {
			const int width = std::atoi(argv[2]);
			if (width > 0) {
				config.windowWidth = width;
			}
		}
		if (argc > 3) {
			const int height = std::atoi(argv[3]);
			if (height > 0) {
				config.windowHeight = height;
			}
		}
		return config;
	}
}

int main(int argc, char** argv) {
	const AppConfig config = ParseArgs(argc, argv);

	if (!glfwInit()) {
		std::fprintf(stderr, "Failed to initialize GLFW.\n");
		return 1;
	}

	auto createWindow = [&](bool requestGl21) -> GLFWwindow* {
		glfwDefaultWindowHints();
		if (requestGl21) {
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
		}
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		return glfwCreateWindow(
			config.windowWidth,
			config.windowHeight,
			gpurenderer::config::kWindowTitleBase,
			nullptr,
			nullptr);
	};

	GLFWwindow* window = createWindow(true);
	if (!window) {
		std::fprintf(stderr, "Failed to create OpenGL 2.1 window. Retrying with default context hints...\n");
		window = createWindow(false);
	}
	if (!window) {
		std::fprintf(stderr, "Failed to create GLFW window.\n");
		glfwTerminate();
		return 1;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	if (!gpurenderer::Initialize(window, config.objPath)) {
		gpurenderer::Shutdown();
		glfwDestroyWindow(window);
		glfwTerminate();
		return 1;
	}

	glfwSetFramebufferSizeCallback(window, gpurenderer::OnFramebufferSize);
	glfwSetMouseButtonCallback(window, gpurenderer::OnMouseButton);
	glfwSetCursorPosCallback(window, gpurenderer::OnMouseMotion);
	glfwSetScrollCallback(window, gpurenderer::OnScroll);
	glfwSetKeyCallback(window, gpurenderer::OnKey);
	glfwSetCharCallback(window, gpurenderer::OnChar);

	glfwShowWindow(window);
	glfwFocusWindow(window);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		gpurenderer::RenderFrame();
		glfwSwapBuffers(window);
	}

	gpurenderer::Shutdown();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
