#include "assets.h"

// Max length, in bytes, that a filename can be 
#define ASSET_NAME_LEN	(512)
// Max length of the asset hash map
#define ASSET_HASH_LEN	(1024)
// Max length of the asset queue
#define ASSET_QUEUE_LEN	(512)

// 32bit FNV-1a hash
// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
static inline uint32_t FNV_hash_32(const char* str)
{
	// NOTE: These change based on the size of the output hash, see above webpage
	static const uint32_t magic_offset = 0x811c9dc5;
	static const uint32_t magic_prime = 16777619;

	uint32_t hash = magic_offset;
	while (*str != '\0')
	{
		hash ^= *str++;
		hash *= magic_prime;
	};       
	return hash;                                           
}  

// Loads an image and creates a texture
static bool load_image(image_t *image, const char *file_name)
{
	bool result = false;

	// Load the image data in RGBA format
	i32 w, h, c;
	u8 *data = stbi_load(file_name, &w, &h, &c, STBI_rgb_alpha);
	if (data)
	{
		// Create a texture handle
		// NOTE: There is no guarantee the texture is actually ready at this point!
		r2d_texture_t *texture = r2d_alloc_texture(w,h,data);
		if (texture)
		{	
			// Set the image data
			image->width = w;
			image->height = h;
			image->texture = texture;
			// Success!
			result = true;
		}
		// Free the loaded image data
		stbi_image_free(data);
	}
	return result;
};
// Frees image data
static void free_image(image_t *image)
{
	r2d_free_texture(image->texture);
};

static asset_t* asset_alloc(size_t size, asset_type_t type)
{
	// Allocate the asset memory
	asset_t *asset = (asset_t*) malloc(size);
	assert(asset != NULL);
	memset(asset, 0, size);
	// Set the type
	asset->type = type;
	// Set the state to none
	asset->state = ASSET_STATE_NONE;
	return asset;
};
static void free_asset(asset_t *asset)
{
	// Free based on type
	switch(asset->type)
	{
		case ASSET_NONE: break;
		case ASSET_IMAGE:
		{
			image_t *image = (image_t *) asset;
			free_image(image);
		} break;
	}
	// Free the struct itself
	free(asset);
};

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

	ticket_mtx_t mtx;

	u32 count;
	u32 head, tail;
	asset_entry_t *entries[ASSET_QUEUE_LEN];
} asset_queue_t;

static void enqueue_asset_entry(asset_queue_t *queue, asset_entry_t *entry)
{
	ticket_mtx_lock(&queue->mtx);
	{
		// If the entry will fit in the queue
		if ((queue->count + 1) < ASSET_QUEUE_LEN)
		{
			// Set the state to queued
			entry->asset->state = ASSET_STATE_QUEUED;
			// Put the entry at the end of the queue
			queue->tail = (queue->tail + 1) % ASSET_QUEUE_LEN;
			queue->entries[queue->tail] = entry;
			queue->count ++;
			// Wake up the load thread
			sem_post(&queue->sem);
		};
	}
	ticket_mtx_unlock(&queue->mtx);
};
static asset_entry_t* dequeue_asset_entry(asset_queue_t *queue)
{
	asset_entry_t *entry = NULL;
	ticket_mtx_lock(&queue->mtx);
	{
		// If there's anything in the queue
		if ((queue->count - 1) >= 0)
		{
			// Get the entry at the front of the queue
			entry = queue->entries[queue->head];
			queue->head = (queue->head + 1) % ASSET_QUEUE_LEN;

			queue->count --;
		};
	}
	ticket_mtx_unlock(&queue->mtx);
	return entry;
};

// Hash map structure
typedef struct
{
	asset_entry_t entries[ASSET_HASH_LEN];
} asset_hash_t;

static asset_entry_t* asset_hash_lookup(asset_hash_t *hash, const char *file_name)
{
	// Make sure the file name is the right size
	assert (strlen(file_name) < ASSET_NAME_LEN);
	// Get the hash of the file name
	const u32 name_hash = FNV_hash_32(file_name);
	// Get the initial index to check
	const u32 init_index = (name_hash % ASSET_HASH_LEN);

	u32 index = init_index;
	do 
	{
		// Get the current entry
		asset_entry_t *entry = (hash->entries + index);
		// If the entry has an asset
		if (entry->asset)
		{
			// Check if the asset name matches, return if true
			if (strcmp(entry->name, file_name) == 0)
				return entry;
		} else {
			// We found an empty entry, set the entry file name
			strcpy(entry->name, file_name);
			// Return the entry
			return entry;
		}
		// Go to the next entry
		index = (index + 1) % ASSET_HASH_LEN;
		// Continue until we get to the original hash entry
	} while(index != init_index);
	// No open slots and no matches, the hash list is full
	return NULL;
};

