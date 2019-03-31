#include "render.h"

typedef struct
{
	int x,y,w,h;
	f32 scale_x, scale_y;
	m44 projection;
} viewport_t;
static viewport_t get_viewport(u32 width, u32 height)
{
	const f32 aspect_ratio = ((f32) SCREEN_W / (f32) SCREEN_H);

	viewport_t viewport;

	viewport.w = width;
	viewport.h = (int) (viewport.w / aspect_ratio + 0.5f);
	if (viewport.h > height)
	{
		viewport.h = height;
		viewport.w = (int) (height * aspect_ratio + 0.5f);
	}
	viewport.x = (width - viewport.w) / 2;
	viewport.y = (height - viewport.h) / 2;

	viewport.scale_x = ((f32) width / SCREEN_W);
	viewport.scale_y = ((f32) height / SCREEN_H);

	const m44 ortho = m44_orthoOffCenter(0.f, (f32) width, (f32) height, 0.f, -1.f, 1.f);
	const m44 scale = m44_scale(viewport.scale_x, viewport.scale_y, 1.f);
	viewport.projection = m44_mul(ortho, scale);

	return viewport;
};
#if 0
static v2 get_viewport_point(const viewport_t *viewport, v2 v)
{
	v2 r;
	r.x = (v.x / viewport->scale_x) - viewport->x*viewport->scale_x;
	r.y = (v.y / viewport->scale_y) - viewport->y*viewport->scale_y;
	return r;
}
#endif

bool render_init()
{
	return true;
};
void render_free()
{
};

void render(u32 width, u32 height, const draw_list_t *draw_list)
{
	viewport_t viewport = get_viewport(width, height);

	glDisable(GL_DEPTH_TEST);

	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glClearColor(0.2f, 0.2f, 0.2f, 1.f);

	glViewport(viewport.x, viewport.y, viewport.w, viewport.h);
	glClear(GL_COLOR_BUFFER_BIT);

	for (u32 i = 0; i < draw_list->cmd_count; i++)
	{
		const draw_cmd_t *cmd = draw_list->cmds + i;
		// TODO
	};
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