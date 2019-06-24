#include "minerva_layer.hpp"

#include <minerva/minerva.hpp>

#include <codewrapper/codewrapper.hpp>

#include <tartarus/model/raw_data.hpp>
#include <tartarus/model/coded_data.hpp>
#include <tartarus/model/coded_pair.hpp>
#include <tartarus/readers.hpp>
#include <tartarus/writers.hpp>



#include <pwd.h>
#include <unistd.h>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <dirent.h>

#include <cmath>
#include <cstring>
#include <string>


#include <vector>

#include <filesystem>

#include <iostream> // REMEMBER TO REMOVE

static const std::string minervafs_root_folder = "/.minervafs";
static const std::string minervafs_basis_folder = "/.basis/";
static const std::string minervafs_registry = "/.registry/";
static const std::string minervafs_identifier_register = "/identifiers";//"/.identifiers";
static const std::string minervafs_config = "/.minervafs_config";
static const std::string minervafs_temp = "/.temp"; // For temporarly decode files
static const std::vector<std::string> IGNORE = {".basis", ".identifiers", ".minervafs_config", ".registry", ".temp"};

static std::string USER_HOME = "";

static minerva::minerva minerva_storage;

static minerva::file_format used_file_format = minerva::file_format::JSON;

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

void set_file_format(int file_format)
{
    switch(file_format)
    {
    case 0:
        set_file_format(minerva::file_format::JSON);
        break;
    case 1:
        set_file_format(minerva::file_format::BSON);
        break;
    case 2:
        set_file_format(minerva::file_format::UBJSON);
        break;
    case 3:
        set_file_format(minerva::file_format::CBOR);
        break;
    case 4:
        set_file_format(minerva::file_format::MSGPACK);
        break;
    default:
        set_file_format(minerva::file_format::JSON);
        break;
    }
}

bool temp_file_exists(const std::string& filename);



/*static*/ void* minerva_init(struct fuse_conn_info *conn)
{
    (void) conn;
    USER_HOME = get_user_home();
    setup();
    return 0;
}

// Base operation

/*static*/ int minerva_getattr(const char* path, struct stat* stbuf)
{
    int res = 0;

    std::string filename = std::filesystem::path(path).filename().string();
    if (temp_file_exists(filename))
    {
        std::string minerva_entry_temp_path = USER_HOME + minervafs_root_folder + minervafs_temp + "/" + filename;
        res = lstat(minerva_entry_temp_path.c_str(), stbuf);
        if (res == -1)
        {
            return -errno;
        }
        return 0;
    }

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
        std::string filename = std::filesystem::path(minerva_entry_path).filename().string();
        if(filename != ".identifiers" && filename != ".minervafs_config")
        {
            size_t size = std::filesystem::file_size(minerva_entry_path);
            if (size != 0)
            {
                nlohmann::json obj = tartarus::readers::msgpack_reader(minerva_entry_path);
                size = obj["file_size"].get<size_t>();
            }
            stbuf->st_size = size;
        }
        else
        {
            stbuf->st_size = std::filesystem::file_size(minerva_entry_path);
        }

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
    (void) mask;
    std::string minerva_entry_path = get_minerva_path(path);
    if (!std::filesystem::exists(minerva_entry_path))
    {
        std::string minerva_entry_temp_path = USER_HOME + minervafs_root_folder + minervafs_temp + "/" + path;
        if (!std::filesystem::exists(minerva_entry_temp_path))
        {
            return -ENOENT;
        }
        return 0;
    }
    return 0;

}


