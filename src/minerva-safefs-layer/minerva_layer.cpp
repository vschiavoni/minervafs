#include "minerva_layer.hpp"

#include <atomic>
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
#include <tartarus/model/coded_data.hpp>
#include <tartarus/model/coded_pair.hpp>
#include <tartarus/model/raw_data.hpp>
#include <tartarus/readers.hpp>
#include <tartarus/writers.hpp>

#include <nlohmann/json.hpp>

#include <chrono>
#include <sstream>
#include <fstream>


static const std::string minervafs_root_folder = "/.minervafs";
static const std::string minervafs_basis_folder = "/.basis/";
static const std::string minervafs_registry = "/.registry/";
static const std::string minervafs_identifier_register = "/identifiers";//"/.identifiers";
static const std::string minervafs_config = "/.minervafs_config";
static const std::string minervafs_temp = "/.temp"; // For temporarly decode files
static const std::vector<std::string> IGNORE = {".indexing", ".minervafs_config", ".temp"};

static std::string USER_HOME = "";

static minerva::minerva minerva_storage;
static std::map<std::string, std::atomic_uint> open_files;
static std::mutex of_mutex;
static minerva::file_format used_file_format = minerva::file_format::JSON;

static std::map<std::string, nlohmann::json> results;
static std::string result_path = "/tmp/minervafs_measurement_result.json"; 

// Helper functions

void setup();

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
 * Compute path of user's home directory
 * @return Path to the home directry
 */
inline std::string get_user_home();

/**
 * Tests whether a flag combination implies that the file is open for modification
 * @param flags Combination of flags to test
 * @return true if the flags are set for modification, false otherwise
 */
static bool is_flagged_for_modification(int flags);

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


std::string convert_result_to_csv_entry(nlohmann::json result)
{

    uint64_t total_time = result["total_time"].get<uint64_t>();
    uint64_t fs_op_time = result["fs_op_time"].get<uint64_t>(); 
    uint64_t minerva_operation_time = result["operation_time"].get<uint64_t>();

    uint64_t coding_time = result["coding_time"].get<uint64_t>();
    uint64_t hashing_time = result["hashing_time"].get<uint64_t>();
    uint64_t index_time = result["index_time"].get<uint64_t>();
    uint64_t release_time = result["release_time"].get<uint64_t>();

    std::stringstream ss;
    ss << "\n" << total_time << ","
       << fs_op_time << ","        
       << minerva_operation_time << ","
       << coding_time << ","
       << hashing_time << ","
       << index_time << "," 
       << release_time; 
    return ss.str(); 
}

void append_result(nlohmann::json result)
{
    std::string str = convert_result_to_csv_entry(result); 
    std::ofstream result_out;
    result_out.open(result_path, std::ios_base::app);
    result_out << str;
    result_out.close();          
}
/*static*/ void* minerva_init(struct fuse_conn_info *conn)
{
    (void) conn;
    setup();
    std::string header = "total time, fs operation time, minerva operation time, coding time, hashing time, index time, release time";
    std::ofstream result_out(result_path);
    result_out << header;
    result_out.close();  
        
    return 0;
}

void minerva_destroy(void *private_data)
{
    
    (void) private_data;

    std::filesystem::remove_all(get_temporary_path(" "));
}

// Base operation

/*static*/ int minerva_getattr(const char* path, struct stat* stbuf)
{
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
        nlohmann::json obj = tartarus::readers::msgpack_reader(minerva_entry_path);
        size = obj["file_size"].get<size_t>();
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
        unlink(path.c_str());
    }
}

/*static*/ int minerva_create(const char *path, mode_t mode, struct fuse_file_info* fi)
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

/*static*/ int minerva_open(const char* path, struct fuse_file_info* fi)
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

/*static*/ int minerva_read(const char *path, char *buf, size_t size, off_t offset,
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
/*static*/ int minerva_write(const char* path, const char *buf, size_t size, off_t offset,
                         struct fuse_file_info* fi)
{


    nlohmann::json measurement;
    std::string res_name = std::string(path);
    if (results.count(res_name) == 1)
    {
        measurement = results[res_name];
    }
    else
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto start_time = static_cast<uint64_t>(start.time_since_epoch().count());
        measurement["start_time"] = start_time;
        measurement["fs_op_time"] = 0;
    }

    auto write_start = std::chrono::high_resolution_clock::now();

    
    
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

    auto write_end = std::chrono::high_resolution_clock::now();
    auto write_time = std::chrono::duration_cast<std::chrono::microseconds>(write_end - write_start);

    measurement["fs_op_time"] = measurement["fs_op_time"].get<uint64_t>() + static_cast<uint64_t>(write_time.count());
    results[res_name] = measurement;
    
    return res;
}

