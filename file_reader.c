#include "file_reader.h"

struct disk_t* disk_open_from_file(const char* volume_file_name)
{
	if ( volume_file_name == NULL )
	{
//		perror("EFAULT");
		return NULL;
	}
	struct disk_t* disk = malloc( sizeof( *disk ) );
	if ( disk == NULL )
	{
		perror("ENOMEM");
		return NULL;
	}
	disk->fptr = fopen( volume_file_name, "rb" );
	if ( disk->fptr == NULL )
	{
//		perror("ENOENT");
		free( disk );
		return NULL;
	}
	return disk;
}

int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read)
{
	if ( pdisk == NULL || buffer == NULL || sectors_to_read == 0 )
	{
		return -1;
	}
	int read = 0;
	fseek( pdisk->fptr, first_sector << 9, SEEK_SET );
	char* temp = buffer;
	for( ; read < sectors_to_read; read++ )
	{
		fread( temp, 512, 1, pdisk->fptr );
		temp = temp + 512;
	}

	return read;
}
int disk_close(struct disk_t* pdisk)
{
	if( pdisk == NULL )
	{
		return 1;
	}
	fclose( pdisk->fptr );
	free(pdisk);
	return 0;
}

struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector)
{
	if( pdisk == NULL )
	{
		return NULL;
	}
	struct volume_t* vol = malloc( sizeof( *vol ) );
	if( vol == NULL )
	{
		return NULL;
	}
	vol->super = malloc( 512 );
	if ( vol->super == NULL )
	{
		free( vol );
		return NULL;
	}
	disk_read( pdisk, first_sector, vol->super, 1 );
	if ( vol->super->reserved_sectors == 0 )
	{
		free( vol->super );
		free( vol );
		return NULL;
	}
	int any = 0;
	for( uint8_t i = 0; i <= 7; i++ )
	{
		if ( ( 1 << i ) == vol->super->sectors_per_cluster )
		{
			any = 1;
			break;
		}
	}
	if ( any == 0 )
	{
		free( vol->super );
		free( vol );
		return NULL;
	}
	vol->fat = calloc( 1, vol->super->bytes_pes_sector * vol->super->sectors_per_fat );
	if ( vol->fat == NULL)
	{
		free( vol->super );
		free( vol );
		return NULL;
	}
	vol->disk = pdisk;
	vol->first_fat_sector = vol->super->reserved_sectors;
	vol->first_root_sector = vol->first_fat_sector + vol->super->fat_count * vol->super->sectors_per_fat;
	vol->first_data_sector = vol->first_root_sector + vol->super->root_dir_capacity;
	vol->fat_size = vol->super->sectors_per_fat * vol->super->bytes_pes_sector;
	vol->first_data_sector = vol->first_root_sector + ( vol->super->root_dir_capacity * sizeof( struct root_dir_t ) / 512 );

	disk_read( pdisk, vol->first_fat_sector, vol->fat, vol->super->sectors_per_fat );

	return vol;
}
int fat_close(struct volume_t* pvolume)
{
	if ( pvolume == NULL )
	{
		return -1;
	}
	free( pvolume->super );
	free( pvolume->fat );
	free( pvolume );
	return 0;
}

struct file_t* file_open(struct volume_t* pvolume, const char* file_name)
{
	if ( pvolume == NULL || file_name == NULL )
	{
		return NULL;
	}

	char sector[512];
	disk_read( pvolume->disk, pvolume->first_root_sector, sector, 1 );
	struct root_dir_t root;
	int found = 0;
	for( int i = 0; i < 16; i++ )
	{
		memcpy( &root, sector + ( i << 5 ), sizeof( root ) );
		if ( *root.filename == 0xe5 )
		{
			continue;
		}
		char name[13];
		extract_name( ( char* )root.filename, name, root.attrib & 0x10 );
		if ( strcmp( name, file_name ) == 0 )
		{
			found = 1;
			break;
		}
	}
	if ( found == 0 )
	{
		return NULL;
	}

	struct file_t* file = malloc( sizeof( *file ) );
	if ( file == NULL )
	{
		return NULL;
	}
	uint32_t first = root.low_order + ( root.high_order << 16 );
	file->chain = get_chain_fat12( pvolume->fat, pvolume->fat_size, first );
	if ( file->chain == NULL )
	{
		free( file );
		return NULL;
	}
	file->vol = pvolume;
	file->pos = 0;
	return file;
}



