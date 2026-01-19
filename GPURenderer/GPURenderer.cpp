#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <GL/freeglut.h>

namespace {
	constexpr int kWindowWidth = 800;
	constexpr int kWindowHeight = 600;
	constexpr float kAnimationSpeed = 1.0f;
	constexpr unsigned int kTargetFrameMs = 16;

	int gWindowWidth = kWindowWidth;
	int gWindowHeight = kWindowHeight;
	float gAnimationSpeed = kAnimationSpeed;
	bool gPaused = false;
	int gPauseStartMs = 0;
	int gAccumPausedMs = 0;

	void UpdateWindowTitle(float r, float g, float b) {
		char title[128];
		std::snprintf(title, sizeof(title),
			"GPURenderer - Hello World | RGB: %.2f %.2f %.2f%s",
			r, g, b, gPaused ? " [Paused]" : "");
		glutSetWindowTitle(title);
	}

	void ResetAnimation() {
		const int elapsedMs = glutGet(GLUT_ELAPSED_TIME);
		if (gPaused) {
			gAccumPausedMs = gPauseStartMs;
		} else {
			gAccumPausedMs = elapsedMs;
		}
	}

	void Display() {
		const int elapsedMs = glutGet(GLUT_ELAPSED_TIME);
		int activeMs = elapsedMs - gAccumPausedMs;
		if (gPaused) {
			activeMs = gPauseStartMs - gAccumPausedMs;
		}

		const float t = 0.001f * gAnimationSpeed * static_cast<float>(activeMs);
		const float r = 0.5f + 0.5f * static_cast<float>(std::sin(t * 0.9f + 0.0f));
		const float g = 0.5f + 0.5f * static_cast<float>(std::sin(t * 1.1f + 2.0f));
		const float b = 0.5f + 0.5f * static_cast<float>(std::sin(t * 1.3f + 4.0f));

		glClearColor(r, g, b, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		UpdateWindowTitle(r, g, b);
		glutSwapBuffers();
	}

	void Reshape(int width, int height) {
		if (height == 0) {
			height = 1;
		}
		glViewport(0, 0, width, height);
	}

	void Keyboard(unsigned char key, int, int) {
		if (key == 27) { // ESC key
			glutLeaveMainLoop();
			return;
		}

		if (key == 'p' || key == 'P') {
			const int elapsedMs = glutGet(GLUT_ELAPSED_TIME);
			if (gPaused) {
				gAccumPausedMs += (elapsedMs - gPauseStartMs);
				gPaused = false;
			} else {
				gPauseStartMs = elapsedMs;
				gPaused = true;
			}
			glutPostRedisplay();
			return;
		}

		if (key == 'r' || key == 'R') {
			ResetAnimation();
			glutPostRedisplay();
		}
	}

	void Timer(int) {
		glutPostRedisplay();
		glutTimerFunc(kTargetFrameMs, Timer, 0);
	}

	void ParseArgs(int argc, char** argv) {
		if (argc > 1) {
			const int width = std::atoi(argv[1]);
			if (width > 0) {
				gWindowWidth = width;
			}
		}

		if (argc > 2) {
			const int height = std::atoi(argv[2]);
			if (height > 0) {
				gWindowHeight = height;
			}
		}

		if (argc > 3) {
			const float speed = static_cast<float>(std::atof(argv[3]));
			if (speed > 0.0f) {
				gAnimationSpeed = speed;
			}
		}
	}
}

int main(int argc, char** argv) {
	ParseArgs(argc, argv);

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowSize(gWindowWidth, gWindowHeight);
	glutCreateWindow("GPURenderer - Hello World");

	glutDisplayFunc(Display);
	glutReshapeFunc(Reshape);
	glutKeyboardFunc(Keyboard);
	glutTimerFunc(kTargetFrameMs, Timer, 0);

	std::printf("Controls: ESC to quit, P to pause/resume, R to reset animation.\n");
	std::printf("Args: [width height speed] (speed > 0). Using %d x %d at %.2fx.\n",
		gWindowWidth, gWindowHeight, gAnimationSpeed);

	glutMainLoop();
	return 0;
}
