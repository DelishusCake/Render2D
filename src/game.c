#include "game.h"

typedef struct
{
	v2 pos;

	aabb_t sprite;
	image_t *image;
} player_t;
static void player_create(player_t *player, assets_t *assets, f32 x, f32 y)
{
	player->pos.x = x;
	player->pos.y = y;

	player->sprite = aabb_rect(306.f, 112.f, 12.f, 16.f);
	player->image = assets_get_image(assets, "data/dungeon_sheet.png");
};
static void player_draw(player_t *player, draw_list_t *draw_list)
{
	xform2d_t xform = xform2d(player->pos, 0.f);
	draw_sprite(draw_list, player->image, player->sprite, xform);
};

static player_t g_player; 
void game_init(assets_t *assets)
{
	player_create(&g_player, assets, 100.f, 100.f);
};
void game_update_and_draw(f64 delta, assets_t *assets, draw_list_t *draw_list)
{
	draw_list_clear(draw_list);
	player_draw(&g_player, draw_list);
};