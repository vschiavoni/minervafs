#include "minerva_layer.hpp"
#include "utils.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream> // REMEMBER TO REMOVE
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include <cmath>
#include <cstring>

#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <codewrapper/codewrapper.hpp>
#include <minerva/minerva.hpp>
#include <minerva/model/data.hpp>

#include <tartarus/model/coded_pair.hpp>

#include <tartarus/readers.hpp>
#include <tartarus/writers.hpp>

#include <nlohmann/json.hpp>



static std::string minervafs_root_folder = "~/.minervafs";
static const std::string minervafs_basis_folder = "/.basis/";
static const std::string minervafs_registry = "/.registry/";
static const std::string minervafs_identifier_register = "/identifiers";//"/.identifiers";
static const std::string minervafs_config = "/.minervafs_config";
static const std::string minervafs_temp = "/.temp"; // For temporarly decode files
static const std::vector<std::string> IGNORE = {".indexing", ".minervafs_config", ".temp"};
static const bool minverva_versioning = false; 

static minerva::minerva minerva_storage;
static std::map<std::string, std::atomic_uint> open_files;
static std::mutex of_mutex;
static minerva::file_format used_file_format = minerva::file_format::JSON;

//TODO: we should deallocate this memory when exiting the application
thread_local codewrapper::codewrapper* codecs[20]={NULL, NULL, NULL, NULL, NULL,
						   NULL, NULL, NULL, NULL, NULL,
						   NULL, NULL, NULL, NULL, NULL,
						   NULL, NULL, NULL, NULL, NULL};

// Helper functions
/**
 * Loads some of the configuration from the settings
 * @param path Path to the json configuration file
 */
static void load_config(std::string path);

/**
 * Creates all the directories and objects necessary to start the filesystem
 */
static void setup();

/**
 * Compute path to the file in the permanent storage folder
 * @param path Path to file as seen on the mountpoint
 * @return Path to the file in the permanent storage folder
 */
std::string get_permanent_path(const char* path);

/**
 * Compute path to the file in the temporary storage folder
 * @param path Path to file as seen on the mountpoint
 * @return Path to the file in the temporary storage folder
 */
std::string get_temporary_path(const char* path);

/**
 * Compute path to a file relative the minerva storage
 * @param path Path to file as seen on the mountpoint
 * @return Path to the file inside of the minerva storage folder (without the base directory)
 */
std::string get_minerva_relative_path(const char* path);

/**
 * Tests whether a flag combination implies that the file is open for modification
 * @param flags Combination of flags to test
 * @return true if the flags are set for modification, false otherwise
 */
static bool is_flagged_for_modification(int flags);

time_t get_mtime(const std::string path);

nlohmann::json get_code_params(size_t file_size);

nlohmann::json extract_code_params(nlohmann::json config);

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

/**
 * Encodes a file from temporary storage to permanent storage
 * @param path Path to file as seen on the mountpoint
 * @return 0 If the encoding went fine, -errno otherwise
 */
int encode(const char* path);

/**
 * Decodes a file from permanent storage to temporary storage
 * @param path Path to file as seen on the mountpoint
 * @return 0 If the decoding went fine, -1 otherwise
 */
int decode(const char* path);


/**
 * Returns a Hamming codec for a specific m value
 * @param cparams code params with code name and m value
 * @ the codec if m was valid and a Hamming codec was requested, NULL otherwise
 */
codewrapper::codewrapper* get_hamming_codec(nlohmann::json code_config)
{


    if ((code_config.find("m") == code_config.end()) || (code_config.find("code_type") == code_config.end()))
    {
        return NULL;
    }

    uint32_t m = code_config["m"].get<uint32_t>();
    codes::code_type code_type = code_config["code_type"].get<codes::code_type>();
    
    
    if(m < 3 || m > 19 || code_type != codes::code_type::HAMMING_CODE)
    {
	return NULL;
    }

    if(codecs[m] == NULL)
    {
	codecs[m] = new codewrapper::codewrapper(code_config);
    }
    
    return codecs[m];
}

void* minerva_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
    (void) conn;
    (void) cfg;
    setup();
    return 0;
}

void minerva_destroy(void *private_data)
{
    (void) private_data;
    std::filesystem::remove_all(get_temporary_path(" "));
}

// Base operation

