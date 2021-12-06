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
	if ( pdisk || first_sector || buffer || sectors_to_read )
	{}

	return 2;
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
	if( pdisk || first_sector )
	{}
	return NULL;
}
int fat_close(struct volume_t* pvolume)
{
	if ( pvolume )
	{}

	return 0;
}

struct file_t* file_open(struct volume_t* pvolume, const char* file_name)
{
	if ( pvolume || file_name )
	{}

	return NULL;
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



