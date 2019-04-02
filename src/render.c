#include "render.h"

typedef struct
{
	u32 size;			// Size (in # of members) of the vertex attrib
	GLenum type;		// Type (BYTE, SHORT, FLOAT, etc)
	bool normalized;	// Normalized? (See OpenGL docs) 
	size_t stride;		// Stride in bytes to next instance of this attrib
	size_t offset;		// Offset in bytes from the beginning of the vertex data to this attrib
} vertex_layout_t;

// Bind a vertex layout array
static inline void bind_vertex_layout(const vertex_layout_t *layout, size_t len)
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

// Default vertex structure
typedef struct
{
	v2 pos;
	v2 uv;
} vertex_t;
// Default vertex structure layout
static vertex_layout_t g_vertex_layout[] =
{
	{ 2, GL_FLOAT, false, sizeof(vertex_t), offsetof(vertex_t, pos) },
	{ 2, GL_FLOAT, false, sizeof(vertex_t), offsetof(vertex_t, uv) },
};
// Helper, create a vertex struct
static inline vertex_t vertex(v2 pos, v2 uv)
{
	vertex_t vertex;
	vertex.pos = pos;
	vertex.uv = uv;
	return vertex;
}

// Default drawing shader
static struct
{
	u32 program;
	// Locations
	u32 u_projection;
	u32 u_sampler;
} g_draw_shader;

static bool load_draw_shader();

// Viewport structure, used for resolution independent rendering
static struct
{
	// Viewport coordinates
	int x,y,w,h;
	// Viewport scale
	v2 scale;
	// Viewport projection matrix
	m44 projection;
} g_viewport;

static void calculate_viewport(u32 width, u32 height);
#if 0
static v2 get_viewport_point(v2 v);
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

// Double buffered dynamic vertex array, used for quickly rendering many sprites
static struct
{
	// Current buffer to write to
	u32 current;
	// OpenGL handles
	u32 vao[2];
	u32 buf[2];
	// Vertex array, host allocated
	u32 vertex_count;
	vertex_t *vertices;
	// Range list
	u32 range_count;
	batch_range_t *ranges;
} g_batch;

static void alloc_batch();
static void free_batch();

static void push_sprite(const texture_t *texture, aabb_t sprite, xform2d_t xform);
static void flush_batch();

// Internal texture handle
struct texture_t
{
	// Texture data
	u32 w, h;
	u8 *pixels;
	// OpenGL texture handle
	u32 handle;
	// Free list pointer
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

static void create_textures();
static void destroy_textures();

bool init_renderer()
{
	if (load_draw_shader())
	{	
		alloc_batch();
		return true;
	}
	return false;
};
void free_renderer()
{
	free_batch();
};

void render(u32 width, u32 height, const draw_list_t *draw_list)
{
	// Calculate the viewport for the frame
	calculate_viewport(width, height);

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
		for (u32 i = 0; i < draw_list->cmd_count; i++)
		{
			const draw_cmd_t *cmd = draw_list->cmds + i;

			const image_t *image = cmd->image;
			if (image->asset.state == ASSET_STATE_LOADED)
			{
				const texture_t *texture = cmd->image->texture;
				if (texture->handle)
				{
					push_sprite(texture, cmd->sprite, cmd->xform);
				}
			}
		};
		// Render the vertex batch
		flush_batch();
	}
	// Destroy any waiting textures
	// NOTE: Done at end of frame in case any textures are still in use
	destroy_textures();
};

static bool load_draw_shader()
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

		g_draw_shader.program = glCreateProgram();
		glAttachShader(g_draw_shader.program, shader_vert);
		glAttachShader(g_draw_shader.program, shader_frag);
		glLinkProgram(g_draw_shader.program);

		glDeleteShader(shader_vert);
		glDeleteShader(shader_frag);

		int len;
		char buf[1024];
		glGetProgramInfoLog(g_draw_shader.program, static_len(buf), &len, buf);
		if (!len)
		{
			g_draw_shader.u_projection = glGetUniformLocation(g_draw_shader.program, "u_projection");
			g_draw_shader.u_sampler = glGetUniformLocation(g_draw_shader.program, "u_sampler");
			result = true;
		} else {
			fprintf(stderr, buf);
		}

		free(vert_code);
		free(frag_code);
	}
	return result;
}

