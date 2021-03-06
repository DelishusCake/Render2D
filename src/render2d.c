#include "render2d.h"

#define MAX_TEXTURES		(256)
// Maximum draw commands allowed in the draw list 
#define MAX_DRAW_CMDS		(1024)
// Batch limits
#define MAX_BATCH_RANGES	(1024)
#define MAX_BATCH_VERTS		(MAX_DRAW_CMDS*6)

// Helper function for loading a file from disk
static u8* r2d_load_entire_file(const char *file_name, size_t *size)
{
	u8* buffer = NULL;

	FILE *f = fopen(file_name, "rb");
	if (f)
	{
		fseek(f, 0, SEEK_END);
		const size_t f_size = ftell(f);
		fseek(f, 0, SEEK_SET);

		buffer = malloc((f_size+1)*sizeof(u8));
		assert(buffer != NULL);
		fread(buffer, sizeof(u8), f_size, f);
		fclose(f);

		buffer[f_size] = '\0';

		if (size) *size = f_size;
	};
	return buffer;
};

// Structure for describing OpenGL vertex layouts
typedef struct
{
	u32 size;			// Size (in # of members) of the vertex attrib
	GLenum type;		// Type (BYTE, SHORT, FLOAT, etc)
	bool normalized;	// Normalized? (See OpenGL docs) 
	size_t stride;		// Stride in bytes to next instance of this attrib
	size_t offset;		// Offset in bytes from the beginning of the vertex data to this attrib
} r2d_vertex_layout_t;

// Bind a vertex layout array
static inline void r2d_bind_vertex_layout(const r2d_vertex_layout_t *layout, size_t len)
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
} r2d_vertex_t;
// Default vertex structure layout
static const r2d_vertex_layout_t g_vertex_layout[] =
{
	{ 2, GL_FLOAT, false, sizeof(r2d_vertex_t), offsetof(r2d_vertex_t, pos) },
	{ 2, GL_FLOAT, false, sizeof(r2d_vertex_t), offsetof(r2d_vertex_t, uv) },
};
// Helper, create a vertex struct
static inline r2d_vertex_t r2d_vertex(v2 pos, v2 uv)
{
	r2d_vertex_t vertex;
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

static bool r2d_load_draw_shader();
static void r2d_free_draw_shader();

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

static void r2d_calculate_viewport(u32 width, u32 height);

typedef struct
{
	// Range texture
	u32 texture_handle;
	// Range coordinates
	u32 offset; // Offset, in number of vertices
	u32 count;	// Count, in number of vertices
} r2d_batch_range_t;

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
	r2d_vertex_t *vertices;
	// Range list
	u32 range_count;
	r2d_batch_range_t *ranges;
} g_batch;

static void r2d_alloc_batch();
static void r2d_free_batch();

static void r2d_push_sprite(const r2d_texture_t *texture, aabb_t sprite, xform2d_t xform);
static void r2d_flush_batch();

typedef struct
{
	aabb_t sprite;
	xform2d_t xform;
	r2d_texture_t *texture;
} draw_cmd_t;
static struct
{
	u32 cmd_count;
	draw_cmd_t *cmds;
} g_draw_list;

static bool r2d_alloc_draw_list();
static void r2d_free_draw_list();

// Internal texture handle
struct r2d_texture_t
{
	// Texture data
	u32 w, h;
	u8 *pixels;
	// OpenGL texture handle
	u32 handle;
	// Free list pointer
	r2d_texture_t *next_free;
};

static struct
{
	// Texture list write mutex
	// NOTE: Only used to control write operations (create/destroy/etc.), read access is free
	ticket_mtx_t mtx;

	// Texture list
	u32 texture_count;
	r2d_texture_t textures[MAX_TEXTURES];
	r2d_texture_t *free_texture; // Texture free list

	// Creation list
	u32 create_count;
	r2d_texture_t *create[MAX_TEXTURES];
	// Destruction list
	u32 destroy_count;
	r2d_texture_t *destroy[MAX_TEXTURES];
} g_texture_list;

static void r2d_init_textures();
static r2d_texture_t* r2d_get_texture_handle();
static void r2d_free_texture_handle(r2d_texture_t *texture);
static void r2d_free_all_textures();

static void r2d_create_queued_textures();
static void r2d_destroy_queued_textures();

bool r2d_init()
{
	if (r2d_load_draw_shader())
	{	
		r2d_init_textures();

		r2d_alloc_batch();
		r2d_alloc_draw_list();
		return true;
	}
	return false;
};
void r2d_free()
{
	r2d_free_all_textures();
	r2d_free_draw_shader();
	r2d_free_draw_list();
	r2d_free_batch();
};

