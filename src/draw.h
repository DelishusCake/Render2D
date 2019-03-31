#ifndef DRAW_H
#define DRAW_H

#include "core.h"
#include "geom.h"

#include "assets.h"

#define SCREEN_W	(1920 >> 2)
#define SCREEN_H	(1080 >> 2)

#define MAX_DRAW_CMDS	(1024)

typedef struct
{
	image_t *image;

	aabb_t sprite;
	xform2d_t xform;
} draw_cmd_t;
typedef struct
{
	u32 cmd_count;
	draw_cmd_t cmds[MAX_DRAW_CMDS];
} draw_list_t;

draw_list_t* draw_list_alloc();
void         draw_list_free(draw_list_t *draw_list);

void draw_list_clear(draw_list_t *draw_list);
void draw_sprite(draw_list_t *draw_list, image_t *image, aabb_t sprite, xform2d_t xform);

#endif