int minerva_getattr(const char* path, struct stat* stbuf, struct fuse_file_info *fi)
{
    (void) fi;
    int res = 0;

    // Check if the file is decoded in the temp directory
    std::string minerva_entry_temp_path = get_temporary_path(path);
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
    std::string minerva_entry_path = get_permanent_path(path);
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
    if (size != 0)
    {
        auto data = tartarus::readers::vector_disk_reader(minerva_entry_path);
        size = 0;
        for (size_t i = 0; i < sizeof(size_t); ++i)
        {
            size_t next = data.at(i);
            next = next << (i * 8);
            size ^= next; 
        }
        
    }
    stbuf->st_size = size;

    return res;
}

int minerva_access(const char* path, int mask)
{
    (void) mask;
    std::string minerva_entry_path = get_permanent_path(path);
    if (!std::filesystem::exists(minerva_entry_path))
    {
        std::string minerva_entry_temp_path = get_temporary_path(path);
        if (!std::filesystem::exists(minerva_entry_temp_path))
        {
            std::cerr << "access(" << path << "): Could not find file" << std::endl;
            return -ENOENT;
        }
        return 0;
    }
    return 0;

}

static void register_open_file(std::string path)
{
    std::lock_guard<std::mutex> lock(of_mutex);
    auto entry = open_files.find(path);
    if (entry != open_files.end())
    {
        entry->second++;
    }
    else
    {
        open_files[path] = 1;
    }
}

static void register_closed_file(std::string path)
{
    std::lock_guard<std::mutex> lock(of_mutex);
    auto entry = open_files.find(path);
    if (entry == open_files.end())
    {
        throw new std::logic_error("Cannot find file that must be closed");
    }
    entry->second--;
    if (entry->second == 0)
    {
        open_files.erase(entry);
        unlink(path.c_str());
    }
}

int minerva_create(const char *path, mode_t mode, struct fuse_file_info* fi)
{
    std::string minerva_entry_path = get_permanent_path(path);
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
    std::string minerva_entry_temp_path = get_temporary_path(path);
    int file_handle = open(minerva_entry_temp_path.c_str(), fi->flags, mode);
    if (file_handle == -1)
    {
        std::cerr << "minerva_create(" << path << "): Could not open temp file" << minerva_entry_temp_path << std::endl;
        return -errno;
    }
    register_open_file(minerva_entry_temp_path);
    fi->fh = file_handle;
    return 0;
}

int minerva_open(const char* path, struct fuse_file_info* fi)
{
    std::string minerva_entry_path = get_permanent_path(path);
    std::string minerva_entry_temp_path = get_temporary_path(path);

    int res;

    // Check if the file is decoded in temporary storage
    if (std::filesystem::exists(minerva_entry_temp_path))
    {
        res = open(minerva_entry_temp_path.c_str(), fi->flags);
        if (res == -1)
        {
            return -errno;
        }
        register_open_file(minerva_entry_temp_path.c_str());
        fi->fh = res;
        return 0;
    }
    // Check if the file is coded in permanent storage
    else if (std::filesystem::exists(minerva_entry_path))
    {
        //Decode existing file in temp directory
        decode(path);
        res = open(minerva_entry_temp_path.c_str(), fi->flags);
        if (res == -1)
        {
            return -errno;
        }
        register_open_file(minerva_entry_temp_path.c_str());
        fi->fh = res;
        return 0;
    }
    // Check if flags are set for creation
    else if (is_flagged_for_modification(fi->flags))
    {
        res = minerva_create(path, 0644, fi);
        if (res == -1)
        {
            return -errno;
        }
        register_open_file(minerva_entry_temp_path.c_str());
        fi->fh = res;
        return 0;
    }
    //The file cannot be found
    std::cerr << "minerva_open(" << path << "): Could not find file -> -ENOENT" << std::endl;
    return -ENOENT;
}

int minerva_read(const char *path, char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi)
{


    std::string minerva_entry_path = get_permanent_path(path);
    std::string filename = std::filesystem::path(path).filename().string();

    std::string minerva_entry_temp_path = get_temporary_path(path);


    int fd;
    int res;

    (void) fi;
    if (!std::filesystem::exists(minerva_entry_temp_path))
    {
        decode(path);
    }

    fd = fi->fh;

    if (fd == -1)
    {
        return -errno;
    }

    res = pread(fd, buf, size, offset);

    if (res == -1)
    {
        res = -errno;
    }

    return res;
}


