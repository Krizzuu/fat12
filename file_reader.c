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

//	char sector[512];
	struct root_dir_t root;
	int res = find_entry( pvolume, &root, file_name );
	if ( res != 0 )
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
	file->size = root.file_size;
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
		return -1;
	}
	if ( stream->pos >= stream->size )
	{
		return 0;
	}
	size_t read, to_read;
	read = 0; // in bytes
	to_read = size * nmemb;
	if ( to_read > stream->size )
	{
		to_read = stream->size;
	}
	char* out = ptr;
	int times = stream->vol->super->sectors_per_cluster;
	int i = 1;
	uint32_t read_sector = stream->vol->first_data_sector + ( (int)*stream->chain->clusters - 2) * stream->vol->super->sectors_per_cluster;
	uint32_t to_skip = stream->pos;
	uint32_t left = stream->size - stream->pos;
	while ( to_skip >= 512 )
	{
		if ( times == 0 )
		{
			read_sector = stream->vol->first_data_sector + ( *( stream->chain->clusters + i++ ) - 2 ) * stream->vol->super->sectors_per_cluster;
			times = stream->vol->super->sectors_per_cluster;
		}
		char sector[512] = { 0 };
		int res = disk_read( stream->vol->disk, (int)read_sector, sector, 1 );
		if ( res == -1 )
		{
			break;
		}
		to_skip -= 512;
		times--;
		read_sector++;
	}
	while ( 1 )
	{
		if ( times == 0 )
		{
			read_sector = stream->vol->first_data_sector + ( *( stream->chain->clusters + i++ ) - 2 ) * stream->vol->super->sectors_per_cluster;
			times = stream->vol->super->sectors_per_cluster;
		}
		if ( to_read == 0 )
		{
			break;
		}
		char sector[512] = { 0 };
		int res = disk_read( stream->vol->disk, (int)read_sector, sector, 1 );
		if ( res == -1 )
		{
			return -1;
		}
		times--;
		if ( to_read < 512 )
		{
			uint32_t reread = 0;
			uint32_t count = to_read > left ? left : to_read ;
			if ( 512 - to_skip < count )
			{
				reread = count;
				count = 512 - to_skip;
				reread -= count;
			}
			read += count;
			memcpy( out, sector + to_skip, count );
			out += count;
			stream->pos += count;
			to_read = 0;
			to_skip = 0;
			if( reread )
			{
				if( times == 0 )
				{
					read_sector = stream->vol->first_data_sector + ( *( stream->chain->clusters + i++ ) - 2 ) * stream->vol->super->sectors_per_cluster;
				}
				else
				{
					read_sector++;
				}
				res = disk_read( stream->vol->disk, (int)read_sector, sector, 1 );
				if ( res == -1 )
				{
					return -1;
				}
				memcpy( out, sector, reread );
				read += reread;
				stream->pos += reread;
			}
		}
		else
		{
			read += 512;
			memcpy( out, sector, 512 - to_skip );
			out += 512;
			stream->pos += to_read;
			to_read -= 512;
			read_sector++;
			left -= 512;
		}
	}

	return read / size;
}
int32_t file_seek(struct file_t* stream, int32_t offset, int whence)
{
	if ( stream == NULL )
	{
		return -1;
	}
	switch (whence)
	{
	case SEEK_SET:
		if( offset < 0 || (uint32_t)offset >= stream->size )
		{
			return -1;
		}
		stream->pos = offset;
		break;
	case SEEK_CUR:
		if( (int)stream->pos + offset < 0 || stream->pos + (uint32_t)offset >= stream->size )
		{
			return -1;
		}
		stream->pos += offset;
		break;
	case SEEK_END:
		if( (uint32_t)offset <= -stream->size || offset > 0 )
		{
			return -1;
		}
		stream->pos = stream->size + offset;
		break;
	default:
		return -1;
	}

	return (int)stream->pos;
}


