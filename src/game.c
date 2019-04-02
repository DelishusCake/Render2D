#include "game.h"

#define MAX_ENTITIES	(256)

typedef u32 entity_t;

#define NULL_ENTITY	(0xFFFFFFFF)

typedef enum
{
	COMPONENT_SET_EMPTY = 0,
	COMPONENT_SPRITE = (1 << 0),
	COMPONENT_TRANSFORM = (1 << 1),
} component_set_t;

typedef struct
{
	aabb_t aabb;
	image_t *image;
} sprite_t;

typedef struct
{
	u32 entity_count;
	u32 free_entity;

	component_set_t components[MAX_ENTITIES];

	sprite_t  sprite[MAX_ENTITIES];
	xform2d_t transform[MAX_ENTITIES];
	u32       next_free[MAX_ENTITIES];
} world_t;

static world_t* alloc_world()
{
	world_t *world = malloc(sizeof(world_t));
	assert(world != NULL);
	memset(world, 0, sizeof(world_t));

	for (u32 i = 0; i < MAX_ENTITIES; i++)
	{
		world->next_free[i] = NULL_ENTITY;
	}
	world->free_entity = NULL_ENTITY;

	return world;
};
static entity_t create_entity(world_t *world, component_set_t components)
{
	entity_t entity = NULL_ENTITY;
	if (world->free_entity != NULL_ENTITY)
	{
		entity = world->free_entity;
		world->free_entity = world->next_free[entity];
	} else {
		assert((world->entity_count+1) < MAX_ENTITIES);
		entity = world->entity_count ++;
	};
	if (entity != NULL_ENTITY)
	{
		world->components[entity] = components;
	};
	return entity;
};

static void system_draw_sprites(world_t *world, draw_list_t *draw_list, f64 delta)
{
	const component_set_t components = (COMPONENT_TRANSFORM | COMPONENT_SPRITE);
	for (u32 i = 0; i < world->entity_count; i++)
	{
		if ((world->components[i] & components) == components)
		{
			const sprite_t *sprite = world->sprite + i;
			const xform2d_t xform = world->transform[i];
			draw_sprite(draw_list, sprite->image, sprite->aabb, xform);
		};
	};
};

static entity_t create_player(world_t *world, assets_t *assets, v2 pos)
{
	const component_set_t components = (COMPONENT_TRANSFORM | COMPONENT_SPRITE);

	entity_t player = create_entity(world, components);
	if (player != NULL_ENTITY)
	{
		world->transform[player] = xform2d(pos, 0.f);

		sprite_t *sprite = world->sprite + player;
		sprite->aabb = aabb_rect(306.f, 112.f, 12.f, 16.f);
		sprite->image = get_image_asset(assets, "data/dungeon_sheet.png");
	};
	return player;
};

static world_t *g_world;

void init_game(assets_t *assets)
{
	g_world = alloc_world();
	create_player(g_world, assets, V2(100.f, 100.f));
};
void update_and_draw_game(f64 delta, assets_t *assets, draw_list_t *draw_list)
{
	clear_draw_list(draw_list);
	system_draw_sprites(g_world, draw_list, delta);
};