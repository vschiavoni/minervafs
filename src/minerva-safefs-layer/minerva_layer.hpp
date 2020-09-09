#pragma once

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 39
#endif

#include <fuse3/fuse.h>
#include <dirent.h>
#include <string>
#include <vector>

static short JSON = 0;
static short BSON = 1;
static short UBJSON = 2;
static short CBOR = 3;
static short MSGPACK = 4;

// Directory pointer for point to a given directory
// or an entry within
struct minerva_dirp
{
    DIR* dp;
    struct dirent* entry;
    off_t offset;
};

void* minerva_init(struct fuse_conn_info *conn, struct fuse_config *cfg);

/**
 * Cleans up the temporary directory when the program ends.
 */
void minerva_destroy(void *private_data);

// Base operation

int minerva_getattr(const char* path, struct stat* stbuf, struct fuse_file_info *fi);

int minerva_access(const char* path, int mask);

int minerva_create(const char* path, mode_t mode, struct fuse_file_info * fi);

int minerva_open(const char* path, struct fuse_file_info* fi);

int minerva_read(const char *path, char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi);

int minerva_write(const char* path, const char *buf, size_t size, off_t offset,
                         struct fuse_file_info* fi);

int minerva_truncate(const char *path, off_t size, struct fuse_file_info *fi);

int minerva_chmod(const char* path, mode_t mode, struct fuse_file_info *fi);

// Make items
int minerva_mknod(const char *path, mode_t mode, dev_t rdev);

int minerva_mkdir(const char *path, mode_t mode);

int minerva_releasedir(const char *path, struct fuse_file_info *fi);

// Directory operations

struct minerva_dirp *get_dirp(struct fuse_file_info *fi);

int minerva_opendir(const char *path, struct fuse_file_info *fi);

int minerva_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                           off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);

int minerva_rmdir(const char *path);

int minerva_flush(const char* path, struct fuse_file_info* fi);

int minerva_release(const char* path, struct fuse_file_info *fi);

int minerva_rename(const char* from, const char* to, unsigned int flags);

int minerva_unlink(const char* path);

int minerva_utimens(const char* path, const struct timespec tv[2], struct fuse_file_info *fi);

//TODO getxattr
//TODO setattr
//TODO setxattr

int minerva_listxattr(const char* path, char* list, size_t size);

// Helper function
void set_file_format(int file_format);


/**
 * Reads parts of a file limited by an offset and an number
 * @param   path    Path to the file
 * @param   offset  Start of the data to read
 * @param   size    Length of the data to read
 * @return  The data read from the file
 */
std::vector<uint8_t> decode(std::string path, off_t offset, size_t size);