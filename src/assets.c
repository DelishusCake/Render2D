#include "assets.h"

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
static bool image_load(image_t *image, const char *file_name)
{
	bool result = false;

	// Load the image data in RGBA format
	i32 w, h, c;
	u8 *data = stbi_load(file_name, &w, &h, &c, STBI_rgb_alpha);
	if (data)
	{
		// Create a texture handle
		// NOTE: There is no guarantee the texture is actually ready at this point!
		texture_t *texture = texture_alloc(w,h,data);
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
static void image_free(image_t *image)
{
	texture_free(image->texture);
};

static asset_t* asset_alloc(size_t size, asset_type_t type)
{
	// Allocate the asset memory
	asset_t *asset = (asset_t*) malloc(size);
	assert(asset != NULL);
	memset(asset, 0, size);
	// Set the type
	asset->type = type;
	// Set the ref count to 1
	asset->ref_count = 1;
	// Set the state to none
	asset->state = ASSET_STATE_NONE;
	return asset;
};
static void asset_free(asset_t *asset)
{
	// Free based on type
	switch(asset->type)
	{
		case ASSET_NONE: break;
		case ASSET_IMAGE:
		{
			image_t *image = (image_t *) asset;
			image_free(image);
		} break;
	}
	// Free the struct itself
	free(asset);
};

static void enqueue_asset_entry(asset_queue_t *queue, asset_entry_t *entry)
{
	// If the entry will fit in the queue
	if ((queue->count + 1) < ASSET_QUEUE_LEN)
	{
		// Set the state to queued
		entry->asset->state = ASSET_STATE_QUEUED;
		// Put the entry at the end of the queue
		queue->tail = (queue->tail + 1) % ASSET_QUEUE_LEN;
		queue->entries[queue->tail] = entry;
		// Atomic increment the count
		atomic_inc(&queue->count);
		// Wake up the load thread
		sem_post(&queue->sem);
	};
};
static asset_entry_t* dequeue_asset_entry(asset_queue_t *queue)
{
	// If there's anything in the queue
	asset_entry_t *entry = NULL;
	if ((queue->count - 1) >= 0)
	{
		// Get the entry at the front of the queue
		entry = queue->entries[queue->head];
		queue->head = (queue->head + 1) % ASSET_QUEUE_LEN;
		// Atomic decrement the count
		atomic_dec(&queue->count);
	};
	return entry;
};

static asset_entry_t* asset_hash_lookup(assets_t *assets, const char *file_name)
{
	// Make sure the file name is the right size
	assert (strlen(file_name) < ASSET_NAME_LEN);
	// Get the hash of the file name
	const u32 hash = FNV_hash_32(file_name);
	// Get the initial index to check
	const u32 init_index = (hash % ASSET_HASH_LEN);

	u32 index = init_index;
	do 
	{
		// Get the current entry
		asset_entry_t *entry = (assets->hash_map + index);
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
					if (image_load(image, file_name))
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
assets_t* assets_alloc()
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
void assets_free(assets_t *assets)
{
	// Set the termination signal 
	asset_queue_t *load_queue = &assets->load_queue;
	load_queue->done = true;
	// Wake up the load thread (if not already awake)
	sem_post(&load_queue->sem);
	// Join the load thread
	pthread_join(assets->load_thread, NULL);
	// Free the loaded assets
	for (u32 i = 0; i < ASSET_HASH_LEN; i++)
	{
		asset_entry_t *entry = assets->hash_map + i;
		if (entry->asset)
		{
			asset_t *asset = entry->asset;
			asset_free(asset);
		};
	};
	// Free the assets structure
	free(assets);
};

image_t* assets_get_image(assets_t *assets, const char *file_name)
{
	image_t *image = NULL;

	// Get the entry for this asset
	asset_entry_t *entry = asset_hash_lookup(assets, file_name);
	if (entry != NULL)
	{
		// If the entry is empty
		if (!entry->asset)
		{
			// Create a new image and set it as the asset for this entry
			image = (image_t*) asset_alloc(sizeof(image_t), ASSET_IMAGE); 
			entry->asset = (asset_t*) image;
			// Enqueue a load for it
			asset_queue_t *load_queue = &assets->load_queue;
			enqueue_asset_entry(load_queue, entry);
		}else {
			// Get the asset
			asset_t *asset = entry->asset;
			// Make sure it's an image
			if (asset->type == ASSET_IMAGE)
			{
				// Increment the reference count and return the asset
				asset->ref_count ++;
				image = (image_t*) asset;
			}
		}
	}
	return image;
};
void assets_release(assets_t *assets, asset_t *asset)
{
	// Decrement the reference count
	asset->ref_count --;
	// If it goes to zero, free the asset
	if (asset->ref_count <= 0)
	{
		asset_free(asset);
	}
};
void assets_wait_for(asset_t *assets, const asset_t *asset)
{
	// Spinlock until the asset is loaded or fails to load
	while (
		(asset->state != ASSET_STATE_LOADED) ||
		(asset->state != ASSET_STATE_FAILED))
	{
		_mm_pause();
	}
};

u8* load_entire_file(const char *file_name, size_t *size)
{
	u8* buffer = NULL;

	FILE *f = fopen(file_name, "rb");
	if (f)
	{
		fseek(f, 0, SEEK_END);
		const size_t f_size = ftell(f);
		fseek(f, 0, SEEK_SET);

		buffer = malloc((f_size+1)*sizeof(u8));
		assert(buffer != NULL);
		fread(buffer, sizeof(u8), f_size, f);
		fclose(f);

		buffer[f_size] = '\0';

		if (size) *size = f_size;
	};
	return buffer;
};