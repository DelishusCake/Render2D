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

static world_t* alloc_world();
static void free_world(world_t *world, assets_t *assets);

static entity_t create_entity(world_t *world, component_set_t components);
static void     destroy_entity(world_t *world, assets_t *assets, entity_t entity);

static void system_draw_sprites(world_t *world, f64 delta);

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
static assets_t *g_assets;

bool init_game()
{
	if (r2d_init())
	{
		g_world = alloc_world();
		g_assets = alloc_assets();

		create_player(g_world, g_assets, V2(100.f, 100.f));

		return true;
	}
	return false;
};
void free_game()
{
	free_world(g_world, g_assets);
	free_assets(g_assets);
	r2d_free();
}
void update_and_draw_game(i32 width, i32 height, f64 delta)
{
	r2d_clear(width, height);
	{
		system_draw_sprites(g_world, delta);
	}
	r2d_flush();
};

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
static void free_world(world_t *world, assets_t *assets)
{
	for (u32 i = 0; i < world->entity_count; i++)
	{
		destroy_entity(world, assets, i);
	}
	free(world);
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
static void destroy_entity(world_t *world, assets_t *assets, entity_t entity)
{
	if (world->components[entity] & COMPONENT_SPRITE)
	{
		sprite_t *sprite = world->sprite + entity;
		release_asset(assets, (asset_t*) sprite->image);
	};

	world->next_free[entity] = world->free_entity;
	world->free_entity = entity;
};

static void system_draw_sprites(world_t *world, f64 delta)
{
	const component_set_t components = (COMPONENT_TRANSFORM | COMPONENT_SPRITE);
	for (u32 i = 0; i < world->entity_count; i++)
	{
		if ((world->components[i] & components) == components)
		{
			const sprite_t *sprite = world->sprite + i;
			const xform2d_t xform = world->transform[i];

			const image_t *image = sprite->image;
			if (image->asset.state == ASSET_STATE_LOADED)
			{
				r2d_texture_t *texture = image->texture; 
				r2d_draw_sprite(texture, sprite->aabb, xform);
			}
		};
	};
};