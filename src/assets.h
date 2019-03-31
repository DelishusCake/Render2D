#ifndef ASSETS_H
#define ASSETS_H

#include <stb_image.h>

#include "core.h"

#define ASSET_NAME_LEN	(512)
#define ASSET_HASH_LEN	(1024)
#define ASSET_QUEUE_LEN	(256)

// Platform specific assets/functions
// NOTE: For internal use only
decl_struct(texture_t);
texture_t* alloc_texture(u32 width, u32 height, u8 *pixels);
void       free_texture(texture_t *texture);

// General asset header data
typedef enum
{
	ASSET_NONE,
	ASSET_IMAGE,
} asset_type_t;
typedef struct
{
	// Asset type marker
	asset_type_t type;
	// Reference count, when zero the asset is unloaded
	i32 ref_count;
} asset_t;

// Specific asset data
typedef struct
{
	asset_t asset;

	u32 width, height;
	texture_t *texture;
} image_t;

// Asset hash entry
typedef struct
{
	char name[ASSET_NAME_LEN];
	asset_t *asset;
} asset_entry_t;
// Asset cache data structure
typedef struct
{
	// Hash map for asset lookup
	asset_entry_t hash_map[ASSET_HASH_LEN];
} asset_cache_t;

// Creates/destroys the asset cache
asset_cache_t* alloc_asset_cache();
void           free_asset_cache(asset_cache_t *cache);

image_t* get_image(asset_cache_t *cache, const char *file_name); 
void     free_asset(asset_cache_t *cache, asset_t *asset);

#endif