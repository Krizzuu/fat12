#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <memory.h>
#include <ctype.h>

#ifndef FILE_READER_H
#define FILE_READER_H


struct __attribute__((packed)) super_t {
	uint8_t jump_code[3];
	char oem_name[8];
	uint16_t bytes_pes_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sectors;
	uint8_t fat_count;
	uint16_t root_dir_capacity;
	uint16_t logical_sectors16;
	uint8_t media_type;
	uint16_t sectors_per_fat;
	uint16_t chs_tracks_per_cylinder;
	uint32_t hidden_sectors;
	uint32_t logical_sectors32;
	uint8_t media_id;
	uint8_t chs_head;
	uint8_t ext_bpb_signature;
	uint32_t serial_number;
	char volume_label[11];
	char fsid[8];
	uint8_t boot_code[448];
	uint16_t magic;
};

struct disk_t{
	FILE* fptr;
};
struct volume_t{
	struct super_t* super;

	struct disk_t* disk;
	uint8_t* fat;

	uint16_t total_sectors;
	int32_t fat_size;
	uint16_t first_data_sector;
	uint16_t first_fat_sector;
	uint16_t first_root_sector;
	uint32_t data_sectors;
	uint32_t total_clusters;
};

struct clusters_chain_t{
	uint32_t *clusters;
	size_t size;
};

struct file_t{
	struct clusters_chain_t* chain;
	struct volume_t* vol;
	uint32_t pos;
	uint32_t size;
};

struct __attribute__((__packed__)) root_dir_t  {
	uint8_t filename[11];
	uint8_t attrib;
	uint8_t reserved;
	uint8_t file_creation_time;
	uint16_t creation_time;
	uint16_t creation_date;
	uint16_t access_date;
	uint16_t high_order;
	uint16_t mod_time;
	uint16_t mod_date;
	uint16_t low_order;
	uint32_t file_size;
};

struct dir_t{
	struct volume_t* vol;
	uint32_t pos;

	int is_root;
	struct clusters_chain_t* chain;
};
struct dir_entry_t{
	char name[13];
	size_t size;
	uint8_t is_archived;
	uint8_t is_readonly;
	uint8_t is_system;
	uint8_t is_hidden;
	uint8_t is_directory;
	struct creation_date_t
	{
		uint8_t  day;
		uint8_t	 month;
		uint16_t year;
	} creation_date;
	struct creation_time_t
	{
		uint8_t hour;
		uint8_t minute;
		uint8_t second;
	} creation_time;

	uint32_t first_cluster;
};

union date_t
{
	uint16_t val;
	struct date_bits_t
	{
		uint16_t day:5;
		uint16_t month:4;
		uint16_t year:7;
	} date_bits;
};
union time_t
{
	uint16_t val;
	struct time_bits_t
	{
		uint16_t seconds:5;
		uint16_t minutes:6;
		uint16_t hour:5;
	} time_bits;
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

void extract_name( const char* src, char* dest, int is_dir );
struct clusters_chain_t* get_chain_fat12(const void * buffer, size_t size, uint32_t first_cluster);

struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path);
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry);
int dir_close(struct dir_t* pdir);

char** path_to_names( const char* path, int* err);
void destroy_names( char** names );

int find_entry( struct volume_t* pvolume, struct root_dir_t *root, const char* path );
#endif //FILE_READER_H
