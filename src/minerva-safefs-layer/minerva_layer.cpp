#include "minerva_layer.hpp"

#include <pwd.h>
#include <unistd.h>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <dirent.h>

#include <cstring> 
#include <string>

#include <vector>

#include <filesystem>

#include <iostream> // REMEMBER TO REMOVE 

/*static*/ void* minerva_init(struct fuse_conn_info *conn)
{
    (void) conn;
    USER_HOME = get_user_home();
    setup_paths();
    return 0;
}

// Base operation

/*static*/ int minerva_getattr(const char* path, struct stat* stbuf)
{
    int res = 0;

    std::string minerva_entry_path = get_minerva_path(path);
    memset(stbuf, 0, sizeof(struct stat));

    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();

    // If the entry actually exists we have to "fake" m_time
    if (std::filesystem::exists(minerva_entry_path))
    {
        stbuf->st_mtime = get_mtime(minerva_entry_path);
    }
    else
    {
        return -ENOENT;
    }
    
    if (strcmp(path, "/") == 0)
    {
        // TODO: Look at st mode
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    }
    else if (!std::filesystem::is_directory(minerva_entry_path))
    {
        stbuf->st_mode = S_IFREG | 0664;
        stbuf->st_nlink = 1;
        stbuf->st_size = std::filesystem::file_size(minerva_entry_path);            
    }
    else if (std::filesystem::is_directory(minerva_entry_path))
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 1;
        //stbuf->st_size = std::filesystem::file_size(minerva_entry_path); // TODO: Chekc if this is valid   
    }
    else
    {
        res = -ENOENT;
    }
    return res;
}

/*static*/ int minerva_fgetattr(const char *path, struct stat *stbuf,
                            struct fuse_file_info *fi)
{

    (void) fi; 
    std::string minerva_entry_path = get_minerva_path(path);    
    if (!std::filesystem::exists(minerva_entry_path))
    {
        return -ENOENT;
    }
        
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_mtime = get_mtime(minerva_entry_path);    
    stbuf->st_mode = S_IFREG | 0777;
    stbuf->st_nlink = 1;
    return 0;
}

/*static*/ int minerva_access(const char* path, int mask)
{
    (void) path;
    (void) mask;
    // TODO: update
    return 0;
    
}
    

/*static*/ int minerva_open(const char* path, struct fuse_file_info* fi)
{
    std::string minerva_entry_path = get_minerva_path(path);

    int res;
    res = open(minerva_entry_path.c_str(), fi->flags);
    if (res == -1)
    {
        return -errno;
    }
    close(res);
    return 0; 
}

/*static*/ int minerva_read(const char *path, char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi)
{
    std::string minerva_entry_path = get_minerva_path(path);
    int fd;
    int res;

    (void) fi;
    fd = open(minerva_entry_path.c_str(), O_RDONLY);

    if (fd == -1)
    {
        return -errno;
    }

    res = pread(fd, buf, size, offset);

    if (res == -1)
    {
        res = -errno;
    }

    close(fd);
    return res;
}


// TODO: Inject the usage of GDD 
/*static*/ int minerva_write(const char* path, const char *buf, size_t size, off_t offset,
                         struct fuse_file_info* fi)
{
    (void) offset; // write the whole thing

    // We convert the buffer to data we can use
    std::vector<uint8_t> data;
    data.assign(buf, buf+size); // we passe the data of buff to the data vector

    std::string minerva_entry_path = get_minerva_path(path);

    int fd;
    int res;

    (void) fi;
    fd = open(minerva_entry_path.c_str(), O_WRONLY);

    // If we are unable to open a file we return an error
    if (fd == -1)
    {
        return -errno;
    }

    // Right now we just make a pars throgh of data 
    res = pwrite(fd, buf, size, offset);
    if (res == -1)
    {
        res = -errno;
    }

    close(fd);
    return res;
}

// TODO: update as needed
/*static*/ int minerva_truncate(const char *path, off_t size)
{
    (void) path;
    (void) size;                    
    return 0;
}

// Make items
/*static*/ int minerva_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int res;

    std::string persistent_entry_path = get_minerva_path(path);

    // Check if the file is in a regular mode
    if (S_ISREG(mode))
    {
        res = open(persistent_entry_path.c_str(), O_CREAT | O_EXCL | O_WRONLY, mode);
        if (res >= 0)
        {
            res = close(res);
        }
    }
    else if (S_ISFIFO(mode))
    {
        res = mkfifo(persistent_entry_path.c_str(), mode);
    }
    else
    {
        res = mknod(persistent_entry_path.c_str(), mode, rdev);
    }
    
    if (res == -1)
    {
        return -errno;        
    }
    return 0;
}

/*static*/ int minerva_mkdir(const char *path, mode_t mode)
{
    std::string persistent_folder_path = get_minerva_path(path);
    if (mkdir(persistent_folder_path.c_str(), mode) == -1)
    {
        return -errno;
    }
    return 0;
}

/*static*/ int minerva_releasedir(const char *path, struct fuse_file_info *fi)
{
    struct minerva_dirp* dirp = get_dirp(fi);
    (void)path;
    // TODO: Implement the following properly
    closedir(dirp->dp);
    free(dirp);
    return 0;
}

// Directory operations
/*static*/ struct minerva_dirp* get_dirp(struct fuse_file_info *fi)
{
    return (struct minerva_dirp*)(uintptr_t)fi->fh;
}

// TODO: update to open sub dirs 
/*static*/ int minerva_opendir(const char *path, struct fuse_file_info *fi)
{
    int res;
    struct minerva_dirp* dirp = (minerva_dirp*) malloc(sizeof(struct minerva_dirp));
    if (dirp == NULL)
    {
        return -ENOMEM;
    }

    dirp->dp = opendir(path);
    if (dirp->dp == NULL)
    {
        res = -errno;
        free(dirp);
        return res;
    }

    dirp->offset = 0;
    dirp->entry = NULL;

    fi->fh = (unsigned long) dirp;
    return 0;
}


// TODO: Check if the first if is blogging to read the sub-dir
/*static*/ int minerva_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                           off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    if (!std::filesystem::exists(path))
    {
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    std::string minerva_entry_path = get_minerva_path(path);

    for (const auto& entry : std::filesystem::directory_iterator(minerva_entry_path))
    {
        std::string entry_name = entry.path().filename().string();
        filler(buf, entry_name.c_str(), NULL, 0);
    }

    return 0;
}

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
    return USER_HOME;
}

time_t get_mtime(const std::string path)
{
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) == -1)
    {
        perror(path.c_str());
        exit(1);
    }
    return statbuf.st_mtime;
}
