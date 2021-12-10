#include <stdio.h>
#include "file_reader.h"

int main()
{
	struct disk_t* disk = disk_open_from_file("chart.img");
	struct volume_t* vol = fat_open( disk, 0 );

	struct dir_t* dir = dir_open(vol,"\\");
	struct dir_entry_t entry;
	struct root_dir_t root;
	dir_read( dir, &entry );
	dir_read( dir, &entry );
	dir_read( dir, &entry );
	dir_read( dir, &entry );
	dir_read( dir, &entry );
	char sector[512];
	disk_read( disk, ( (int)entry.first_cluster - 2 ) * vol->super->sectors_per_cluster + vol->first_data_sector, sector, 1 );
	memcpy( &root, sector + 32, sizeof( root ) );
	disk_read( disk, root.low_order * vol->super->sectors_per_cluster, sector, 1 );

	disk_close( disk );
	fat_close( vol );
	dir_close( dir );
	return 0;
}