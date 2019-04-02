#ifndef ASSETS_H
#define ASSETS_H

#include <pthread.h>
#include <semaphore.h>

#include <stb_image.h>

#include "core.h"
#include "render2d.h"

// General asset header data
typedef enum
{
	ASSET_NONE,
	ASSET_IMAGE,
} asset_type_t;
typedef enum
{
	ASSET_STATE_NONE,
	ASSET_STATE_QUEUED,
	ASSET_STATE_LOADED,
	ASSET_STATE_FAILED,
} asset_state_t;
typedef struct
{
	// Asset type marker
	asset_type_t type;
	// Asset state marker
	asset_state_t state;
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
	r2d_texture_t *texture;
} image_t;

// Declare the asset cache structure
decl_struct(assets_t);

// Creates/destroys the asset cache
assets_t* alloc_assets();
void      free_assets(assets_t *assets);

// Gets an image asset handle from the asset cache
image_t*  get_image_asset(assets_t *assets, const char *file_name);

// Returns an asset to the cache 
void release_asset(assets_t *assets, asset_t *asset);
// Wait until an asset is completely loaded
// NOTE: Blocking! Don't use unless completely necessary
void wait_for_asset(asset_t *assets, const asset_t *asset);

#endif