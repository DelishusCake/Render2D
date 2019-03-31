#include "render.h"

#define GLSL_HEADER	\
	"#version 330 core\n"\
	"#extension GL_ARB_separate_shader_objects : enable\n"

#define GLSL(code)	GLSL_HEADER#code

typedef struct
{
	u32 size;			// Size (in # of members) of the vertex attrib
	GLenum type;		// Type (BYTE, SHORT, FLOAT, etc)
	bool normalized;	// Normalized? (See OpenGL docs) 
	size_t stride;		// Stride in bytes to next instance of this attrib
	size_t offset;		// Offest in bytes from the begining of the vertex data to this attrib
} vertex_layout_t;

// Bind a vertex layout array
static inline void bindVertexLayout(const vertex_layout_t *layout, size_t len)
{
	for (size_t i = 0; i < len; i++)
	{
		glEnableVertexAttribArray(i);
		glVertexAttribPointer(i, 
			layout[i].size,
			layout[i].type,
			layout[i].normalized,
			layout[i].stride,
			(const void*) layout[i].offset);
	}
};

typedef struct
{
	v2 pos;
	v2 uv;
} vertex_t;
static vertex_layout_t g_vertex_layout[] =
{
	{ 2, GL_FLOAT, false, sizeof(vertex_t), offsetof(vertex_t, pos) },
	{ 2, GL_FLOAT, false, sizeof(vertex_t), offsetof(vertex_t, uv) },
};
static vertex_t vertex(v2 pos, v2 uv)
{
	vertex_t vertex;
	vertex.pos = pos;
	vertex.uv = uv;
	return vertex;
}

struct texture_t
{
	f32 w, h;
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
	texture->w = width;
	texture->h = height;
	texture->handle = handle;
	return texture;
};
void texture_free(texture_t *texture)
{
	glDeleteTextures(1, &texture->handle);
	free(texture);
};

const char *g_shader_vertex = GLSL(
	layout(location=0) in vec2 i_pos;
	layout(location=1) in vec2 i_uv;

	out VS_OUT
	{
		vec2 uv;
	} vs_out;

	uniform mat4 u_projection;

	void main()
	{
		vs_out.uv = i_uv;
		gl_Position = u_projection * vec4(i_pos, 0.f, 1.f);
	};
);
const char *g_shader_fragment = GLSL(
	in VS_OUT
	{
		vec2 uv;
	} fs_in;

	uniform sampler2D u_sampler;

	out vec4 o_frag;

	void main()
	{
		o_frag = texture(u_sampler, fs_in.uv);
	};
);

typedef struct
{
	u32 program;
	// Locations
	u32 u_projection;
	u32 u_sampler;
} draw_shader_t;

static void draw_shader_create(draw_shader_t *shader)
{
	const u32 shader_vert = glCreateShader(GL_VERTEX_SHADER);
	const u32 shader_frag = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(shader_vert, 1, &g_shader_vertex, NULL);
	glShaderSource(shader_frag, 1, &g_shader_fragment, NULL);

	glCompileShader(shader_vert);
	glCompileShader(shader_frag);

	shader->program = glCreateProgram();
	glAttachShader(shader->program, shader_vert);
	glAttachShader(shader->program, shader_frag);
	glLinkProgram(shader->program);

	glDeleteShader(shader_vert);
	glDeleteShader(shader_frag);

	int len;
	char buf[1024];
	glGetProgramInfoLog(shader->program, static_len(buf), &len, buf);
	if(len)
	{
		fprintf(stderr, buf);
	}

	shader->u_projection = glGetUniformLocation(shader->program, "u_projection");
	shader->u_sampler = glGetUniformLocation(shader->program, "u_sampler");
};

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

#define MAX_BATCH_RANGES	(1024)
#define MAX_BATCH_VERTS		(MAX_BATCH_RANGES*6)

typedef struct
{
	texture_t *texture;
	u32 offset;
	u32 count;
} batch_range_t;
typedef struct
{
	u32 vao, buf;

	u32 vertex_count;
	vertex_t vertices[MAX_BATCH_VERTS];

	u32 range_count;
	batch_range_t ranges[MAX_BATCH_RANGES];
} batch_t;