int file_close(struct file_t* stream)
{
	if ( stream == NULL )
	{
		return -1;
	}
	free( stream->chain->clusters );
	free( stream->chain );
	free( stream );
	return 0;
}
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream)
{
	if ( ptr == NULL || nmemb == 0 || size == 0 || stream == NULL )
	{
		return 0;
	}
	size_t read, to_read;
	read = 0; // in bytes
	to_read = size * nmemb;
	char* out = ptr;
	int times = stream->vol->super->sectors_per_cluster;
	int i = 1;
	uint32_t read_sector = stream->vol->first_data_sector + ( (int)*stream->chain->clusters - 2) * stream->vol->super->sectors_per_cluster;
	while ( 1 )
	{
		if ( times == 0 )
		{
			read_sector = stream->vol->first_data_sector + ( *( stream->chain->clusters + i++ ) - 2 ) * stream->vol->super->sectors_per_cluster;
			times = 2;
		}
		if ( to_read == 0 )
		{
			break;
		}
		char sector[512] = { 0 };
		int res = disk_read( stream->vol->disk, (int)read_sector, sector, 1 );
		if ( res == -1 )
		{
			break;
		}
		times--;
		if ( to_read < 512 )
		{
			read += to_read;
			memcpy( out, sector, to_read );
			to_read = 0;
		}
		else
		{
			read += 512;
			memcpy( out, sector, 512 );
			out += 512;
			to_read -= 512;
			read_sector++;
		}
	}

	return read / size;
}
int32_t file_seek(struct file_t* stream, int32_t offset, int whence)
{
	if ( stream || offset || whence )
	{}

	return 0;
}

void extract_name( const char* src, char* dest, int is_dir )
{
	int len = 11;
	for ( int i = 0; i < 10; i++ )
	{
		if( *( src + i ) == ' ' )
		{
			len--;
		}
	}

	*dest = *src;
	int k = 1;
	for ( int i = 0; k < len; i++ )
	{
		if ( *( src + i + 1) != ' ')
		{
			*( dest + k++ ) = *( src + i + 1 );
		}
	}
	if ( !is_dir )
	{
		for (int i = 0; i < 3; i++)
		{
			*( dest + len - i ) = *( dest + len - i - 1 );
		}
		*( dest + len - 3 ) = '.';
		*( dest + len + 1 ) = '\0';
	}
	else
		*( dest + len ) = '\0';
}

struct clusters_chain_t* get_chain_fat12(const void * buffer, size_t size, uint32_t first_cluster)
{
	if ( buffer == NULL || size == 0 )
	{
		return NULL;
	}
	struct clusters_chain_t* chain = malloc( sizeof( *chain ) );
	if ( chain == NULL )
	{
		return NULL;
	}
	chain->clusters = malloc( sizeof( uint32_t ) );
	if ( chain->clusters == NULL )
	{
		free( chain );
		return NULL;
	}
	*chain->clusters = first_cluster;
	chain->size = 1;
	const uint8_t* fat = buffer;
	size_t pos = (size_t)( first_cluster + (size_t)( first_cluster >> 1 ) );
	uint32_t offset = first_cluster;
	while ( 1 )
	{
		if ( pos >= size )
		{
			break;
		}
		uint16_t one, two, val;
		one = *(fat + (int)( offset + (offset >> 1) ) );
		two = *(fat + (int)( offset + (offset >> 1) ) + 1);

		if (offset % 2 == 0)
		{
			val = two & 0xf;
			val = (val << 8) + one;
		}
		else
		{
			val = two << 4;
			val += (one >> 4);
		}
		if ( val == 0xfff || val == 0xff8 )
		{
			break;
		}
		if ( val == 0x0 )
		{
			fat++;
			pos += 2;
			continue;
		}

		uint32_t* temp = realloc( chain->clusters, sizeof( uint32_t ) * ( chain->size + 1 ) );
		if ( temp == NULL )
		{
			free( chain->clusters );
			free( chain );
			return NULL;
		}
		chain->clusters = temp;
		*( chain->clusters + chain->size ) = val;
		chain->size++;
		offset = val;
		pos = val + ( val >> 1 );
	}
	return chain;
}

struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path)
{
	if ( pvolume || dir_path )
	{}

	return NULL;
}
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry)
{
	if ( pdir || pentry )
	{}

	return 0;
}
int dir_close(struct dir_t* pdir)
{
	if ( pdir )
	{}

	return 0;
}