// TODO: Inject the usage of GDD
int minerva_write(const char* path, const char *buf, size_t size, off_t offset,
                         struct fuse_file_info* fi)
{
    (void) fi;
    std::string minerva_entry_temp_path = get_temporary_path(path);
    std::string minerva_entry_path = get_permanent_path(path);
    
    //Check if the file is currently decoded in temp directory
    if (!std::filesystem::exists(minerva_entry_temp_path))
    {
        //Check if the file exists in permanent storage
        if (!std::filesystem::exists(minerva_entry_path))
        {
            std::cerr << "minerva_write(" << path << "): Cannot find entry (" << minerva_entry_path << ")" << std::endl;
            return -ENOENT;
        }
        //Decode from permanent storage into temp directory
        //TODO refactor in its own function as it might be reused in the read function
        decode(path);
    }

    int fd = fi->fh;
    // If we are unable to open a file we return an error
    if (fd == -1)
    {
        std::cerr << "minerva_write(" << path << "): Cannot open temp entry (" << minerva_entry_temp_path  << ")" << std::endl;
        return -errno;
    }

    // Right now we just make a pars throgh of data
    int res = pwrite(fd, buf, size, offset);
    if (res == -1)
    {
        res = -errno;
    }

    return res;
}

int minerva_release(const char* path, struct fuse_file_info *fi)
{
    // Check if path points to directory
    std::string minerva_entry_path = get_permanent_path(path);
    if (std::filesystem::is_directory(minerva_entry_path))
    {
        std::cerr << "minerva_release(" << path << "): " << path << " is a directory" << std::endl;
        return -EISDIR;
    }

    // Check if path points to a non existing file or closed file
    std::string minerva_entry_temp_path = get_temporary_path(path);
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

    // Close file descriptor
    if (fi->fh && close(fi->fh) == -1)
    {
        std::cerr << "minerva_release(" << path << "): Could not close file descriptor (fd=" << fi->fh << ")" << std::endl;
        return -errno;
    }

    // If file was open for modifications, encode it and let encode function take care of unlinking the decided file in temporary storage
    if (is_flagged_for_modification(fi->flags))
    {
        encode(path);
        register_closed_file(minerva_entry_temp_path);
    }
    return 0;
}