void extract_name( const char* src, char* dest, int is_dir )
{
	if( !is_dir )
	{
		char name[9] = { 0 };
		char ext[4] = { 0 };
//		strncpy((char*)name, src, 8);
//		strncpy((char*)name, src + 8, 3);
		for( int i = 0; i < 8; i++ )
		{
			name[i] = src[i];
		}
		for( int i = 0; i < 3; i++ )
		{
			ext[i] = src[8+i];
		}
		for( int i = 0; i < 8; i++ )
		{
			while ( *( name + i ) == ' ' )
			{
				for ( int j = i; j < 8; j++ )
				{
					*( name + j ) = *( name + j + 1 );
				}
			}
		}
		for( int i = 0; i < 3; i++ )
		{
			while ( *( ext + i ) == ' ' )
			{
				for ( int j = i; j < 3; j++ )
				{
					*( ext + j ) = *( ext + j + 1 );
				}
			}
		}
		memcpy( dest, name, 9 );
		if( *ext )
		{
			uint offset = strlen(name);
			*(dest + offset) = '.';
			memcpy(dest + offset + 1, ext, 4);
		}

	}
	else
	{
		char name[13] = { 0 };
		strncpy( name, src, 11 );
		for( int i = 0; i < 13; i++ )
		{
			while ( *( name + i ) == ' ' )
			{
				for ( int j = i; j < 13; j++ )
				{
					*( name + j ) = *( name + j + 1 );
				}
			}
		}
		memcpy( dest, name, 13 );
	}
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
	if ( pvolume == NULL || dir_path == NULL )
	{
		return NULL;
	}

	struct dir_t* dir = malloc( sizeof( *dir ) );
	if ( dir == NULL )
	{
		return NULL;
	}
	dir->vol = pvolume;
	dir->pos = 0;
	return dir;
}
int dir_read(struct dir_t* pdir, struct dir_entry_t* entry)
{
	if ( pdir == NULL || entry == NULL )
	{
		return -1;
	}
	struct root_dir_t root;
	char sector[512];
	for ( uint i = 0; i < ( pdir->vol->super->root_dir_capacity * sizeof( root ) ) >> 9; i++ )
	{
		disk_read( pdir->vol->disk, pdir->vol->first_root_sector + (int)i, sector, 1 );
		for (int j = 0; j < 16; j++)
		{
			if ( i * 16 + j < pdir->pos )
			{
				continue;
			}
			memcpy(&root, sector + (j << 5), sizeof(root));
			if( *root.filename == 0x0 )
			{
				return 1;
			}
			if (*root.filename == 0xe5 )
			{
				continue;
			}
			char name[13] = { 0 };
			entry->is_directory = ( root.attrib & 0x10 ) != 0;
			extract_name((char*)root.filename, name, entry->is_directory );

			memcpy( entry->name, name, 13 );
			entry->size = root.file_size;
			entry->is_archived = ( root.attrib & 0x20 ) != 0;

			entry->is_system = ( root.attrib & 0x04 ) != 0;
			entry->is_hidden = ( root.attrib & 0x02 ) != 0;
			entry->is_readonly = ( root.attrib & 0x01 ) != 0;

			union date_t date;
			date.val = root.creation_date;
			union time_t time;
			time.val = root.creation_time;

			entry->creation_date.year = 1980 + date.date_bits.year;
			entry->creation_date.month = date.date_bits.month;
			entry->creation_date.day = date.date_bits.day;

			entry->creation_time.hour = time.time_bits.hour;
			entry->creation_time.minute = time.time_bits.minutes;
			entry->creation_time.second = time.time_bits.seconds;
			entry->first_cluster = ( root.high_order << 16 ) + root.low_order;

			pdir->pos = i * 16 + j + 1;

			return 0;
		}
	}

	return 1;
}
int dir_close(struct dir_t* pdir)
{
	if ( pdir == NULL )
	{
		return -1;
	}
//	free( pdir );
	free( pdir );
	return 0;
}

char** path_to_names( const char* path, int* err)
{
	// 8 MEMLACK  1 BAD DATA  0 SUCCESS
	char** names = NULL;
	int count = 1;
	while( *path )
	{
		char** tarr = realloc( names, sizeof( *names ) * ( count + 1 ) );
		if( tarr == NULL )
		{
			for( int i = 0; *(names + i); i++ )
			{
				free( *(names + i) );
			}
			free( names );
			*err = 8;
			return NULL;
		}
		names = tarr;
		*(names + count) = NULL;

		if ( *path == '\\' )
			path++;
		char* name = NULL;
		int len = 1;
		while( *path && *path != '\\' )
		{
			char* temp = realloc( name, len + 1 );
			if ( temp == NULL )
			{
				*err = 8;
				free( name );
				for( int i = 0; *(names + i); i++ )
				{
					free( *(names + i) );
				}
				free( names );
				return NULL;
			}
			name = temp;
			*( name + len ) = '\0';
			char ch = (char)toupper( *path );
			*( name + len - 1 ) = ch;
			path++;
			len++;
		}
		if( strcmp( name, "." ) == 0 )
		{
			free( name );
			continue;
		}
		else if ( strcmp( name, ".." ) == 0  )
		{
			free( name );
			if ( count == 1 )
			{
				free( names );
				*err = 1;
				return NULL;
			}
			free( *( names + count - 2) );
			count--;
			continue;
		}
		*( names + count++ - 1 ) = name;
	}
	return names;
}
void destroy_names( char** names )
{
	for( int i = 0; *(names + i); i++ )
	{
		free( *(names + i) );
	}
	free( names );
}