static batch_t* batch_alloc()
{
	batch_t *batch = malloc(sizeof(batch_t));
	assert(batch != NULL);
	memset(batch, 0, sizeof(batch_t));

	glGenVertexArrays(1, &batch->vao);
	glGenBuffers(1, &batch->buf);

	glBindVertexArray(batch->vao);
	{
		glBindBuffer(GL_ARRAY_BUFFER, batch->buf);
		glBufferData(GL_ARRAY_BUFFER, sizeof(batch->vertices), NULL, GL_STREAM_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	glBindVertexArray(0);
	return batch;
};
static void batch_free(batch_t *batch)
{
	glDeleteVertexArrays(1, &batch->vao);
	glDeleteBuffers(1, &batch->buf);
	free(batch);
}
static void batch_push(batch_t *batch, 
	texture_t *texture, aabb_t sprite, xform2d_t xform)
{
	batch_range_t *range = NULL;
	if (batch->range_count == 0)
	{
		range = batch->ranges + batch->range_count++;
		range->texture = texture;
		range->offset = 0;
		range->count = 0;
	} else {
		range = batch->ranges + (batch->range_count-1);
	}
	if (range->texture != texture)
	{
		range = batch->ranges + batch->range_count;
		range->texture = texture;
		range->offset = batch->vertex_count;
		range->count = 0;
		batch->range_count ++;
	};

	const v2 i_tex_size = V2(1.f / texture->w, 1.f / texture->h);

	const v2 scale = v2_sub(sprite.max, sprite.min);
	const v2 verts[] = 
	{
		xform2d_apply(xform, v2_mul(scale, V2(-0.5f, -0.5f))),
		xform2d_apply(xform, v2_mul(scale, V2( 0.5f, -0.5f))),
		xform2d_apply(xform, v2_mul(scale, V2( 0.5f,  0.5f))),
		xform2d_apply(xform, v2_mul(scale, V2(-0.5f,  0.5f))),
	};
	const v2 uvs[] = 
	{
		v2_mul(i_tex_size, V2(sprite.min.x, sprite.min.y)),
		v2_mul(i_tex_size, V2(sprite.max.x, sprite.min.y)),
		v2_mul(i_tex_size, V2(sprite.max.x, sprite.max.y)),
		v2_mul(i_tex_size, V2(sprite.min.x, sprite.max.y)),
	};

	batch->vertices[batch->vertex_count++] = vertex(verts[0], uvs[0]);
	batch->vertices[batch->vertex_count++] = vertex(verts[1], uvs[1]);
	batch->vertices[batch->vertex_count++] = vertex(verts[2], uvs[2]);

	batch->vertices[batch->vertex_count++] = vertex(verts[0], uvs[0]);
	batch->vertices[batch->vertex_count++] = vertex(verts[2], uvs[2]);
	batch->vertices[batch->vertex_count++] = vertex(verts[3], uvs[3]);

	range->count += 6;
};
static void batch_flush(batch_t *batch, const draw_shader_t *shader, const viewport_t *viewport)
{
	if (batch->range_count)
	{
		glUseProgram(shader->program);
		{
			glProgramUniformMatrix4fv(shader->program, shader->u_projection,
				1, false, (const f32*) viewport->projection.m);
			
			glBindVertexArray(batch->vao);
			{
				glBindBuffer(GL_ARRAY_BUFFER, batch->buf);
				glBufferSubData(GL_ARRAY_BUFFER, 0, batch->vertex_count*sizeof(vertex_t), batch->vertices);

				bindVertexLayout(g_vertex_layout, static_len(g_vertex_layout));

				for (u32 i = 0; i < batch->range_count; i++)
				{
					const batch_range_t *range = batch->ranges + i;
					glActiveTexture(GL_TEXTURE0); 
					glBindTexture(GL_TEXTURE_2D, range->texture->handle);
					glDrawArrays(GL_TRIANGLES, range->offset, range->count);
				};
			}
			glBindVertexArray(0);
		}
		glUseProgram(0);

		batch->vertex_count = 0;
		batch->range_count = 0;
	}
};

static u32 g_current_batch;
static batch_t *g_batches[2];
static draw_shader_t g_shader;

bool render_init()
{
	g_current_batch = 0;
	g_batches[0] = batch_alloc();
	g_batches[1] = batch_alloc();
	draw_shader_create(&g_shader);
	return true;
};
void render_free()
{
	batch_free(g_batches[1]);
	batch_free(g_batches[0]);
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

	batch_t *batch = g_batches[g_current_batch];
	for (u32 i = 0; i < draw_list->cmd_count; i++)
	{
		const draw_cmd_t *cmd = draw_list->cmds + i;
		batch_push(batch, cmd->image->texture, cmd->sprite, cmd->xform);
	};
	batch_flush(batch, &g_shader, &viewport);
	g_current_batch = 1 - g_current_batch;
};