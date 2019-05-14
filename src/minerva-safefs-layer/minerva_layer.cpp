#include "minerva_layer.hpp"

#include <pwd.h>
#include <unistd.h>

#include <cstring> 
#include <string>

#include <vector>

#include <filesystem>

#include <iostream> // REMEMBER TO REMOVE 

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



void setup_paths()
{
    const std::string MKDIR = "mkdir";
    const std::string TOUCH = "touch";

    if (!std::filesystem::exists(USER_HOME + minervafs_root_folder))
    {
        std::system((MKDIR + " " + USER_HOME + minervafs_root_folder).c_str());
    }

    if (!std::filesystem::exists(USER_HOME + minervafs_root_folder + minervafs_basis_folder))
    {
        std::system((MKDIR + " " + USER_HOME + minervafs_root_folder + minervafs_basis_folder).c_str());
    }

    if (!std::filesystem::exists(USER_HOME + minervafs_root_folder + minervafs_registry))
    {
        std::system((MKDIR + " " + USER_HOME + minervafs_root_folder + minervafs_registry).c_str());
    }    
}

///
/// @param path const char* is path provide from the fuse file system impl
/// @return a path correct for the minerva folder 
/// 
std::string get_minerva_path(const char* path)
{
    std::string internal_path(path);
    return USER_HOME + minervafs_root_folder + "/" + internal_path.substr(1);
}

std::string get_user_home()
{
    char* homedir;
    if ((homedir = getenv("HOME")) != NULL)
    {
        homedir = getpwuid(getuid())->pw_dir;
    }

    USER_HOME = std::string(homedir, strlen(homedir));
}
