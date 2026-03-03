#pragma once

#include <GLFW/glfw3.h>

#include <string>

namespace gpurenderer {
	namespace config {
		inline constexpr int kDefaultWindowWidth = 1500;
		inline constexpr int kDefaultWindowHeight = 520;
		inline constexpr const char* kWindowTitleBase = "GPURenderer - Project 7";
		inline constexpr const char* kUsageFormat = "Usage: %s <model.obj> [width height]\n";
	}

	bool Initialize(GLFWwindow* window, const std::string& objPath);
	void Shutdown();
	void RenderFrame();

	void OnFramebufferSize(GLFWwindow* window, int width, int height);
	void OnMouseButton(GLFWwindow* window, int button, int action, int mods);
	void OnMouseMotion(GLFWwindow* window, double x, double y);
	void OnScroll(GLFWwindow* window, double xoffset, double yoffset);
	void OnKey(GLFWwindow* window, int key, int scancode, int action, int mods);
	void OnChar(GLFWwindow* window, unsigned int codepoint);
}
