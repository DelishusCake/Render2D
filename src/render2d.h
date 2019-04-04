#ifndef RENDER_H
#define RENDER_H

#include <stdio.h>

#include <GL\gl3w.h>

#include "core.h"
#include "geom.h"

// Virtual screen width and height to render at
#define R2D_SCREEN_W	(1920 >> 2)
#define R2D_SCREEN_H	(1080 >> 2)

// Forward declare some structures for rendering
decl_struct(r2d_texture_t);

// Library initialization/destruction
bool r2d_init();
void r2d_free();

// Get the viewport position of a point on the screen
v2 r2d_screen_to_viewport(v2 screen);

// Allocate/free teextures for drawing
r2d_texture_t* r2d_alloc_texture(u32 width, u32 height, u8 *pixels);
void           r2d_free_texture(r2d_texture_t *texture);

// Clear the draw buffer and begin a new frame
void r2d_clear(u32 width, u32 height);
// Draw a sprite with a given texture and transformation
void r2d_draw_sprite(r2d_texture_t *texture, aabb_t sprite, xform2d_t xform);
// Flush the draw buffer to the screen
void r2d_flush();

#endif