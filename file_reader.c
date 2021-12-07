#include "file_reader.h"

struct disk_t* disk_open_from_file(const char* volume_file_name)
{
	if ( volume_file_name == NULL )
	{
		perror("EFAULT");
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
		perror("ENOENT");
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
	fseek( pdisk->fptr, 0 + ( first_sector << 9 ), SEEK_SET );
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
	struct file_t* file = malloc( sizeof( *file ) );
	if ( file == NULL )
	{
		return NULL;
	}
	char sector[512];
	disk_read( pvolume->disk, pvolume->first_root_sector, sector, 1 );
	struct root_dir_t root;
//	int found = 0;
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
//			found = 1;
			break;
		}
	}

	return file;
}
int file_close(struct file_t* stream)
{
	if ( stream )
	{}

	return 0;
}
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream)
{
	if ( ptr || size || nmemb || stream )
	{}

	return 0;
}
int32_t file_seek(struct file_t* stream, int32_t offset, int whence)
{
	if ( stream || offset || whence )
	{}

	return 0;
}

void extract_name( char* src, char* dest, int is_dir )
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



