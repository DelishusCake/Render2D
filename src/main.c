#include <stdio.h>

#include <GL\gl3w.h>
#include <glfw\glfw3.h>

#include "core.h"
#include "geom.h"

#include "draw.h"
#include "assets.h"
#include "render.h"

#include "game.h"

// Ensure we're using the discrete GPU on laptops
__declspec(dllexport) DWORD NvOptimusEnablement = 0x01;
__declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x01; 

static void glfwCallbackError(int error, const char *msg)
{
	fprintf(stderr, "[GLFW] (ERROR) :: %s\n", msg);
};
int main(int argc, const char *argv[])
{
	const bool vsync = true;
	const uint32_t window_width  = ((SCREEN_W << 2) * 3) / 4;
	const uint32_t window_height = ((SCREEN_H << 2) * 3) / 4;

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
			glfwSwapInterval(vsync);

			if (gl3wInit() == 0)
			{
				printf("OpenGL %s\n", glGetString(GL_VERSION));
				printf("GLSL %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

				draw_list_t *draw_list = draw_list_alloc();
				assets_t *assets = assets_alloc();

				if (assets && draw_list && render_init())
				{
					u32 frames = 0;
					f64 timer = 0.0;

					game_init(assets);

					f64 last = glfwGetTime();
					while (!glfwWindowShouldClose(window))
					{
						const f64 now = glfwGetTime();
						const f64 delta = (now - last);
						last = now;

						game_update_and_draw(delta, assets, draw_list);

						i32 width, height;
						glfwGetFramebufferSize(window, &width, &height);

						render(width, height, draw_list);
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
					assets_free(assets);
					draw_list_free(draw_list);
					render_free();
				}
			};
		};
		glfwTerminate();
	};
	return 0;
}