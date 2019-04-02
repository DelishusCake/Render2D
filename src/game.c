#include "game.h"

typedef struct
{
	v2 pos;

	aabb_t sprite;
	image_t *image;
} player_t;
static void create_player(player_t *player, assets_t *assets, f32 x, f32 y)
{
	player->pos.x = x;
	player->pos.y = y;

	player->sprite = aabb_rect(306.f, 112.f, 12.f, 16.f);
	player->image = get_image_asset(assets, "data/dungeon_sheet.png");
};
static void draw_player(player_t *player, draw_list_t *draw_list)
{
	xform2d_t xform = xform2d(player->pos, 0.f);
	draw_sprite(draw_list, player->image, player->sprite, xform);
};

static player_t g_player; 
void init_game(assets_t *assets)
{
	create_player(&g_player, assets, 100.f, 100.f);
};
void update_and_draw_game(f64 delta, assets_t *assets, draw_list_t *draw_list)
{
	clear_draw_list(draw_list);
	draw_player(&g_player, draw_list);
};