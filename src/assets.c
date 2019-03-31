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

static image_t* image_load(const char *file_name)
{
	image_t *image = NULL;

	i32 w, h, c;
	u8 *data = stbi_load(file_name, &w, &h, &c, STBI_rgb_alpha);
	if (data)
	{
		texture_t *texture = texture_alloc(w,h,data);
		if (texture)
		{
			image = (image_t*) malloc(sizeof(image_t));
			assert(image != NULL);
			memset(image, 0, sizeof(image_t));
			
			image->asset.type = ASSET_IMAGE;
			image->asset.ref_count = 1;
			
			image->width = w;
			image->height = h;
			image->texture = texture;
		}
		stbi_image_free(data);
	}
	return image;
};
static void image_free(image_t *image)
{
	texture_free(image->texture);
	free(image);
};

static void asset_free(asset_t *asset)
{
	switch(asset->type)
	{
		case ASSET_NONE: break;
		case ASSET_IMAGE:
		{
			image_t *image = (image_t *) asset;
			image_free(image);
		} break;
	}
};

static asset_entry_t* asset_hash_lookup(assets_t *assets, const char *file_name)
{
	assert (strlen(file_name) < ASSET_NAME_LEN);
	
	const u32 hash = FNV_hash_32(file_name);
	const u32 init_index = (hash % ASSET_HASH_LEN);

	u32 index = init_index;
	do 
	{
		asset_entry_t *entry = (assets->hash_map + index);
		if (entry->asset)
		{
			if (strcmp(entry->name, file_name) == 0)
				return entry;
		} else {
			return entry;
		}

	} while(index != init_index);
	return NULL;
};

assets_t* assets_alloc()
{
	assets_t *assets = malloc(sizeof(assets_t));
	assert(assets != NULL);
	memset(assets, 0, sizeof(assets_t));
	return assets;
};
void assets_free(assets_t *assets)
{
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

	asset_entry_t *entry = asset_hash_lookup(assets, file_name);
	if (entry != NULL)
	{
		if (!entry->asset)
		{
			image = image_load(file_name);
			if (image)
			{
				entry->asset = (asset_t*) image;
			}
		}else {
			asset_t *asset = entry->asset;
			if (asset->type == ASSET_IMAGE)
			{
				asset->ref_count ++;
				image = (image_t*) asset;
			}
		}
	}
	return image;
}; 
void assets_release(assets_t *assets, asset_t *asset)
{
	asset->ref_count --;
	if (asset->ref_count == 0)
	{
		asset_free(asset);
	}
};
