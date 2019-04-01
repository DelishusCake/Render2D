#include "render.h"

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
	// Texture data
	u32 w, h;
	u8 *pixels;
	// OpenGL texture handle
	u32 handle;
	// Freelist pointer
	texture_t *next_free;
};

static struct
{
	// Free texture handle list
	texture_t *free_texture;
	// Creation list
	volatile u32 create_count;
	texture_t *create[256];
	// Destruction list
	volatile u32 destroy_count;
	texture_t *destroy[256];
} g_textures;

static texture_t* get_texture_handle()
{
	texture_t *texture = NULL;
	// If theres a texture in the free list
	if (g_textures.free_texture != NULL)
	{
		// Get the head of the free list
		texture = g_textures.free_texture;
		// Move the list forward
		g_textures.free_texture = texture->next_free;
	} else {
		// Nothing in the list, allocate a new one
		texture = malloc(sizeof(texture_t));
		assert(texture != NULL);
	}
	// Zero everything
	memset(texture, 0, sizeof(texture_t));
	return texture;
};
static void free_texture_handle(texture_t *texture)
{
	texture->next_free = g_textures.free_texture;
	g_textures.free_texture = texture;
}
static void create_textures()
{
	// Create every texture in the creation list
	for (u32 i = 0; i < g_textures.create_count; i++)
	{
		texture_t *texture = g_textures.create[i];

		glGenTextures(1, &texture->handle);

		glBindTexture(GL_TEXTURE_2D, texture->handle);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 
			0, GL_RGBA, texture->w, texture->h, 
			0, GL_RGBA, GL_UNSIGNED_BYTE, texture->pixels);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	// Reset list
	g_textures.create_count = 0;
};
static void destroy_textures()
{
	for (u32 i = 0; i < g_textures.destroy_count; i++)
	{
		// Free texture data
		texture_t *texture = g_textures.create[i];
		glDeleteTextures(1, &texture->handle);
		free(texture->pixels);
		// Add to the free list
		free_texture_handle(texture);
	}
	// Reset list
	g_textures.destroy_count = 0;
};

texture_t* texture_alloc(u32 width, u32 height, u8 *pixels)
{
	// TODO: Thread safety
	// TODO: Texture formats
	const size_t size = width*height*4;

	// Get a free texture handle
	texture_t *texture = get_texture_handle();
	// Set the data
	texture->w = width;
	texture->h = height;
	// Allocate and copy the pixel array
	texture->pixels = malloc(size);
	assert(texture->pixels != NULL);
	memcpy(texture->pixels, pixels, size);
	// Insert into the creation list
	const i32 index = atomic_inc(&g_textures.create_count);
	g_textures.create[index] = texture;
	return texture;
};
void texture_free(texture_t *texture)
{
	// Insert into the destroy list
	const i32 index = atomic_inc(&g_textures.destroy_count);
	g_textures.create[index] = texture;
};

typedef struct
{
	u32 program;
	// Locations
	u32 u_projection;
	u32 u_sampler;
} draw_shader_t;

static bool load_draw_shader(draw_shader_t *shader)
{
	bool result = false;

	char *vert_code = (char*) load_entire_file("data/shader.vert", NULL);
	char *frag_code = (char*) load_entire_file("data/shader.frag", NULL);
	if (vert_code && frag_code)
	{
		const u32 shader_vert = glCreateShader(GL_VERTEX_SHADER);
		const u32 shader_frag = glCreateShader(GL_FRAGMENT_SHADER);

		glShaderSource(shader_vert, 1, (const char**) &vert_code, NULL);
		glShaderSource(shader_frag, 1, (const char**) &frag_code, NULL);

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
		if (!len)
		{
			shader->u_projection = glGetUniformLocation(shader->program, "u_projection");
			shader->u_sampler = glGetUniformLocation(shader->program, "u_sampler");
			result = true;
		} else {
			fprintf(stderr, buf);
		}

		free(vert_code);
		free(frag_code);
	}
	return result;
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
	u32 texture_handle;
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
	const texture_t *texture, aabb_t sprite, xform2d_t xform)
{
	// Get the range
	batch_range_t *range = NULL;
	if (batch->range_count == 0)
	{
		// No current range, initialze one
		range = batch->ranges + batch->range_count++;
		range->texture_handle = texture->handle;
		range->offset = 0;
		range->count = 0;
	} else {
		// Get the current range
		range = batch->ranges + (batch->range_count-1);
	}
	// There's a new texture!
	if (range->texture_handle != texture->handle)
	{
		// Create a new range
		range = batch->ranges + batch->range_count ++;
		range->texture_handle = texture->handle;
		range->offset = batch->vertex_count;
		range->count = 0;
	};
	// Inverse texture size for UV calculation
	const v2 i_size = V2(1.f / (f32) texture->w, 1.f / (f32) texture->h);
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
		v2_mul(i_size, V2(sprite.min.x, sprite.min.y)),
		v2_mul(i_size, V2(sprite.max.x, sprite.min.y)),
		v2_mul(i_size, V2(sprite.max.x, sprite.max.y)),
		v2_mul(i_size, V2(sprite.min.x, sprite.max.y)),
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
						glBindTexture(GL_TEXTURE_2D, range->texture_handle);
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
	if (load_draw_shader(&g_shader))
	{	
		g_current_batch = 0;
		g_batches[0] = batch_alloc();
		g_batches[1] = batch_alloc();
		return true;
	}
	return false;
};
void render_free()
{
	batch_free(g_batches[1]);
	batch_free(g_batches[0]);
};

void render(u32 width, u32 height, const draw_list_t *draw_list)
{
	// Calculate the viewport for the frame
	g_viewport = get_viewport(width, height);

	// Create/upload any waiting textures
	// NOTE: Done at start of frame to make sure textures are ready for use
	create_textures();

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

			const image_t *image = cmd->image;
			if (image->asset.state == ASSET_STATE_LOADED)
			{
				const texture_t *texture = cmd->image->texture;
				if (texture->handle)
				{
					batch_push(batch, texture, cmd->sprite, cmd->xform);
				}
			}
		};
		// Render the vertex batch
		batch_flush(batch, &g_shader, &g_viewport);
		// Double buffered batches for faster rendering
		g_current_batch = (1 - g_current_batch);
	}
	// Destroy any waiting textures
	// NOTE: Done at end of frame in case any textures are still in use
	destroy_textures();

	Sleep(1000);
};