/*static*/ int minerva_release(const char* path, struct fuse_file_info *fi)
{

    auto release_start = std::chrono::high_resolution_clock::now();
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

    auto release_end = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();
    auto end_time = static_cast<uint64_t>(end.time_since_epoch().count());
    auto release_time = std::chrono::duration_cast<std::chrono::microseconds>(release_end - release_start);
    
    
    std::string res_name = std::string(path);
    auto measurement = results[res_name]; 
    measurement["end_time"] = end_time;
    measurement["release_time"] = static_cast<uint64_t>(release_time.count()); 
    uint64_t start_time = measurement["start_time"].get<uint64_t>();
    measurement["total_time"] = (end_time - start_time); 
    append_result(measurement);

    results.erase(res_name); 
    
    return 0;
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

/*static*/ int minerva_chmod(const char* path, mode_t mode)
{
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
/*static*/ int minerva_mknod(const char *path, mode_t mode, dev_t rdev)
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

/*static*/ int minerva_mkdir(const char *path, mode_t mode)
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
/*static*/ int minerva_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                           off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;
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

/*static*/ int minerva_rmdir(const char *path)
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

/*static*/ int minerva_flush(const char* path, struct fuse_file_info* fi) {
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

/*static*/ int minerva_rename(const char* from, const char* to) {
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

/*static*/ int minerva_unlink(const char* path)
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

/*static*/ int minerva_utimens(const char* path, const struct timespec tv[2])
{
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

// Helper functions



void setup()
{
    const std::string MKDIR = "mkdir";
    const std::string TOUCH = "touch";

    std::string base_directory = get_user_home() + minervafs_root_folder;
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
    return get_user_home() + minervafs_root_folder + "/" + internal_path.substr(1);
}

/**
 * Generate a temp path based on the path given in the FUSE call
 * @param   path const char* is path provide from the fuse file system impl
 * @return  a path in the temp of the minerva folder
 */
std::string get_temporary_path(const char* path)
{
    std::string internal_path(path);
    return get_user_home() + minervafs_root_folder + minervafs_temp + "/" + internal_path.substr(1);
}

std::string get_minerva_relative_path(const char* path)
{
    std::string permanent_path = get_permanent_path(path);
    std::string root_folder = std::filesystem::path(get_user_home() + "/" + minervafs_root_folder).string();
    std::string relative_path = "/" + permanent_path.substr(root_folder.length());
    return relative_path;
}

inline std::string get_user_home()
{
    if (USER_HOME.empty()) {
        char* homedir;
        if ((homedir = getenv("HOME")) != NULL)
        {
            homedir = getpwuid(getuid())->pw_dir;
        }
        USER_HOME = std::string(homedir, strlen(homedir));
    }
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
    params.mgf = 256;
    params.n = pow(2,m);
    params.k = params.n - params.m - 1;
    params.d = 4;
    params.r = 2;
    
    return params;
}

static std::string stringify_code_params(codes::code_params params) {
    std::string message = params.code_name +
        ", n = " + std::to_string(params.n) +
        ", k = " + std::to_string(params.k) +
        ", m = " + std::to_string(params.m) +
        ", mgf = " + std::to_string(params.mgf) +
        ", d = " + std::to_string(params.d) +
        ", r = " + std::to_string(params.r);
    return message;
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

int encode(const char* path)
{

    std::string minerva_entry_temp_path = get_temporary_path(path);
    size_t file_size = std::filesystem::file_size(minerva_entry_temp_path);
    std::vector<uint8_t> data = tartarus::readers::vector_disk_reader(minerva_entry_temp_path);

    codes::code_params code_param = get_code_params(file_size);
    codewrapper::CodeWrapper code(code_param);
    std::string filename = get_minerva_relative_path(path);
    std::string mime_type = "TODO"; // TODO: Change;
    tartarus::model::raw_data raw {0, static_cast<uint32_t>(file_size), filename, mime_type, data};
    auto encode_start = std::chrono::high_resolution_clock::now();
    tartarus::model::coded_data coded = code.encode_data(raw);
    auto encode_end = std::chrono::high_resolution_clock::now();    
    std::cout << "encode(" << path << "): "  << stringify_code_params(code_param) << std::endl;

    uint64_t hashing_time = 0;
    uint64_t index_time = 0; 
    auto storage_start = std::chrono::high_resolution_clock::now();

    if (!minerva_storage.store(coded, hashing_time, index_time))
    {
        std::cerr << "encode(" << path << "): Did not store file provided" << std::endl;
        return -errno;
    }
    auto storage_end = std::chrono::high_resolution_clock::now();

    std::string res_name = std::string(path);

    auto measurement = results[res_name];

    auto encoding_time = std::chrono::duration_cast<std::chrono::microseconds>(encode_end-encode_start);

    measurement["coding_time"] = static_cast<uint64_t>(encoding_time.count());
    
    auto storage_time = std::chrono::duration_cast<std::chrono::microseconds>(storage_end-storage_start);
    measurement["operation_time"] = static_cast<uint64_t>(storage_time.count());
    measurement["hashing_time"] = hashing_time;
    measurement["index_time"] = index_time; 
    results[res_name] = measurement; 
    return 0;

}

int decode(const char* path)
{
    std::string minerva_entry_path = get_permanent_path(path);
    std::string minerva_entry_temp_path = get_temporary_path(path);
    std::string filename = get_minerva_relative_path(path);

    tartarus::model::coded_data coded_data = minerva_storage.load(filename);
    codes::code_params params = extract_code_params(coded_data.coding_configuration);
    codewrapper::CodeWrapper coder(params);
    tartarus::model::raw_data data = coder.decode_data(coded_data);
    if (!tartarus::writers::vector_disk_writer(minerva_entry_temp_path, data.data))
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