/*static*/ int minerva_open(const char* path, struct fuse_file_info* fi)
{
    std::string minerva_entry_path = get_minerva_path(path);
    std::string minerva_entry_temp_path = USER_HOME + minervafs_root_folder + minervafs_temp + "/" +
        std::filesystem::path(minerva_entry_path).filename().string();



    int res;

    if (std::filesystem::exists(minerva_entry_temp_path))
    {
        res = open(minerva_entry_temp_path.c_str(), fi->flags);
    }
    else
    {
        res = open(minerva_entry_path.c_str(), fi->flags);
    }


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
    std::cout << "read path " << minerva_entry_path << std::endl;
    std::string filename = std::filesystem::path(path).filename().string();

    std::string minerva_entry_temp_path = USER_HOME + minervafs_root_folder + minervafs_temp + "/" + filename;


    int fd;
    int res;

    (void) fi;
    if (!std::filesystem::exists(minerva_entry_temp_path))
    {

        std::cout << "finding json bug" << std::endl;
        tartarus::model::coded_data coded_data = minerva_storage.load(filename);
        std::cout << "lol" << std::endl;

        codes::code_params params = extract_code_params(coded_data.coding_configuration);
        codewrapper::CodeWrapper coder(params);

        tartarus::model::raw_data data = coder.decode_data(coded_data);



        tartarus::writers::vector_disk_writer(minerva_entry_temp_path, data.data);
    }


    fd = open(minerva_entry_temp_path.c_str(), O_RDONLY);

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

    int fd;
    int res;

    (void) fi;
    std::string minerva_entry_path = get_minerva_path(path);
    fd = open(minerva_entry_path.c_str(), O_WRONLY);


    // If we are unable to open a file we return an error
    if (fd == -1)
    {
        std::string minerva_temp_path = USER_HOME + minervafs_root_folder + minervafs_temp + "/" + path;
        fd = open(minerva_temp_path.c_str(), O_WRONLY);
        if (fd == -1)
        {
            return -errno;
        }
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


/*static*/ int minerva_release(const char* path, struct fuse_file_info *fi)
{
    (void) fi;

    std::string minerva_entry_path = get_minerva_path(path);

    if (std::filesystem::is_directory(minerva_entry_path))
    {
        return 0;
    }

    bool need_to_remove_temp_file = false;
    if (!std::filesystem::exists(minerva_entry_path))
    {
        //Try to locate file from .temp folder
        minerva_entry_path = USER_HOME + minervafs_root_folder + minervafs_temp + "/" + path;
        if (!std::filesystem::exists(minerva_entry_path))
        {
            return -ENOENT;
        }
        need_to_remove_temp_file = true;
    }

    auto file_size = std::filesystem::file_size(minerva_entry_path);
    std::vector<uint8_t> data = tartarus::readers::vector_disk_reader(minerva_entry_path);

    //(static_cast<size_t>(file_size));

    // minerva_read(path, (char*)data.data(), data.size(), 0, fi);


    codes::code_params code_param = get_code_params(file_size);
    codewrapper::CodeWrapper code(code_param);

    std::string filename = (std::filesystem::path(path)).filename().string();
    std::string mime_type = "TODO"; // TODO: Change;

    tartarus::model::raw_data raw {0, static_cast<uint32_t>(file_size), filename, mime_type, data};
    tartarus::model::coded_data coded = code.encode_data(raw);

    if (minerva_storage.store(coded))
    {
        if (need_to_remove_temp_file)
        {
            unlink(minerva_entry_path.c_str());
        }
        return 0;
    }
    else
    {
        return -errno;
    }
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
    std::cout << "I am in end of mknode";
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
        if (strcmp(path, "/") == 0 && std::find(IGNORE.begin(), IGNORE.end(), entry_name) != IGNORE.end())
        {
            continue;
        }
        filler(buf, entry_name.c_str(), NULL, 0);
    }

    return 0;
}

/*static*/ int minerva_flush(const char* path, struct fuse_file_info* fi) {
    std::string minerva_entry_path = get_minerva_path(path);
    std::string minerva_temp_path = USER_HOME + minervafs_root_folder + minervafs_temp + "/" + path;
    if (!std::filesystem::exists(minerva_entry_path) && !std::filesystem::exists(minerva_temp_path))
    {
        return -ENOENT;
    }
    (void) fi;
    return 0;
}

/*static*/ int minerva_rename(const char* from, const char* to) {
    std::string source = get_minerva_path(from);
    if (!std::filesystem::exists(source))
    {
        return -ENOENT;
    }
    //Need to determine whether `to` is a file or a directory
    std::string destination = get_minerva_path(to);
    if (rename(source.c_str(), destination.c_str()) == -1)
    {
        return -errno;
    }
    return 0;
}

// Helper functions



void setup()
{
    const std::string MKDIR = "mkdir";
    const std::string TOUCH = "touch";

    std::string config_file_path = USER_HOME + minervafs_root_folder + minervafs_config;

    if ( std::filesystem::exists(config_file_path))
    {
        nlohmann::json config = tartarus::readers::json_reader(config_file_path);
        minerva_storage = minerva::minerva(config);
        if (!std::filesystem::exists(USER_HOME + minervafs_root_folder + minervafs_temp))
        {
            std::system((MKDIR + " " + USER_HOME + minervafs_root_folder + minervafs_temp).c_str());
        }
        return;
    }

    // TODO if config exists load and return

    // TODO add missing folder

    if (!std::filesystem::exists(USER_HOME + minervafs_root_folder))
    {
        std::system((MKDIR + " " + USER_HOME + minervafs_root_folder).c_str());
    }

    // if (!std::filesystem::exists(USER_HOME + minervafs_root_folder + minervafs_basis_folder))
    // {
    //     std::system((MKDIR + " " + USER_HOME + minervafs_root_folder + minervafs_basis_folder).c_str());
    // }

    // if (!std::filesystem::exists(USER_HOME + minervafs_root_folder + minervafs_registry))
    // {
    //     std::system((MKDIR + " " + USER_HOME + minervafs_root_folder + minervafs_registry).c_str());
    // }

    // if (!std::filesystem::exists(USER_HOME + minervafs_root_folder + minervafs_identifier_register))
    // {
    //     std::system((TOUCH + " " + USER_HOME + minervafs_root_folder + minervafs_identifier_register).c_str());
    // }

    if (!std::filesystem::exists(USER_HOME + minervafs_root_folder + minervafs_temp))
    {
        std::system((MKDIR + " " + USER_HOME + minervafs_root_folder + minervafs_temp).c_str());
    }

    nlohmann::json minerva_config;

    minerva_config["register_path"] = (USER_HOME + minervafs_root_folder + "/.identifiers");
    minerva_config["base_register_path"] =  (USER_HOME + minervafs_root_folder + minervafs_registry);
    minerva_config["base_out_path"] = (USER_HOME + minervafs_root_folder + minervafs_basis_folder);;
    minerva_config["out_path"] = (USER_HOME + minervafs_root_folder + "/");
    minerva_config["max_registry_size"] = 8589934592; // 8 GB of RAM;
    minerva_config["file_format"] = used_file_format;
    // minerva_config["register_path"] = (USER_HOME + minervafs_root_folder + minervafs_identifier_register); // string
    // minerva_config["base_register_path"] = (USER_HOME + minervafs_root_folder + minervafs_registry);       // string
    // minerva_config["base_out_path"] = (USER_HOME + minervafs_root_folder + minervafs_basis_folder);        // string
    // minerva_config["out_path"] = (USER_HOME + minervafs_root_folder + "/");                                // string
    // minerva_config["max_registry_size"] = 8589934592; // 8 GB of RAM

    minerva_storage = minerva::minerva(minerva_config);
    tartarus::writers::json_writer(config_file_path, minerva_config);


//    minerva_storage = minerva::minerva(t_config);




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

codes::code_params get_code_params(size_t file_size)
{
    int m = 15;

    while (!((std::pow(2, m) * 2) <= file_size))
    {
        --m;
    }
    if (m < 3)
    {
        m = 3; 
    }
    
    codes::code_params params;
    params.code_name = "hammingcode";
    params.m = m;
    return params;
}

codes::code_params extract_code_params(nlohmann::json config)
{
    codes::code_params params;
    params.code_name = config["name"].get<std::string>();

    if (params.code_name == "hammingcode")
    {
        params.m = config["m"].get<int>();
    }
    return params;
}

void set_file_format(minerva::file_format file_format)
{
    used_file_format = file_format;
}

bool temp_file_exists(const std::string& filename)
{
    std::string minerva_entry_temp_path = USER_HOME + minervafs_root_folder + minervafs_temp + "/" + filename;
    return std::filesystem::exists(minerva_entry_temp_path);
}