static void calculate_viewport(u32 width, u32 height)
{
	const f32 aspect_ratio = ((f32) SCREEN_W / (f32) SCREEN_H);

	g_viewport.w = width;
	g_viewport.h = (int) (g_viewport.w / aspect_ratio + 0.5f);
	if (g_viewport.h > height)
	{
		g_viewport.h = height;
		g_viewport.w = (int) (height * aspect_ratio + 0.5f);
	}
	g_viewport.x = (width - g_viewport.w) / 2;
	g_viewport.y = (height - g_viewport.h) / 2;

	g_viewport.scale.x = ((f32) width / SCREEN_W);
	g_viewport.scale.y = ((f32) height / SCREEN_H);

	const m44 ortho = m44_orthoOffCenter(0.f, (f32) width, (f32) height, 0.f, -1.f, 1.f);
	const m44 scale = m44_scale(g_viewport.scale.x, g_viewport.scale.y, 1.f);
	g_viewport.projection = m44_mul(ortho, scale);
};
#if 0
static v2 get_viewport_point(, v2 v)
{
	v2 r;
	r.x = (v.x / viewport->scale_x) - viewport->x*viewport->scale_x;
	r.y = (v.y / viewport->scale_y) - viewport->y*viewport->scale_y;
	return r;
}
#endif

static void alloc_batch()
{
	// Set the current buffer
	g_batch.current = 0;
	g_batch.range_count = 0;
	g_batch.vertex_count = 0;
	// Allocate memory
	g_batch.vertices = malloc(MAX_BATCH_VERTS*sizeof(vertex_t));
	assert(g_batch.vertices != NULL);
	g_batch.ranges = malloc(MAX_BATCH_RANGES*sizeof(batch_range_t));
	assert(g_batch.ranges != NULL);
	// Generate some arrays/buffers
	glGenVertexArrays(2, g_batch.vao);
	glGenBuffers(2, g_batch.buf);
	// Make sure the buffers are big enough
	for (u32 i = 0; i < 2; i++)
	{
		glBindVertexArray(g_batch.vao[i]);
		{
			glBindBuffer(GL_ARRAY_BUFFER, g_batch.buf[i]);
			glBufferData(GL_ARRAY_BUFFER, (MAX_BATCH_VERTS*sizeof(vertex_t)), NULL, GL_STREAM_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
		glBindVertexArray(0);
	}
};
static void free_batch()
{
	glDeleteVertexArrays(2, g_batch.vao);
	glDeleteBuffers(2, g_batch.buf);
	free(g_batch.vertices);
	free(g_batch.ranges);
}
static void push_sprite(const texture_t *texture, aabb_t sprite, xform2d_t xform)
{
	// Get the range
	batch_range_t *range = NULL;
	if (g_batch.range_count == 0)
	{
		// No current range, initialze one
		range = g_batch.ranges + g_batch.range_count++;
		range->texture_handle = texture->handle;
		range->offset = 0;
		range->count = 0;
	} else {
		// Get the current range
		range = g_batch.ranges + (g_batch.range_count-1);
	}
	// There's a new texture!
	if (range->texture_handle != texture->handle)
	{
		// Create a new range
		range = g_batch.ranges + g_batch.range_count ++;
		range->texture_handle = texture->handle;
		range->offset = g_batch.vertex_count;
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
		g_batch.vertices[g_batch.vertex_count++] = vertex(sprite_verts[index], sprite_uvs[index]);
		// Increment the range index count
		range->count ++;
	}
};
static void flush_batch()
{
	// If any ranges were recorded
	if (g_batch.range_count)
	{
		// Bind the shader
		glUseProgram(g_draw_shader.program);
		{
			// Set the projection uniform
			glProgramUniformMatrix4fv(g_draw_shader.program, g_draw_shader.u_projection,
				1, false, (const f32*) g_viewport.projection.m);
			// Bind the vertex array
			glBindVertexArray(g_batch.vao[g_batch.current]);
			{
				// Bind the buffer
				glBindBuffer(GL_ARRAY_BUFFER, g_batch.buf[g_batch.current]);
				// Map the buffer for data upload
				void *data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
				if (data)
				{
					// Copy the data and un-map the buffer
					memcpy(data, g_batch.vertices, g_batch.vertex_count*sizeof(vertex_t));
					glUnmapBuffer(GL_ARRAY_BUFFER);
					// Bind the vertex layout
					bind_vertex_layout(g_vertex_layout, static_len(g_vertex_layout));
					// For each range
					for (u32 i = 0; i < g_batch.range_count; i++)
					{
						// Get the range
						const batch_range_t *range = g_batch.ranges + i;
						// Bind the range texture
						glActiveTexture(GL_TEXTURE0); 
						glBindTexture(GL_TEXTURE_2D, range->texture_handle);
						// Issue the range draw call
						glDrawArrays(GL_TRIANGLES, range->offset, range->count);
					};
				}
			}
			glBindVertexArray(0);
		}
		glUseProgram(0);
	}
	// Clear the batch
	g_batch.vertex_count = 0;
	g_batch.range_count = 0;
	// Go to the next buffer
	g_batch.current = 1 - g_batch.current;
};

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
texture_t* alloc_texture(u32 width, u32 height, u8 *pixels)
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
void free_texture(texture_t *texture)
{
	// Insert into the destroy list
	const i32 index = atomic_inc(&g_textures.destroy_count);
	g_textures.create[index] = texture;
};

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