// TODO: update as needed
int minerva_truncate(const char *path, off_t length, struct fuse_file_info *fi)
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
    (void) fi;

    std::string permanent_path = get_permanent_path(path);
    if (std::filesystem::is_directory(permanent_path))
    {
        std::cerr << "truncate(" << path << "): path points to a directory" << std::endl;
        return -EISDIR;
    }
    std::string minerva_entry_temp_path = get_temporary_path(path);
    size_t actual_file_size = 0;
    if (std::filesystem::exists(minerva_entry_temp_path))
    {
        actual_file_size = std::filesystem::file_size(minerva_entry_temp_path);
    }
    else if (std::filesystem::exists(permanent_path))
    {
        nlohmann::json obj = tartarus::readers::msgpack_reader(permanent_path);
        actual_file_size = obj["file_size"].get<size_t>();
    }
    else
    {
        std::cerr << "truncate(" << path << "): Could not find file" << std::endl;
        return -ENOENT;
    }

    size_t size = static_cast<size_t>(length);
    if (size == actual_file_size)
    {
        return 0;
    }

    if (!std::filesystem::exists(minerva_entry_temp_path))
    {
        decode(path);
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

int minerva_chmod(const char* path, mode_t mode, struct fuse_file_info *fi)
{
    (void) fi;
    //FIXME It should be possible to chmod files that are not yet in permanent storage
    std::string temporary_path = get_temporary_path(path);
    bool in_temporary_storage = std::filesystem::exists(temporary_path);
    std::string permanent_path = get_permanent_path(path);
    bool found_permanent = std::filesystem::exists(permanent_path);
    if (!in_temporary_storage && !found_permanent)
    {
        std::cerr << "minerva_chmod(" << path << "): Could not find file" << std::endl;
        return -ENOENT;
    }
    if (in_temporary_storage)
    {
        if (chmod(temporary_path.c_str(), mode) == -1)
        {
            return -errno;
        }
    }
    if (found_permanent)
    {
        if (chmod(permanent_path.c_str(), mode) == -1)
        {
            return -errno;
        }
    }
    return 0;
}


// Make items
int minerva_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int res;

    std::string persistent_entry_path = get_permanent_path(path);

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

int minerva_mkdir(const char *path, mode_t mode)
{
    std::string persistent_folder_path = get_permanent_path(path);
    if (mkdir(persistent_folder_path.c_str(), mode) == -1)
    {
        std::cerr << "mkdir(" << path << "): Could not create permanent directory (" << persistent_folder_path << ")" << std::endl;
        return -errno;
    }
    std::string temporary_folder_path = get_temporary_path(path);
    if (mkdir(temporary_folder_path.c_str(), mode) == -1)
    {
        std::cerr << "mkdir(" << path << "): Could not create temporary directory (" << temporary_folder_path << ")" << std::endl;
        return -errno;
    }
    return 0;
}

int minerva_releasedir(const char *path, struct fuse_file_info *fi)
{
    if (strlen(path) == 1 && path[0] == ((char) 7)) {
        return 0;
    }

    struct minerva_dirp* dirp = get_dirp(fi);
    closedir(dirp->dp);
    free(dirp);
    return 0;
}

// Directory operations
struct minerva_dirp* get_dirp(struct fuse_file_info *fi)
{
    return (struct minerva_dirp*)(uintptr_t)fi->fh;
}

// TODO: update to open sub dirs
int minerva_opendir(const char *path, struct fuse_file_info *fi)
{
    std::string minerva_entry_path = get_permanent_path(path);
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
    return 0;
}


// TODO: Check if the first if is blogging to read the sub-dir
int minerva_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                           off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags  flags)
{
    (void) offset;
    (void) fi;
    (void) flags;
    std::string minerva_entry_path = get_permanent_path(path);

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


    enum fuse_fill_dir_flags fill_flags = (fuse_fill_dir_flags) 0;
    filler(buf, ".", NULL, 0, fill_flags);
    filler(buf, "..", NULL, 0, fill_flags);

    for (const auto& entry : std::filesystem::directory_iterator(minerva_entry_path))
    {
        std::string entry_name = entry.path().filename().string();
        if (strcmp(path, "/") == 0 && std::find(IGNORE.begin(), IGNORE.end(), entry_name) != IGNORE.end())
        {
            continue;
        }
        filler(buf, entry_name.c_str(), NULL, 0, fill_flags);
    }

    return 0;
}

int minerva_rmdir(const char *path)
{
    std::string permanent_path = get_permanent_path(path);
    std::string temporary_path = get_temporary_path(path);
    if (!std::filesystem::exists(temporary_path))
    {
        std::cerr << "minerva_rmdir(" << path << "): Could not find entry in temporary storage" << std::endl;
        return -ENOENT;
    }
    if (!std::filesystem::exists(permanent_path))
    {
        std::cerr << "minerva_rmdir(" << path << "): Could not find entry in permanent storage" << std::endl;
        return -ENOENT;
    }
    if (!std::filesystem::is_directory(temporary_path))
    {
        std::cerr << "minerva_rmdir(" << path << "): Is not a directory in temporary storage" << std::endl;
        return -ENOTDIR;
    }
    if (!std::filesystem::is_directory(permanent_path))
    {
        std::cerr << "minerva_rmdir(" << path << "): Is not a directory in permanent storage" << std::endl;
        return -ENOTDIR;
    }
    if (rmdir(temporary_path.c_str()) == -1)
    {
        std::cerr << "minerva_rmdir(" << path << "): Could not rmdir temporary directory" << std::endl;
        return -errno;
    }
    if (rmdir(permanent_path.c_str()) == -1)
    {
        std::cerr << "minerva_rmdir(" << path << "): Could not rmdir permanent directory" << std::endl;
        return -errno;
    }
    return 0;
}

int minerva_flush(const char* path, struct fuse_file_info* fi) {
    std::string minerva_entry_path = get_permanent_path(path);
    std::string minerva_temp_path = get_temporary_path(path);
    if (!std::filesystem::exists(minerva_entry_path) && !std::filesystem::exists(minerva_temp_path))
    {
        std::cerr << "flush(" << path << "): Could not find file" << std::endl;
        return -ENOENT;
    }
    (void) fi;
    return 0;
}

