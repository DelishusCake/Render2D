#ifndef ASSETS_H
#define ASSETS_H

#include <stb_image.h>

#include "core.h"

// Max length, in bytes, that a filename can be 
#define ASSET_NAME_LEN	(512)
// Max length of the asset hash map
#define ASSET_HASH_LEN	(1024)

// Platform specific assets/functions
// NOTE: For internal use only
decl_struct(texture_t);
texture_t* texture_alloc(u32 width, u32 height, u8 *pixels);
void       texture_free(texture_t *texture);

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
	// Asset header
	asset_t asset;
	// Asset data
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
asset_cache_t* asset_cache_alloc();
void           asset_cache_free(asset_cache_t *cache);

image_t* asset_cache_get_image(asset_cache_t *cache, const char *file_name); 
void     asset_cache_release(asset_cache_t *cache, asset_t *asset);

#endif