v2 r2d_screen_to_viewport(v2 screen)
{
	v2 v;
	v.x = (screen.x / g_viewport.scale.x) - (g_viewport.x*g_viewport.scale.x);
	v.y = (screen.y / g_viewport.scale.y) - (g_viewport.y*g_viewport.scale.y);
	return v;
};

void r2d_clear(u32 width, u32 height)
{
	// Clear the draw list
	g_draw_list.cmd_count = 0;
	// Calculate the viewport for the frame
	r2d_calculate_viewport(width, height);
};
void r2d_draw_sprite(r2d_texture_t *texture, aabb_t sprite, xform2d_t xform)
{
	assert((g_draw_list.cmd_count+1) < MAX_DRAW_CMDS);

	const u32 index = g_draw_list.cmd_count ++;
	draw_cmd_t *cmd = g_draw_list.cmds + index;
	cmd->xform = xform;
	cmd->sprite = sprite;
	cmd->texture = texture;
};
void r2d_flush()
{
	// Create/upload any waiting textures
	// NOTE: Done at start of frame to make sure textures are ready for use
	r2d_create_queued_textures();

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
		for (u32 i = 0; i < g_draw_list.cmd_count; i++)
		{
			const draw_cmd_t *cmd = g_draw_list.cmds + i;
			const r2d_texture_t *texture = cmd->texture;
			if (texture->handle)
			{
				r2d_push_sprite(texture, cmd->sprite, cmd->xform);
			}
		};
		// Render the vertex batch
		r2d_flush_batch();
	}
	// Destroy any waiting textures
	// NOTE: Done at end of frame in case any textures are still in use
	r2d_destroy_queued_textures();
};

