#pragma once

#define FUSE_USER_VERSION 26

// Filesystem includes
#include <fuse.h>
#include <dirent.h>

#include <string>

static const std::string minervafs_root_folder = "/.minervafs";
static const std::string minervafs_basis_folder = "/.basis";
static const std::string minervafs_registry = "/.registry";

std::string USER_HOME;

// Directory pointer for point to a given directory
// or an entry within 
struct minerva_dirp
{
    DIR* dp;
    struct dirent* entry;
    off_t offset;
};

static int minerva_init(struct fuse_operations** fuse_operations);

// Base operation

static int minerva_getattr(const char* path, struct stat* stbuf);

static int minerva_fgetattr(const char *path, struct stat *stbuf,
                            struct fuse_file_info *fi);

static int minerva_access(const char* path, int mask);

static int minerva_open(const char* path, struct fuse_file_info* fi);

static int minerva_read(const char *path, char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi);

static int minerva_write(const char* path, const char *buf, size_t size, off_t offset,
                         struct fuse_file_info* fi);

// Make items
static int minerva_mknod(const char *path, mode_t mode, dev_t rdev);

static int minerva_mkdir(const char *path, mode_t mode);

static int minerva_releasedir(const char *path, struct fuse_file_info *fi);

// Directory operations

static inline struct minerva_dirp *get_dirp(struct fuse_file_info *fi);

static int minerva_opendir(const char *path, struct fuse_file_info *fi);

static int minerva_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                           off_t offset, struct fuse_file_info *fi);

// Helper functions



void setup_paths();

///
/// @param path const char* is path provide from the fuse file system impl
/// @return a path correct for the minerva folder 
/// 
std::string get_minerva_path(const char* path);

std::string get_user_home();
