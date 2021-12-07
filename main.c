#include <stdio.h>
#include "file_reader.h"

int main()
{
	struct disk_t* disk = disk_open_from_file("fat12_volume.img");
	struct volume_t* vol = fat_open( disk, 0 );

	free(disk);
	free(vol);
	return 0;
}