// Asset cache data structure
struct assets_t
{
	// Hash map for asset lookup
	asset_hash_t hash;
	// Queue for asset loading
	asset_queue_t load_queue;
	pthread_t load_thread;
};

static void* load_proc(void *data)
{
	// Get the queue
	asset_queue_t *queue = (asset_queue_t*) data;
	// So long as we don't have a termination signal
	while (!queue->done)
	{
		// If there's anything in the queue
		if (queue->count != 0)
		{
			// Get the head entry
			asset_entry_t *entry = dequeue_asset_entry(queue);
			assert (entry != NULL);
			// Get the data pointers
			asset_t *asset = entry->asset;
			const char *file_name = entry->name;
			// Load based on type
			switch (asset->type)
			{
				case ASSET_NONE: break;
				case ASSET_IMAGE:
				{
					image_t *image = (image_t *) asset;
					if (load_image(image, file_name))
					{
						asset->state = ASSET_STATE_LOADED;
					} else {
						asset->state = ASSET_STATE_FAILED;
					}
				};
			};
		};
		// Wait until the signal semaphore is signaled
		sem_wait(&queue->sem);
	};
	return NULL;
};
assets_t* alloc_assets()
{
	assets_t *assets = malloc(sizeof(assets_t));
	assert(assets != NULL);
	memset(assets, 0, sizeof(assets_t));

	// Create the load queue
	asset_queue_t *load_queue = &assets->load_queue;
	load_queue->head = 0; 
	load_queue->tail = (ASSET_QUEUE_LEN-1); 
	sem_init(&load_queue->sem, 0, ASSET_QUEUE_LEN);
	// Create the load thread
	pthread_create(&assets->load_thread, NULL, load_proc, load_queue);

	return assets;
};
void free_assets(assets_t *assets)
{
	asset_hash_t *hash = &assets->hash;
	asset_queue_t *load_queue = &assets->load_queue;

	// Set the termination signal 
	load_queue->done = true;
	// Wake up the load thread (if not already awake)
	sem_post(&load_queue->sem);
	// Join the load thread
	pthread_join(assets->load_thread, NULL);
	// Free the loaded assets
	for (u32 i = 0; i < ASSET_HASH_LEN; i++)
	{

		asset_entry_t *entry = hash->entries + i;
		if (entry->asset)
		{
			asset_t *asset = entry->asset;
			free_asset(asset);
		};
	};
	// Free the assets structure
	free(assets);
};

image_t* get_image_asset(assets_t *assets, const char *file_name)
{
	image_t *image = NULL;

	asset_hash_t *hash = &assets->hash;
	asset_queue_t *load_queue = &assets->load_queue;

	// Get the entry for this asset
	asset_entry_t *entry = asset_hash_lookup(hash, file_name);
	if (entry != NULL)
	{
		// If the entry is empty
		if (!entry->asset)
		{
			// Create a new image and set it as the asset for this entry
			image = (image_t*) asset_alloc(sizeof(image_t), ASSET_IMAGE); 
			entry->asset = (asset_t*) image;
			// Enqueue a load for it
			enqueue_asset_entry(load_queue, entry);
		}else {
			// Get the asset
			asset_t *asset = entry->asset;
			// Make sure it's an image
			if (asset->type == ASSET_IMAGE)
			{
				image = (image_t*) asset;
			}
		}
		// Increment the reference count and return the asset
		if (image)
		{
			image->asset.ref_count ++;
		}
	}
	return image;
};
void release_asset(assets_t *assets, asset_t *asset)
{
	// Decrement the reference count
	asset->ref_count --;
	// If it goes to zero, free the asset
	if (asset->ref_count <= 0)
	{
		free_asset(asset);
	}
};
void wait_for_asset(asset_t *assets, const asset_t *asset)
{
	// Spinlock until the asset is loaded or fails to load
	while (
		(asset->state != ASSET_STATE_LOADED) &&
		(asset->state != ASSET_STATE_FAILED))
	{
		_mm_pause();
	}
};