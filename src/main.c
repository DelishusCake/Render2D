#include <stdio.h>

#include <GL\gl3w.h>
#include <glfw\glfw3.h>

#include "core.h"
#include "geom.h"

#include "assets.h"
#include "render.h"

// Ensure we're using the discrete GPU on laptops
__declspec(dllexport) DWORD NvOptimusEnablement = 0x01;
__declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x01; 

static void glfwCallbackError(int error, const char *msg)
{
	fprintf(stderr, "[GLFW] (ERROR) :: %s\n", msg);
};
int main(int argc, const char *argv[])
{
	const uint32_t window_width  = (1920 * 3) / 4;
	const uint32_t window_height = (1080 * 3) / 4;

	glfwSetErrorCallback(glfwCallbackError);
	if (glfwInit())
	{
		glfwWindowHint(GLFW_DOUBLEBUFFER, true);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		GLFWwindow *window = glfwCreateWindow(window_width, window_height, "Game", NULL, NULL);
		if (window)
		{
			glfwMakeContextCurrent(window);
			if (gl3wInit() == 0)
			{
				printf("OpenGL %s\n", glGetString(GL_VERSION));
				printf("GLSL %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

				asset_cache_t *asset_cache = asset_cache_alloc();

				if (asset_cache && render_init())
				{
					u32 frames = 0;
					f64 timer = 0.0;

					f64 last = glfwGetTime();
					while (!glfwWindowShouldClose(window))
					{
						const f64 now = glfwGetTime();
						const f64 delta = (now - last);
						last = now;

						i32 width, height;
						glfwGetFramebufferSize(window, &width, &height);

						render(width, height);
						glfwSwapBuffers(window);

						frames ++;
						timer += delta;
						if (timer >= 1.0)
						{
							char buf[128];
							sprintf(buf, "Game - %dfps", frames);
							glfwSetWindowTitle(window, buf);

							timer -= 1.0;
							frames = 0;
						}
						glfwPollEvents();
					};
				}

			};
		};
		glfwTerminate();
	};
	return 0;
}