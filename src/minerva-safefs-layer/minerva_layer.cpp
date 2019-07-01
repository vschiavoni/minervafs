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
std::string get_minerva_temp_path(const char* path);

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

/**
 * Encodes a file from temporary storage to temporary storage
 * @param path Path to file as seen on the mountpoint
 * @return 0 If the encoding went right, -errno otherwise
 */
int encode(const char* path);
void decode_to_temp(const char* path);



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
    std::cout << "getattr(" << path << ")" << std::endl;
    int res = 0;

    // Check if the file is decoded in the temp directory
    std::string minerva_entry_temp_path = get_minerva_temp_path(path);
    if (std::filesystem::exists(minerva_entry_temp_path))
    {
        res = lstat(minerva_entry_temp_path.c_str(), stbuf);
        if (res == -1)
        {
            return -errno;
        }
        return 0;
    }

    // Check if the file exists in coded form
    std::string minerva_entry_path = get_minerva_path(path);
    memset(stbuf, 0, sizeof(struct stat));


    if (!std::filesystem::exists(minerva_entry_path))
    {
        std::cerr << "getattr(" << path << "): Could not find file" << std::endl;
        return -ENOENT;
    }

    // If the entry actually exists we have to "fake" m_time
    stbuf->st_mtime = get_mtime(minerva_entry_path);
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();

    // If the path points to a directory
    if (std::filesystem::is_directory(minerva_entry_path))
    {
        stbuf->st_mode = S_IFDIR | 0755;
        // If that directory is the root
        if (strcmp(path, "/") == 0)
        {
            // TODO: Look at st mode
            stbuf->st_nlink = 2;
            return 0;
        }
        stbuf->st_nlink = 1;
        //stbuf->st_size = std::filesystem::file_size(minerva_entry_path); // TODO: Chekc if this is valid
        return 0;
    }
    // If the path points to a file
    stbuf->st_mode = S_IFREG | 0664;
    stbuf->st_nlink = 1;
    
    std::string filename = std::filesystem::path(minerva_entry_path).filename().string();
    size_t size = std::filesystem::file_size(minerva_entry_path);
    std::cout << "\tsize = " << size << std::endl;
    if (size != 0)
    {
        nlohmann::json obj = tartarus::readers::msgpack_reader(minerva_entry_path);
        size = obj["file_size"].get<size_t>();
        std::cout << "\tunpacked size:" << size << std::endl;
    }
    stbuf->st_size = size;

    return res;
}

/*static*/ int minerva_fgetattr(const char *path, struct stat *stbuf,
                            struct fuse_file_info *fi)
{
    (void) fi;
    return minerva_getattr(path, stbuf);
}

/*static*/ int minerva_access(const char* path, int mask)
{
    (void) mask;
    std::string minerva_entry_path = get_minerva_path(path);
    if (!std::filesystem::exists(minerva_entry_path))
    {
        std::string minerva_entry_temp_path = get_minerva_temp_path(path);
        if (!std::filesystem::exists(minerva_entry_temp_path))
        {
            std::cerr << "access(" << path << "): Could not find file" << std::endl;
            return -ENOENT;
        }
        return 0;
    }
    return 0;

}

/*static*/ int minerva_create(const char *path, mode_t mode, struct fuse_file_info* fi)
{
    std::string minerva_entry_path = get_minerva_path(path);
    std::cout << "minerva_create(" << path << "): Checking if " << minerva_entry_path  << " exists" << std::endl;
    if (std::filesystem::exists(minerva_entry_path))
    {
        if (std::filesystem::is_directory(minerva_entry_path))
        {
            std::cerr << "minerva_create(" << path << "): Points to an existing directory" << std::endl;
            return -EISDIR;
        }
        std::cerr << "minerva_create(" << path << "): Points to an existing file" << std::endl;
        return -EEXIST;
    }
    std::string minerva_entry_temp_path = get_minerva_temp_path(path);
    std::cout << "minerva_create(" << path << "): About to open temp file (" << minerva_entry_temp_path  << ")" << std::endl;
    int file_handle = open(minerva_entry_temp_path.c_str(), fi->flags, mode);
    if (file_handle == -1)
    {
        std::cerr << "minerva_create(" << path << "): Could not open temp file" << minerva_entry_temp_path << std::endl;
        return -errno;
    }
    std::cout << "minerva_create(" << path << "): Successfully opened temp file" << minerva_entry_temp_path << " (" <<  file_handle  << ")" << std::endl;
    fi->fh = file_handle;
    //close(file_handle);
    return 0;
}

