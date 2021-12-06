#include "file_reader.h"

struct disk_t* disk_open_from_file(const char* volume_file_name)
{
	if ( volume_file_name == NULL )
	{
		return NULL;
	}
	struct disk_t* disk = malloc( sizeof( *disk ) );
	if ( disk == NULL )
	{
		return NULL;
	}
	disk->fptr = fopen( volume_file_name, "rb" );
	if ( disk->fptr == NULL )
	{
		free( disk );
		return NULL;
	}
	return disk;
}

int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read)
{
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