int minerva_rename(const char* from, const char* to, unsigned int flags) {
    (void) flags;
    std::string source = get_temporary_path(from);
    std::string destination = get_temporary_path(to);

    if (!std::filesystem::exists(source))
    {
        source = get_permanent_path(from);
        if (!std::filesystem::exists(source))
        {
            std::cerr << "rename(" << from << ", " << to << "): Could not find entry (" << from << ")" << std::endl;
            return -ENOENT;
        }
        destination = get_permanent_path(to);
        if (rename(source.c_str(), destination.c_str()) == -1)
        {
            return -errno;
        }
        return 0;
    }

    if (rename(source.c_str(), destination.c_str()) == -1)
    {
        return -errno;
    }
    source = get_permanent_path(from);
    destination = get_permanent_path(to);
    if (rename(source.c_str(), destination.c_str()) == -1)
    {
        return -errno;
    }

    return 0;
}

int minerva_unlink(const char* path)
{
    std::string entry_path = get_permanent_path(path);
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

int minerva_utimens(const char* path, const struct timespec tv[2], struct fuse_file_info *fi)
{
    (void) fi;
    std::string minerva_entry = get_permanent_path(path);
    if (!std::filesystem::exists(minerva_entry))
    {
        minerva_entry = get_temporary_path(path);
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

int minerva_listxattr(const char* path, char* list, size_t size)
{
    (void) list;
    (void) size;
    std::string minerva_entry = get_permanent_path(path);
    if (!std::filesystem::exists(minerva_entry))
    {
        minerva_entry = get_temporary_path(path);
        if (!std::filesystem::exists(minerva_entry))
        {
            std::cerr << "utimens(" << path << "): Could not find file" << std::endl;
            return -ENOENT;
        }
    }
    return 0;
}

// Helper functions
static void load_config(std::string path)
{
    assert(std::filesystem::exists(path));
    std::ifstream ifs(path, std::ifstream::in);
    nlohmann::json configuration = nlohmann::json::parse(ifs);
    minervafs_root_folder = configuration.value("root_folder", minervafs_root_folder);
    if (minervafs_root_folder.at(0) == '~')
    {
        const char *home = getenv("HOME");
        if (home == NULL)
        {
            home = getpwuid(getuid())->pw_dir;
        }
        std::string home_directory(home);
        minervafs_root_folder.replace(0, 1, home_directory);
    }
}

static void setup()
{
    const std::string MKDIR = "mkdir";
    const std::string TOUCH = "touch";

    auto path_to_config = std::filesystem::path(get_binary_directory()).parent_path().parent_path().parent_path() / "minervafs.json";
    load_config(path_to_config.string());

    std::string base_directory = minervafs_root_folder;
    std::string config_file_path = base_directory + minervafs_config;
    std::string temp_directory = base_directory + minervafs_temp;
    std::string indexing_directory = base_directory + "/.indexing";

    if ( std::filesystem::exists(config_file_path))
    {
        nlohmann::json config = tartarus::readers::json_reader(config_file_path);
        minerva_storage = minerva::minerva(config);
        if (!std::filesystem::exists(temp_directory))
        {
            std::system((MKDIR + " " + temp_directory).c_str());
        }
        return;
    }

    // TODO if config exists load and return

    // TODO add missing folder

    if (!std::filesystem::exists(base_directory))
    {
        std::system((MKDIR + " " + base_directory).c_str());
    }


    if (!std::filesystem::exists(temp_directory))
    {
        std::system((MKDIR + " " + temp_directory).c_str());
    }

    nlohmann::json indexing_config;
    indexing_config["indexing_path"] = (indexing_directory);
    
    nlohmann::json minerva_config;
    minerva_config["fileout_path"] = (base_directory + "/");
    minerva_config["file_format"] = used_file_format;
    minerva_config["indexing_config"] = indexing_config;
    minerva_config["versioning"] = minverva_versioning;    

    minerva_storage = minerva::minerva(minerva_config);
    tartarus::writers::json_writer(config_file_path, minerva_config);
}

///
/// @param path const char* is path provide from the fuse file system impl
/// @return a path correct for the minerva folder
///
std::string get_permanent_path(const char* path)
{
    std::string internal_path(path);
    return minervafs_root_folder + "/" + internal_path.substr(1);
}

/**
 * Generate a temp path based on the path given in the FUSE call
 * @param   path const char* is path provide from the fuse file system impl
 * @return  a path in the temp of the minerva folder
 */
std::string get_temporary_path(const char* path)
{
    std::string internal_path(path);
    return minervafs_root_folder + minervafs_temp + "/" + internal_path.substr(1);
}

std::string get_minerva_relative_path(const char* path)
{
    std::string permanent_path = get_permanent_path(path);
    std::string root_folder = std::filesystem::path(minervafs_root_folder).string();
    std::string relative_path = "/" + permanent_path.substr(root_folder.length());
    return relative_path;
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

nlohmann::json get_code_params(size_t file_size)
{
    int m = 15;

    while (!((std::pow(2, m) * 2) <= file_size))
    {
        --m;
    }

    // We set a minum size of m = 15
    // Because it provides a minumum of 4kB bases
    // and the size of an I/O block is 4kB 
    if (m < 15)
    {
        m = 15;
    }
    // if (m < 3)
    // {
    //     m = 3; 
    // }


    nlohmann::json config;
    config["m"] = m;
    config["n"] = pow(2, m) / 8;
    config["r"] = 1;
    config["mgf"] = 256;
    config["code_type"] = codes::code_type::HAMMING_CODE;    

    return config;
}


void set_file_format(minerva::file_format file_format)
{
    used_file_format = file_format;
}

int encode(const char* path)
{
    std::string minerva_entry_temp_path = get_temporary_path(path);
    size_t file_size = std::filesystem::file_size(minerva_entry_temp_path);
//    std::vector<uint8_t> data = tartarus::readers::vector_disk_reader(minerva_entry_temp_path);

    nlohmann::json code_config = get_code_params(file_size);
    size_t n = code_config["n"].get<size_t>();

    if (n <= file_size)
    {
        // todo simple load all data 
    }
    else
    {
        // TODO: load part and encode
        size_t number_of_basis = file_size / n;
    }
    
    codewrapper::codewrapper* code = get_hamming_codec(code_config);
    std::string filename = get_minerva_relative_path(path);
    auto coded = code->encode(data);

    if (!minerva_storage.store(filename, static_cast<uint32_t>(file_size), coded, code_config))
    {
        std::cerr << "encode(" << path << "): Did not store file provided" << std::endl;
        return -errno;
    }
    sync();
    return 0;

}

int decode(const char* path)
{
    std::string minerva_entry_path = get_permanent_path(path);
    std::string minerva_entry_temp_path = get_temporary_path(path);
    std::string filename = get_minerva_relative_path(path);

    minerva::model::data coded_data = minerva_storage.load(filename);

    codewrapper::codewrapper* code = get_hamming_codec(coded_data.config());
    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> dd = coded_data.basis_and_deviation_pairs();
//    auto data = code->decode(coded_data.basis_and_deviation_pairs());
    auto data = code->decode(dd);
    
    // Make sure the parent directory is present for the file to be decoded in the right place
    std::filesystem::path target_path(minerva_entry_temp_path);
    std::filesystem::path parent_path = target_path.parent_path();
    if (!std::filesystem::is_directory(parent_path))
    {
        if (!std::filesystem::create_directories(parent_path))
        {
            std::cerr << "decode(" << path << "): Could not create parent directory to decode" << std::endl;
            return -1;
        }
    }

    if (!tartarus::writers::vector_disk_writer(minerva_entry_temp_path, std::vector<uint8_t>(data.begin(), data.begin() + coded_data.file_size())))
    {
        std::cerr << "decode(" << path << "): Could not write decoded data to " << minerva_entry_temp_path << std::endl;
        return -1;
    }
    return 0;
}

static bool is_flagged_for_modification(int flags)
{
    return  ((flags & O_WRONLY) == O_WRONLY)   ||
            ((flags & O_RDWR) == O_RDWR)       ||
            ((flags & O_EXCL) == O_EXCL)       ||
            ((flags & O_APPEND) == O_APPEND)   ||
            ((flags & O_CREAT) == O_CREAT)     ||
            ((flags & O_TMPFILE) == O_TMPFILE) ||
            ((flags & O_TRUNC) == O_TRUNC);
}