static bool r2d_load_draw_shader()
{
	bool result = false;

	char *vert_code = (char*) r2d_load_entire_file("data/shader.vert", NULL);
	char *frag_code = (char*) r2d_load_entire_file("data/shader.frag", NULL);
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
static void r2d_free_draw_shader()
{
	glDeleteProgram(g_draw_shader.program);
};

static void r2d_calculate_viewport(u32 width, u32 height)
{
	const f32 aspect_ratio = ((f32) R2D_SCREEN_W / (f32) R2D_SCREEN_H);

	g_viewport.w = width;
	g_viewport.h = (int) (g_viewport.w / aspect_ratio + 0.5f);
	if (g_viewport.h > height)
	{
		g_viewport.h = height;
		g_viewport.w = (int) (height * aspect_ratio + 0.5f);
	}
	g_viewport.x = (width - g_viewport.w) / 2;
	g_viewport.y = (height - g_viewport.h) / 2;

	g_viewport.scale.x = ((f32) width / R2D_SCREEN_W);
	g_viewport.scale.y = ((f32) height / R2D_SCREEN_H);

	const m44 ortho = m44_orthoOffCenter(0.f, (f32) width, (f32) height, 0.f, -1.f, 1.f);
	const m44 scale = m44_scale(g_viewport.scale.x, g_viewport.scale.y, 1.f);
	g_viewport.projection = m44_mul(ortho, scale);
};

static void r2d_alloc_batch()
{
	// Set the current buffer
	g_batch.current = 0;
	g_batch.range_count = 0;
	g_batch.vertex_count = 0;
	// Allocate memory
	g_batch.vertices = malloc(MAX_BATCH_VERTS*sizeof(r2d_vertex_t));
	assert(g_batch.vertices != NULL);
	g_batch.ranges = malloc(MAX_BATCH_RANGES*sizeof(r2d_batch_range_t));
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
			glBufferData(GL_ARRAY_BUFFER, (MAX_BATCH_VERTS*sizeof(r2d_vertex_t)), NULL, GL_STREAM_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
		glBindVertexArray(0);
	}
};
static void r2d_free_batch()
{
	glDeleteVertexArrays(2, g_batch.vao);
	glDeleteBuffers(2, g_batch.buf);
	free(g_batch.vertices);
	free(g_batch.ranges);
}
static void r2d_push_sprite(const r2d_texture_t *texture, aabb_t sprite, xform2d_t xform)
{
	// Get the range
	r2d_batch_range_t *range = NULL;
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
		g_batch.vertices[g_batch.vertex_count++] = r2d_vertex(sprite_verts[index], sprite_uvs[index]);
		// Increment the range index count
		range->count ++;
	}
};
static void r2d_flush_batch()
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
					memcpy(data, g_batch.vertices, g_batch.vertex_count*sizeof(r2d_vertex_t));
					glUnmapBuffer(GL_ARRAY_BUFFER);
					// Bind the vertex layout
					r2d_bind_vertex_layout(g_vertex_layout, static_len(g_vertex_layout));
					// For each range
					for (u32 i = 0; i < g_batch.range_count; i++)
					{
						// Get the range
						const r2d_batch_range_t *range = g_batch.ranges + i;
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

static void r2d_init_textures()
{
	g_texture_list.mtx = (ticket_mtx_t){0};
};
static r2d_texture_t* r2d_get_texture_handle()
{
	r2d_texture_t *texture = NULL;
	// If theres a texture in the free list
	if (g_texture_list.free_texture != NULL)
	{
		// Get the head of the free list
		texture = g_texture_list.free_texture;
		// Move the list forward
		g_texture_list.free_texture = texture->next_free;
	} else {
		// Nothing in the list, allocate a new one
		assert ((g_texture_list.texture_count + 1) < MAX_TEXTURES);
		const u32 index = u32_atomic_inc(&g_texture_list.texture_count);
		texture = g_texture_list.textures + index;
	}
	// Zero everything
	memset(texture, 0, sizeof(r2d_texture_t));
	return texture;
};
static void r2d_free_texture_handle(r2d_texture_t *texture)
{
	texture->next_free = g_texture_list.free_texture;
	g_texture_list.free_texture = texture;
}

r2d_texture_t* r2d_alloc_texture(u32 width, u32 height, u8 *pixels)
{
	// TODO: Texture formats
	const size_t size = width*height*4;
	
	r2d_texture_t *texture = NULL;
	ticket_mtx_lock(&g_texture_list.mtx);
	{
		// Get a free texture handle
		texture = r2d_get_texture_handle();
		// Set the data
		texture->w = width;
		texture->h = height;
		// Allocate and copy the pixel array
		texture->pixels = malloc(size);
		assert(texture->pixels != NULL);
		memcpy(texture->pixels, pixels, size);
		// Insert into the creation list
		assert ((g_texture_list.create_count + 1) < MAX_TEXTURES);
		g_texture_list.create[g_texture_list.create_count++] = texture;
	}
	ticket_mtx_unlock(&g_texture_list.mtx);
	return texture;
};
void r2d_free_texture(r2d_texture_t *texture)
{
	ticket_mtx_lock(&g_texture_list.mtx);
	{
		// Insert into the destroy list
		assert ((g_texture_list.destroy_count + 1) < MAX_TEXTURES);
		g_texture_list.create[g_texture_list.destroy_count++] = texture;
	}
	ticket_mtx_unlock(&g_texture_list.mtx);
};

static void r2d_free_all_textures()
{
	ticket_mtx_lock(&g_texture_list.mtx);
	{
		for (u32 i = 0; i < g_texture_list.texture_count; i++)
		{
			// Free texture data
			r2d_texture_t *texture = g_texture_list.textures + i;
			if (texture->handle)
				glDeleteTextures(1, &texture->handle);
			if (texture->pixels)
				free(texture->pixels);
		}
		g_texture_list.texture_count = 0;
	}
	ticket_mtx_unlock(&g_texture_list.mtx);
};

static void r2d_create_queued_textures()
{
	ticket_mtx_lock(&g_texture_list.mtx);
	{
		// Create every texture in the creation list
		for (u32 i = 0; i < g_texture_list.create_count; i++)
		{
			r2d_texture_t *texture = g_texture_list.create[i];

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
		g_texture_list.create_count = 0;
	}
	ticket_mtx_unlock(&g_texture_list.mtx);
};
static void r2d_destroy_queued_textures()
{
	ticket_mtx_lock(&g_texture_list.mtx);
	{	
		for (u32 i = 0; i < g_texture_list.destroy_count; i++)
		{
			// Free texture data
			r2d_texture_t *texture = g_texture_list.create[i];
			glDeleteTextures(1, &texture->handle);
			free(texture->pixels);
			// Add to the free list
			r2d_free_texture_handle(texture);
		}
		// Reset list
		g_texture_list.destroy_count = 0;
	}
	ticket_mtx_unlock(&g_texture_list.mtx);
};

static bool r2d_alloc_draw_list()
{
	g_draw_list.cmd_count = 0;
	g_draw_list.cmds = malloc(MAX_DRAW_CMDS*sizeof(draw_cmd_t));
	assert(g_draw_list.cmds != NULL);
	return true;
};
static void r2d_free_draw_list()
{
	free(g_draw_list.cmds);
};