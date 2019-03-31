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
	v2 i_size;
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
	texture->i_size = V2(1.f / (f32) width, 1.f / (f32) height);
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
	// Range texture
	texture_t *texture;
	// Range coordinates
	u32 offset; // Offset, in number of vertices
	u32 count;	// Count, in number of vertices
} batch_range_t;
typedef struct
{
	// OpenGL handles
	u32 vao, buf;
	// Vertex array, host allocated
	u32 vertex_count;
	vertex_t vertices[MAX_BATCH_VERTS];
	// Range list
	u32 range_count;
	batch_range_t ranges[MAX_BATCH_RANGES];
} batch_t;

static batch_t* batch_alloc()
{
	batch_t *batch = malloc(sizeof(batch_t));
	assert(batch != NULL);
	memset(batch, 0, sizeof(batch_t));

	// Generate some arrays/buffers
	glGenVertexArrays(1, &batch->vao);
	glGenBuffers(1, &batch->buf);
	// Make sure the buffers are big enough
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
	// Get the range
	batch_range_t *range = NULL;
	if (batch->range_count == 0)
	{
		// No current range, initialze one
		range = batch->ranges + batch->range_count++;
		range->texture = texture;
		range->offset = 0;
		range->count = 0;
	} else {
		// Get the current range
		range = batch->ranges + (batch->range_count-1);
	}
	// There's a new texture!
	if (range->texture != texture)
	{
		// Create a new range
		range = batch->ranges + batch->range_count ++;
		range->texture = texture;
		range->offset = batch->vertex_count;
		range->count = 0;
	};
	// Get the size of the sprite
	const v2 sprite_scale = v2_sub(sprite.max, sprite.min);
	// Calculate the transformed sprite vertices
	const v2 sprite_verts[] = 
	{
		xform2d_apply(xform, v2_mul(sprite_scale, V2(-0.5f, -0.5f))),
		xform2d_apply(xform, v2_mul(sprite_scale, V2( 0.5f, -0.5f))),
		xform2d_apply(xform, v2_mul(sprite_scale, V2( 0.5f,  0.5f))),
		xform2d_apply(xform, v2_mul(sprite_scale, V2(-0.5f,  0.5f))),
	};
	// Calculate the sprite texture coordinates
	const v2 sprite_uvs[] = 
	{
		v2_mul(texture->i_size, V2(sprite.min.x, sprite.min.y)),
		v2_mul(texture->i_size, V2(sprite.max.x, sprite.min.y)),
		v2_mul(texture->i_size, V2(sprite.max.x, sprite.max.y)),
		v2_mul(texture->i_size, V2(sprite.min.x, sprite.max.y)),
	};

	// Sprite indices
	const u16 indices[] = { 0, 1, 2, 0, 2, 3 };
	// For each index
	for (u32 i = 0; i < static_len(indices); i++)
	{
		// Get the index
		const u16 index = indices[i];
		// Push the vertex data
		batch->vertices[batch->vertex_count++] = vertex(sprite_verts[index], sprite_uvs[index]);
		// Increment the range index count
		range->count ++;
	}
};
static void batch_flush(batch_t *batch, const draw_shader_t *shader, const viewport_t *viewport)
{
	// If any ranges were recorded
	if (batch->range_count)
	{
		// Bind the shader
		glUseProgram(shader->program);
		{
			// Set the projection uniform
			glProgramUniformMatrix4fv(shader->program, shader->u_projection,
				1, false, (const f32*) viewport->projection.m);
			// Bind the vertex array
			glBindVertexArray(batch->vao);
			{
				// Bind the buffer
				glBindBuffer(GL_ARRAY_BUFFER, batch->buf);
				// Map the buffer for data upload
				void *data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
				if (data)
				{
					// Copy the data and un-map the buffer
					memcpy(data, batch->vertices, batch->vertex_count*sizeof(vertex_t));
					glUnmapBuffer(GL_ARRAY_BUFFER);
					// Bind the vertex layout
					bindVertexLayout(g_vertex_layout, static_len(g_vertex_layout));
					// For each range
					for (u32 i = 0; i < batch->range_count; i++)
					{
						// Get the range
						const batch_range_t *range = batch->ranges + i;
						// Bind the range texture
						glActiveTexture(GL_TEXTURE0); 
						glBindTexture(GL_TEXTURE_2D, range->texture->handle);
						// Issue the range draw call
						glDrawArrays(GL_TRIANGLES, range->offset, range->count);
					};
				}
			}
		}
		// Reset graphics state
		glBindVertexArray(0);
		glUseProgram(0);
		// Clear the batch
		batch->vertex_count = 0;
		batch->range_count = 0;
	}
};

static u32 g_current_batch;
static batch_t *g_batches[2];
static viewport_t g_viewport;
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
	g_viewport = get_viewport(width, height);

	// Clear the whole screen for the "black bars" effect
	glDisable(GL_SCISSOR_TEST);
	glClearColor(0.0f, 0.0f, 0.0f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);

	// Set the viewport/scissor region
	glEnable(GL_SCISSOR_TEST);
	glScissor(
		g_viewport.x, g_viewport.y, 
		g_viewport.w, g_viewport.h);
	glViewport(
		g_viewport.x, g_viewport.y, 
		g_viewport.w, g_viewport.h);
	// Clear the render area
	glClearColor(0.2f, 0.2f, 0.2f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	{
		// Set the drawing settings
		glDisable(GL_DEPTH_TEST);
		
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// Build the vertex batch
		batch_t *batch = g_batches[g_current_batch];
		for (u32 i = 0; i < draw_list->cmd_count; i++)
		{
			const draw_cmd_t *cmd = draw_list->cmds + i;
			batch_push(batch, cmd->image->texture, cmd->sprite, cmd->xform);
		};
		// Render the vertex batch
		batch_flush(batch, &g_shader, &g_viewport);
		// Double buffered batches for faster rendering
		g_current_batch = 1 - g_current_batch;
	}
};