#include "render.h"

bool render_init()
{
	return true;
};
void render_free()
{
};

void render(u32 width, u32 height)
{
	glDisable(GL_DEPTH_TEST);

	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glClearColor(0.2f, 0.2f, 0.2f, 1.f);

	glViewport(0,0,width,height);
	glClear(GL_COLOR_BUFFER_BIT);
};

struct texture_t
{
	u32 handle;
};
texture_t* texture_alloc(u32 width, u32 height, u8 *data)
{
	u32 handle;
	glGenTextures(1, &handle);

	glBindTexture(GL_TEXTURE_2D, handle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glBindTexture(GL_TEXTURE_2D, 0);

	texture_t *texture = malloc(sizeof(texture_t));
	assert(texture != NULL);
	texture->handle = handle;
	return texture;
};
void texture_free(texture_t *texture)
{
	glDeleteTextures(1, &texture->handle);
	free(texture);
};