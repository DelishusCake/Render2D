#ifndef ASSETS_H
#define ASSETS_H

#include <stb_image.h>

#include <pthread.h>
#include <semaphore.h>

#include "core.h"

// Max length, in bytes, that a filename can be 
#define ASSET_NAME_LEN	(512)
// Max length of the asset hash map
#define ASSET_HASH_LEN	(1024)
// Max length of the asset queue
#define ASSET_QUEUE_LEN	(512)

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
	texture_t *texture;
} image_t;

// Asset hash entry
typedef struct
{
	char name[ASSET_NAME_LEN];
	asset_t *asset;
} asset_entry_t;
// Asset queue structure
typedef struct
{
	sem_t sem;
	bool done;

	volatile u32 count;
	volatile u32 head, tail;
	asset_entry_t *entries[ASSET_QUEUE_LEN];
} asset_queue_t;
// Asset cache data structure
typedef struct
{
	// Hash map for asset lookup
	asset_entry_t hash_map[ASSET_HASH_LEN];
	// Queue for asset loading
	asset_queue_t load_queue;
	pthread_t load_thread;
} assets_t;

// Creates/destroys the asset cache
assets_t* assets_alloc();
void      assets_free(assets_t *assets);

// Gets an image asset handle from the asset cache
image_t*  assets_get_image(assets_t *assets, const char *file_name);

// Returns an asset to the cache 
void assets_release(assets_t *assets, asset_t *asset);
// Wait until an asset is completely loaded
// NOTE: Blocking! Don't use unless completely necessary
void assets_wait_for(asset_t *assets, const asset_t *asset);

// Directly load an entire file from the hard disk
u8* load_entire_file(const char *file_name, size_t *size);

#endif