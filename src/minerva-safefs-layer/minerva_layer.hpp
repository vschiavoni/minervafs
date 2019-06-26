#pragma once

#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <dirent.h>
#include <string>

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

/*static*/ void* minerva_init(struct fuse_conn_info *conn);

// Base operation

/*static*/ int minerva_getattr(const char* path, struct stat* stbuf);

/*static*/ int minerva_fgetattr(const char *path, struct stat *stbuf,
                            struct fuse_file_info *fi);

/*static*/ int minerva_access(const char* path, int mask);

/*static*/ int minerva_create(const char* path, mode_t mode, struct fuse_file_info * fi);

/*static*/ int minerva_open(const char* path, struct fuse_file_info* fi);

/*static*/ int minerva_read(const char *path, char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi);

/*static*/ int minerva_write(const char* path, const char *buf, size_t size, off_t offset,
                         struct fuse_file_info* fi);

/*static*/ int minerva_truncate(const char *path, off_t size);

// Make items
/*static*/ int minerva_mknod(const char *path, mode_t mode, dev_t rdev);

/*static*/ int minerva_mkdir(const char *path, mode_t mode);

/*static*/ int minerva_releasedir(const char *path, struct fuse_file_info *fi);

// Directory operations

/*static*/ struct minerva_dirp *get_dirp(struct fuse_file_info *fi);

/*static*/ int minerva_opendir(const char *path, struct fuse_file_info *fi);

/*static*/ int minerva_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                           off_t offset, struct fuse_file_info *fi);

/*static*/ int minerva_flush(const char* path, struct fuse_file_info* fi);

/*static*/ int minerva_release(const char* path, struct fuse_file_info *fi);

/*static*/ int minerva_rename(const char* from, const char* to);

/*static*/ int minerva_unlink(const char* path);



//TODO getxattr
//TODO setattr
//TODO setxattr

// Helper function
void set_file_format(int file_format);
