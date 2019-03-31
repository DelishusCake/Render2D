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
} assets_t;

// Creates/destroys the asset cache
assets_t* assets_alloc();
void      assets_free(assets_t *assets);

image_t*  assets_get_image(assets_t *assets, const char *file_name); 
void      assets_release(assets_t *assets, asset_t *asset);

#endif