/*static*/ int minerva_open(const char* path, struct fuse_file_info* fi)
{
    std::string minerva_entry_path = get_minerva_path(path);
    std::string minerva_entry_temp_path = get_minerva_temp_path(path);
    printf("minerva_open(%s) flags: %x\n", path, fi->flags);

    int res;

    //FIXME check whether open flags are set to create empty file
    if (std::filesystem::exists(minerva_entry_temp_path))
    {
        std::cout << "minerva_open(" << path << "): Found temp file (" << minerva_entry_temp_path << ")"  << std::endl;
        res = open(minerva_entry_temp_path.c_str(), fi->flags);
    }
    else if (std::filesystem::exists(minerva_entry_path))
    {
        std::cout << "minerva_open(" << path << "): Found coded file (" << minerva_entry_path << ")"  << std::endl;
        //Decode existing file in temp directory
        decode_to_temp(path);
        res = open(minerva_entry_temp_path.c_str(), fi->flags);
    }
    else
    {
        std::cerr << "minerva_open(" << path << "): Could not find file -> -ENOENT" << std::endl;
        return -ENOENT;
    }

    if (res == -1)
    {
        return -errno;
    }
    fi->fh = res;
    return 0;
}

/*static*/ int minerva_read(const char *path, char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi)
{


    std::string minerva_entry_path = get_minerva_path(path);
    std::cout << "read path " << minerva_entry_path << std::endl;
    std::string filename = std::filesystem::path(path).filename().string();

    std::string minerva_entry_temp_path = get_minerva_temp_path(path);


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
    (void) fi;
    std::cout << "minerva_write(" << path << ")" << std::endl;
    std::string minerva_entry_temp_path = get_minerva_temp_path(path);
    std::string minerva_entry_path = get_minerva_path(path);
    
    //Check if the file is currently decoded in temp directory
    if (!std::filesystem::exists(minerva_entry_temp_path))
    {

        std::cout << "minerva_write(" << path << "): Cannot find temp entry (" << minerva_entry_temp_path << ")" << std::endl;
        //Check if the file exists in permanent storage
        if (!std::filesystem::exists(minerva_entry_path))
        {
            std::cout << "minerva_write(" << path << "): Cannot find entry (" << minerva_entry_path << ")" << std::endl;
            return -ENOENT;
        }
        //Decode from permanent storage into temp directory
        //TODO refactor in its own function as it might be reused in the read function
        std::cout << "minerva_write(" << path << "): Decoding " << minerva_entry_path  <<  " into " << minerva_entry_temp_path << "" << std::endl;
        
        std::string filename = std::filesystem::path(path).filename().string();
        tartarus::model::coded_data coded_data = minerva_storage.load(filename);
        codes::code_params params = extract_code_params(coded_data.coding_configuration);
        codewrapper::CodeWrapper coder(params);
        tartarus::model::raw_data data = coder.decode_data(coded_data);
        tartarus::writers::vector_disk_writer(minerva_entry_temp_path, data.data);
        
        std::cout << "minerva_write(" << path << "): Coded file decoded in temp directory" << std::endl;
    }

    //int fd = open(minerva_entry_temp_path.c_str(), O_WRONLY);
    int fd = fi->fh;
    // If we are unable to open a file we return an error
    if (fd == -1)
    {
        std::cout << "minerva_write(" << path << "): Cannot open temp entry (" << minerva_entry_temp_path  << ")" << std::endl;
        return -errno;
    }

    // Right now we just make a pars throgh of data
    int res = pwrite(fd, buf, size, offset);
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
        std::cerr << "minerva_release(" << path << "): " << path << " is a directory" << std::endl;
        return 0;
    }

    std::string minerva_entry_temp_path = get_minerva_temp_path(path);
    
    if (!std::filesystem::exists(minerva_entry_temp_path))
    {
        std::cerr << "minerva_release(" << path << "): Cannot find temporary entry for " << path << std::endl;
        if (!std::filesystem::exists(minerva_entry_path))
        {
            std::cerr << "minerva_release(" << path << "): Cannot find entry for " << path << std::endl;
            return -ENOENT;
        }
        return 0;
    }

    return encode(path);
}

