#pragma once

#include <codewrapper/codewrapper.hpp> 
#include <minerva/minerva.hpp>

#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <dirent.h>
#include <string>


static const std::string minervafs_root_folder = "/.minervafs";
static const std::string minervafs_basis_folder = "/.basis/";
static const std::string minervafs_registry = "/.registry/";
static const std::string minervafs_identifier_register = "/identifiers";//"/.identifiers";
static const std::string minervafs_config = "/.minervafs_config";
static const std::string minervafs_temp = "/.temp"; // For temporarly decode files

static std::string USER_HOME = "";

static minerva::minerva minerva_storage;

static minerva::file_format used_file_format = minerva::file_format::JSON;

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

/*static*/ int minerva_release(const char* path, struct fuse_file_info *fi); 

// Helper functions



void setup();

///
/// @param path const char* is path provide from the fuse file system impl
/// @return a path correct for the minerva folder 
/// 
std::string get_minerva_path(const char* path);

std::string get_user_home();

time_t get_mtime(const std::string path);

codes::code_params get_code_params(size_t file_size);

codes::code_params extract_code_params(nlohmann::json config);

void set_file_format(minerva::file_format file_format);

bool temp_file_exists(const std::string& filename);