int find_entry( struct volume_t* pvolume, struct root_dir_t *out, const char* path)
{
	// 0 SUCCESS	1 WRONG INPUT	2 NO MEM
	int err;
	char** names = path_to_names( path, &err );
	if ( err == 8 )
	{
		return 2;
	}
	else if ( err == 1 )
	{
		return 1;
	}
	// ROOT DIR
	char sector[512];
	struct root_dir_t root;
	int found = 0;
	uint i;
	// SKIP DOTS or find error
	for( i = 0; *( names + i ); i++ )
	{
		if( *( names + i ) == NULL )
		{
			destroy_names( names );
			return 0;
		}
		else if ( strcmp( *( names + i ), "." ) == 0 )
		{
			continue;
		}
		else if ( strcmp( *( names + i ), ".." ) == 0 )
		{
			destroy_names( names );
			return 1;
		}
		break;
	}
	// SEARCH FOR ENTRY IN ROOT
	for ( uint j = 0; j < ( pvolume->super->root_dir_capacity * sizeof( root ) ) >> 9 && found == 0; j++ )
	{
		disk_read( pvolume->disk, pvolume->first_root_sector + (int)j, sector, 1 );
		for (int k = 0; k < 16; k++)
		{
			memcpy(&root, sector + (k << 5), sizeof(root));
			if( *root.filename == 0x0 )
			{
				break;
			}
			if (*root.filename == 0xe5 )
			{
				continue;
			}
			if( *( names + i + 1 ) != NULL && ( root.attrib & 0x10 ) != 0x10 )
			{
				continue;
			}
			char name[13];
			extract_name((char*)root.filename, name, root.attrib & 0x10);
			if ( strcmp( name, *(names + i) ) == 0 )
			{
				found = 1;
				break;
			}
		}
	}
	if ( found == 0 )
	{
		destroy_names( names );
		return 1;
	}
	if ( *( names + i + 1 ) == NULL )
	{
		*out = root;
		destroy_names( names );
		return 0;
	}
	if( ( root.attrib & 0x10 ) != 0x10 ) // check if can continue scanning / is even a dir
	{
		destroy_names( names );
		return 1;
	}
	// CONTINUE SEARCHING
	struct clusters_chain_t* chain = NULL;
	found = 1;
	while( *( names + i + 1 ) )
	{
		if ( chain ) free( chain->clusters );
		free( chain );
		if ( found == 0 )
		{
			destroy_names( names );
			free( chain->clusters );
			free( chain );
		}
		uint32_t first = root.low_order + ( root.high_order << 16 );
		chain = get_chain_fat12( pvolume->fat, pvolume->fat_size, first );
		uint32_t* temp = chain->clusters;
		i++;
		found = 0;
		for( uint j = 0; j < chain->size; j++ )
		{
			for( uint k = 0; k < pvolume->super->sectors_per_cluster; k++ )
			{
				disk_read(pvolume->disk, pvolume->first_data_sector + ( (int)*temp - 2 ) * pvolume->super->sectors_per_cluster + (int)k, sector, 1);
				for( uint m = 0; m < 16; m++ )
				{
					memcpy(&root, sector + (m << 5), sizeof(root));
					if( *root.filename == 0x0 )
					{
						break;
					}
					if (*root.filename == 0xe5 )
					{
						continue;
					}
					if( *( names + i + 1 ) != NULL && ( root.attrib & 0x10 ) != 0x10 )
					{
						continue;
					}
					char name[13];
					extract_name((char*)root.filename, name, root.attrib & 0x10);
					if ( strcmp( name, *(names + i) ) == 0 )
					{
						found = 1;
						break;
					}
				}
			}
			if ( found == 1 )
			{
				break;
			}
			temp++;
		}
	}
	free( chain->clusters );
	free( chain );
	*out = root;
	destroy_names( names );
	return 0;
}