// TODO: update as needed
/*static*/ int minerva_truncate(const char *path, off_t length)
{
    /*
    TODO truncate a minerva file
    1. Locate file
    2. Read acutal file size (afz) from metadata
    3. Test size argument against afz
    3a. If size == afz -> return 0
    4. Decode file .temp
    5. Test size argument against afz
    5a. If size < afz -> Trim decoded file to match size
    5b. If size > afz -> Zero-fill decoded file to match size
    6. Encoded modified file
    7. Store modified file
    8. Delete temp file
    */


    std::string minerva_entry_path = get_minerva_path(path);
    if (std::filesystem::is_directory(minerva_entry_path))
    {
        std::cerr << "truncate(" << path << "): path points to a directory" << std::endl;
        return -EISDIR;
    }
    if (!std::filesystem::exists(minerva_entry_path))
    {
        std::cerr << "truncate(" << path << "): Could not find file" << std::endl;
        return -ENOENT;
    }


    nlohmann::json obj = tartarus::readers::msgpack_reader(minerva_entry_path);
    size_t actual_file_size = obj["file_size"].get<size_t>();
    size_t size = static_cast<size_t>(length);
    if (size == actual_file_size)
    {
        return 0;
    }


    std::string minerva_entry_temp_path = get_minerva_temp_path(path);
    if (!std::filesystem::exists(minerva_entry_temp_path))
    {

        tartarus::model::coded_data coded_data = minerva_storage.load(minerva_entry_path);

        codes::code_params params = extract_code_params(coded_data.coding_configuration);
        codewrapper::CodeWrapper coder(params);

        tartarus::model::raw_data data = coder.decode_data(coded_data);

        tartarus::writers::vector_disk_writer(minerva_entry_temp_path, data.data);
    }

    if (truncate(minerva_entry_temp_path.c_str(), size) == -1)
    {
        std::cerr << "minerva_truncate(" << path << "): could not truncate file from " << actual_file_size << " to " << size << std::endl;
        if (unlink(minerva_entry_temp_path.c_str()) == -1) {
            std::cerr << "minerva_truncate(" << path << "): could not delete temp file" << std::endl;
            return -errno;
        }
        return -errno;
    }

    // Encode modified file
    return encode(path);
}

// Make items
/*static*/ int minerva_mknod(const char *path, mode_t mode, dev_t rdev)
{
    std::cout << "minerva_mknod(" << path << ")" << std::endl;
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
        
    std::cout << "minerva_mknod(" << path << "): " << "I am in end of mknode" << std::endl;
    return 0;
}

/*static*/ int minerva_mkdir(const char *path, mode_t mode)
{
    std::cout << "minerva_mkdir(" << path << ")" << std::endl;
    std::string persistent_folder_path = get_minerva_path(path);
    if (mkdir(persistent_folder_path.c_str(), mode) == -1)
    {
        std::cout << "minerva_mkdir(" << path << "): Could not create permanent directory (" << persistent_folder_path << ")" << std::endl;
        return -errno;
    }
    std::string temporary_folder_path = get_minerva_temp_path(path);
    if (mkdir(temporary_folder_path.c_str(), mode) == -1)
    {
        std::cout << "minerva_mkdir(" << path << "): Could not create temporary directory (" << temporary_folder_path << ")" << std::endl;
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
    std::string minerva_entry_path = get_minerva_path(path);
    std::cout << "opendir(" << path << "): " << minerva_entry_path << std::endl;
    if (!std::filesystem::exists(minerva_entry_path))
    {
        std::cerr << "opendir(" << path << "): Could not find entry (" << minerva_entry_path << ")" << std::endl;
        return -ENOENT;
    }
    if (!std::filesystem::is_directory(minerva_entry_path))
    {
        std::cerr << "opendir(" << path << "): " << minerva_entry_path << " is not a directory" << std::endl;
        return -ENOTDIR;
    }
    struct minerva_dirp* dirp = (minerva_dirp*) malloc(sizeof(struct minerva_dirp));
    if (dirp == NULL)
    {
        std::cerr << "opendir(" << path << "): -ENOMEM" << std::endl;
        return -ENOMEM;
    }

    dirp->dp = opendir(minerva_entry_path.c_str());
    if (dirp->dp == NULL)
    {
        free(dirp);
        std::cerr << "opendir(" << path << "): -" << errno << std::endl;
        return -errno;
    }

    dirp->offset = 0;
    dirp->entry = NULL;

    fi->fh = (unsigned long) dirp;
    std::cout << "opendir(" << path << "): Done" << std::endl;
    return 0;
}


// TODO: Check if the first if is blogging to read the sub-dir
/*static*/ int minerva_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                           off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;
    std::string minerva_entry_path = get_minerva_path(path);
    std::cout << "readdir(" << path << "): " << minerva_entry_path << std::endl;

    if (!std::filesystem::exists(minerva_entry_path))
    {
        std::cerr << "readdir(" << path << "): Could not find entry" << std::endl;
        return -ENOENT;
    }

    if (!std::filesystem::is_directory(minerva_entry_path))
    {
        std::cerr << "readdir(" << path << "): " << minerva_entry_path << " is not a directory" << std::endl;
        return -ENOTDIR;
    }

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

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
    std::string minerva_temp_path = get_minerva_temp_path(path);
    if (!std::filesystem::exists(minerva_entry_path) && !std::filesystem::exists(minerva_temp_path))
    {
        std::cerr << "flush(" << path << "): Could not find file" << std::endl;
        return -ENOENT;
    }
    (void) fi;
    return 0;
}

