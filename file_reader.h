#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef FILE_READER_H
#define FILE_READER_H

int bits_per_fat_entry = (235983 % 2 == 1) ? 12 : 16;

struct __attribute__((packed)) boot_t {
	uint8_t  boot_code[3];
	uint8_t  OEM[8];
	uint16_t sector_size;
	uint8_t  cluster_size;
	uint16_t rev_size;
	uint8_t  nFATs;
	uint16_t max_files_root_dir;
	uint16_t nSectors_FS1;
	uint8_t  mediaType;
	uint16_t sizeofFAT; // in sectors
	uint16_t sectorsPerTrack;
	uint16_t nHeads;
	uint32_t nHeads_bef_start_partition;
	uint32_t nSectors_FS2;
};

struct __attribute__((packed)) super_t {
	struct boot_t basicBSector;
	uint8_t biosIntDriveNum;
	uint8_t notUsed1;
	uint8_t extBootSign;
	uint32_t volSerialNum;
	uint8_t volLabel[11];
	uint8_t FSLabel[8];
	uint8_t notUsed2[448];
	uint16_t signVal;
};

struct disk_t{
	FILE* fptr;
	struct super_t super;
};
struct volume_t{
	struct super_t* super;

	struct disk_t* disk;
	uint8_t* fat;

	uint16_t total_sectors;
	int32_t fat_size;
	uint16_t root_dir_sectors;
	uint16_t first_data_sector;
	uint16_t first_fat_sector;
	uint32_t data_sectors;
	uint32_t total_clusters;
};
struct file_t{

};
struct dir_t{

};

struct disk_t* disk_open_from_file(const char* volume_file_name);
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read);
int disk_close(struct disk_t* pdisk);

struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector);
int fat_close(struct volume_t* pvolume);

struct file_t* file_open(struct volume_t* pvolume, const char* file_name);
int file_close(struct file_t* stream);
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream);
int32_t file_seek(struct file_t* stream, int32_t offset, int whence);

struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path);
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry);
int dir_close(struct dir_t* pdir);

#endif //FILE_READER_H
