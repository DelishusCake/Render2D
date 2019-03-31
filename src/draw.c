#include "draw.h"

draw_list_t* draw_list_alloc()
{
	draw_list_t *draw_list = malloc(sizeof(draw_list_t));
	assert(draw_list != NULL);
	memset(draw_list, 0, sizeof(draw_list_t));
	return draw_list;
};
void draw_list_free(draw_list_t *draw_list)
{
	free(draw_list);
};

void draw_list_clear(draw_list_t *draw_list)
{
	draw_list->cmd_count = 0;
};
void draw_sprite(draw_list_t *draw_list, image_t *image, aabb_t sprite, xform2d_t xform)
{
	assert((draw_list->cmd_count+1) < MAX_DRAW_CMDS);

	const u32 index = draw_list->cmd_count ++;
	draw_cmd_t *cmd = draw_list->cmds + index;
	cmd->image = image;
	cmd->sprite = sprite;
	cmd->xform = xform;
};