/*static*/ int minerva_rename(const char* from, const char* to) {
    std::string source = get_minerva_path(from);
    if (!std::filesystem::exists(source))
    {
        std::cerr << "rename(" << from << ", " << to << "): Could not find file" << std::endl;
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

/*static*/ int minerva_unlink(const char* path)
{
    std::string entry_path = get_minerva_path(path);
    if (!std::filesystem::exists(entry_path))
    {
        std::cerr << "unlink(" << path << "): Could not find file" << std::endl;
        return -ENOENT;
    }
    if (unlink(entry_path.c_str()) == -1) {
        return -errno;
    }
    return 0;
}

/*static*/ int minerva_utimens(const char* path, const struct timespec tv[2])
{
    std::string minerva_entry = get_minerva_path(path);
    if (!std::filesystem::exists(minerva_entry))
    {
        minerva_entry = get_minerva_temp_path(path);
        if (!std::filesystem::exists(minerva_entry))
        {
            std::cerr << "utimens(" << path << "): Could not find file" << std::endl;
            return -ENOENT;
        }
    }
    int fd = open(minerva_entry.c_str(), O_ASYNC);
    if (fd == -1) {
        return -errno;
    }
    if (futimens(fd, tv) == -1)
    {
        return -errno;
    }
    close(fd);
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

/**
 * Generate a temp path based on the path given in the FUSE call
 * @param   path const char* is path provide from the fuse file system impl
 * @return  a path in the temp of the minerva folder
 */
std::string get_minerva_temp_path(const char* path)
{
    std::string filename = std::filesystem::path(path).filename().string();
    return get_user_home() + minervafs_root_folder + minervafs_temp + "/" + filename;
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


int encode(const char* path)
{
    std::string minerva_entry_temp_path = get_minerva_temp_path(path);
    size_t file_size = std::filesystem::file_size(minerva_entry_temp_path);
    std::vector<uint8_t> data = tartarus::readers::vector_disk_reader(minerva_entry_temp_path);

    codes::code_params code_param = get_code_params(file_size);
    codewrapper::CodeWrapper code(code_param);
    std::string filename = (std::filesystem::path(path)).filename().string();
    std::string mime_type = "TODO"; // TODO: Change;
    tartarus::model::raw_data raw {0, static_cast<uint32_t>(file_size), filename, mime_type, data};
    tartarus::model::coded_data coded = code.encode_data(raw);

    if (!minerva_storage.store(coded))
    {
        return -errno;
    }
    assert(unlink(minerva_entry_temp_path.c_str()) == 0);
    return 0;

}

void decode_to_temp(const char* path)
{
    std::string minerva_entry_path = get_minerva_path(path);
    std::string minerva_entry_temp_path = get_minerva_temp_path(path);
    std::string filename = std::filesystem::path(path).filename().string();

    tartarus::model::coded_data coded_data = minerva_storage.load(filename);
    codes::code_params params = extract_code_params(coded_data.coding_configuration);
    codewrapper::CodeWrapper coder(params);
    tartarus::model::raw_data data = coder.decode_data(coded_data);
    tartarus::writers::vector_disk_writer(minerva_entry_temp_path, data.data);
}
