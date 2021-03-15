#include "minerva_layer.hpp"
#include "registry/registry.hpp"
#include "serialization/serializer.hpp"
#include "structures/file_structure.hpp"

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
#include <minerva/minerva.hpp> // TODO: REMOVE 
#include <minerva/model/data.hpp> // TODO REMOVE

#include <tartarus/model/coded_pair.hpp>

#include <tartarus/readers.hpp>
#include <tartarus/writers.hpp>

#include <harpocrates/hashing.hpp>
#include <aloha/aloha.hpp>

#include <nlohmann/json.hpp>

#include <cassert>

static std::string minervafs_root_folder = "~/.minervafs";
static const std::string minervafs_basis_folder = "/.basis/";
static const std::string minervafs_registry = "/.registry/";
static const std::string minervafs_identifier_register = "/identifiers";//"/.identifiers";
static const std::string minervafs_config = "/.minervafs_config";
static const std::string minervafs_temp = "/.temp"; // For temporarly decode files
static const std::vector<std::string> IGNORE = {".indexing", ".minervafs_config", ".temp"};

static const std::string minerva_config_path = ""; 

static bool minverva_versioning = false;
static bool minverva_compression = false;
static nlohmann::json minerva_versioning_config;
static nlohmann::json minverva_compression_config;
static nlohmann::json coding_config;
static nlohmann::json log_config; 

static minerva::registry registry; 
static codewrapper::codewrapper* coder;
static aloha::aloha alog; 

//static minerva::minerva minerva_storage;
static std::map<std::string, std::atomic_uint> open_files;
static std::mutex of_mutex;
static minerva::file_format used_file_format = minerva::file_format::JSON;

//TODO: we should deallocate this memory when exiting the application
thread_local codewrapper::codewrapper* codecs[20]={NULL, NULL, NULL, NULL, NULL,
						   NULL, NULL, NULL, NULL, NULL,
						   NULL, NULL, NULL, NULL, NULL,
						   NULL, NULL, NULL, NULL, NULL};

std::map<std::string, minerva::structures::file_structure> files;
std::map<std::string, std::vector<uint8_t>> file_remainder;


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
1045
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

    // Check if the file is decoded in the temp directory
    std::string minerva_entry_temp_path = get_temporary_path(path);
    if (std::filesystem::exists(minerva_entry_temp_path))
    {
        int res = lstat(minerva_entry_temp_path.c_str(), stbuf);
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
    stbuf->st_blocks = (blkcnt_t) ceil(((double) size) / 512);

    return 0;
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

// [[deprecated]]
// static void register_closed_file(std::string path)
// {
//     std::lock_guard<std::mutex> lock(of_mutex);
//     auto entry = open_files.find(path);
//     if (entry == open_files.end())
//     {
//         throw new std::logic_error("Cannot find file that must be closed");
//     }
//     entry->second--;
//     if (entry->second == 0)
//     {
//         open_files.erase(entry);
//         unlink(path.c_str());
//     }
// }

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

    (void) fi;
    // Get the place holder file path
    std::string minerva_entry_path = get_permanent_path(path);

    if (!std::filesystem::exists(minerva_entry_path))
    {
        return -errno;
    }

    // Load the placeholder
    auto raw_placeholder = tartarus::readers::vector_disk_reader(minerva_entry_path);

    std::vector<std::vector<uint8_t>> fingerprints;
    std::vector<std::pair<uint64_t, std::vector<uint8_t>>> pairs;
    nlohmann::json coding_config;
    size_t file_size;

    minerva::serializer::convert_store_structure(raw_placeholder, fingerprints, pairs, coding_config, file_size);

    raw_placeholder.clear(); // Clear the memory

    size_t chunk_size = (coder->configuration())["n"].get<size_t>();

    if (file_size <= (size_t) offset)
    {
        return 0;
    }

    off_t chunk_offset = 0;
    if (offset != 0)
    {
        chunk_offset = (offset / chunk_size); // Test that it does ceil
    }

    // Number of chunk to read
    size_t num_chunks = size / chunk_size;
    // If there is a remainder we have to account for this
    if ((size % chunk_size) != 0)
    {
        num_chunks = num_chunks + 1;
    }

    // Identify what bases we should load
    std::map<std::vector<uint8_t>, std::vector<uint8_t>> bases_to_read;
    for (size_t i = chunk_offset; i < (chunk_offset + num_chunks); ++i)
    {
        auto fingerprint = fingerprints.at(pairs.at(i).first);
        std::string fingerprint_str = to_hexadecimal(fingerprint);
        if (bases_to_read.find(fingerprint) == bases_to_read.end())
        {
            std::vector<uint8_t> basis;
            bases_to_read[fingerprint] = basis;
        }
    }
    // Load bases
    registry.load_bases(bases_to_read);

    // Build the list of pairs to decode
    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> coded_pairs(num_chunks);
    for (size_t i = chunk_offset, j = 0; i < (chunk_offset + num_chunks); ++i, ++j)
    {
        auto basis = bases_to_read[fingerprints.at(pairs.at(i).first)];
        auto pair = std::make_pair(basis, pairs.at(i).second);
        coded_pairs.at(j) = pair;
    }
    // Decode the pairs
    auto raw = coder->decode(coded_pairs);

    // Adjust the boundaries of the data to read in the decoded chunks
    auto to_read = size;
    if (raw.size() < to_read)
    {
       to_read = raw.size();
    }
    auto offset_in_first_chunk = offset % chunk_size;
    if (offset_in_first_chunk > 0)
    {
        to_read -= offset_in_first_chunk;
    }
    // Copy the data in the out buffer
    std::memcpy(buf, raw.data() + offset_in_first_chunk, to_read);

    // Return the amount of bytes read in the function
    return to_read;
}


int minerva_write(const char* path, const char *buf, size_t size, off_t offset,
                         struct fuse_file_info* fi)
{

    (void) fi;
    (void) offset;
    
    std::string origin_file_path(path);

    


    /// Setting up file object
    minerva::structures::file_structure file;
    
    if (files.find(origin_file_path) == files.end())
    {
        file.file_size = 0;
        file.bd_pairs = std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>();
        file.fingerprints = std::map<std::vector<uint8_t>, uint8_t>();
        files[origin_file_path] = file;
    }
    else
    {
        file = files[origin_file_path];
    }

    file.file_size += size;

    std::vector<uint8_t> data;

    // Check for previous data in remainder
    if (file_remainder.find(origin_file_path) != file_remainder.end())
    {
        std::vector<uint8_t> remainder = file_remainder.at(origin_file_path);
        file_remainder.erase(origin_file_path);

        data = std::vector<uint8_t>(remainder.size() + size);
        std::memcpy(data.data(), remainder.data(), remainder.size());
        std::memcpy(data.data() + remainder.size(), buf, size);
    }
    else
    {
        data = std::vector<uint8_t>(size);
        std::memcpy(data.data(), buf, size);
    }

    size_t chunk_size = (coder->configuration())["n"].get<size_t>();

    size_t num_chunks = data.size() / chunk_size;

    // Check if there is a new remainder
    if (data.size() % chunk_size != 0)
    {
        size_t remainder_size = data.size() - (num_chunks * chunk_size);
        auto remainder = std::vector<uint8_t>(remainder_size);
        std::memcpy(remainder.data(), data.data() + data.size() - remainder_size, remainder_size);
        file_remainder[origin_file_path] = remainder;
        data.resize(data.size() - remainder_size);
    }

    auto pairs = coder->encode(data);

    std::map<std::vector<uint8_t>, std::vector<uint8_t>> bases_to_store;

    for (const auto& pair : pairs)
    {
        std::vector<uint8_t> fingerprint;
        harpocrates::hashing::vectors::hash(pair.first, fingerprint, harpocrates::hashing::hash_type::SHA1);

        // Check if basis is new here:
        if (bases_to_store.find(fingerprint) == bases_to_store.end())
        {
            bases_to_store[fingerprint] = pair.first;
            file.fingerprints[fingerprint] = 1; 
        }

        auto bd_pair = std::make_pair(fingerprint, pair.second);
        // Add the basis and deviation pair
        file.bd_pairs.push_back(bd_pair);              
    }

    registry.store_bases(bases_to_store);

    files[origin_file_path] = file;
    return size;//data.size();    
    
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
    // std::string minerva_entry_temp_path = get_temporary_path(path);
    // if (!std::filesystem::exists(minerva_entry_temp_path))
    // {
    //     std::cerr << "minerva_release(" << path << "): Cannot find temporary entry for " << path << std::endl;
    //     if (!std::filesystem::exists(minerva_entry_path))
    //     {
    //         std::cerr << "minerva_release(" << path << "): Cannot find entry for " << path << std::endl;
    //         return -ENOENT;
    //     }
    //     return 0;
    // }

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
//        register_closed_file(minerva_entry_temp_path);
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

void set_config_path(std::string path)
{
    minerva_config_path = path; 
}

static void load_config(std::string path)

{
    assert(std::filesystem::exists(path));

    std::ifstream input(path);
    nlohmann::json configuration;

    // Loading configuration 
    input >> configuration;

    if (!configuration.contains("root_folder"))
    {
        // TODO: Throw error
    }

    minervafs_root_folder = configuration["root_folder"].get<std::string>(); 
    

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


    // TODO: change configuration key to transformation
    if (!configuration.contains("code"))
    {
        // TODO: throw excedption
    }

    coding_config = configuration["code"].get<nlohmann::json>();
    coder = new codewrapper::codewrapper(coding_config); 


    if (configuration.contains("compression"))
    {
        minverva_compression_config = configuration["compression"].get<nlohmann::json>();
        if (minverva_compression_config.find("basis_size") == minverva_compression_config.end())
        {
            minverva_compression_config["basis_size"] = coder->get_k(); 
        }
        minverva_compression = true;        
    }
    
    if (configuration.contains("versioning"))
    {
        minerva_versioning_config = configuration["versioning"].get<nlohmann::json>();
        minverva_versioning = true;
    }
}

static void setup()
{
    const std::string MKDIR = "mkdir";
    const std::string TOUCH = "touch";

    if (minerva_config_path.size() == 0)
    {

        auto _path = std::filesystem::path(get_binary_directory()).parent_path() / "minervafs.json";
        const std::string _spath = _path.string(); 
        set_config_path(_spath); 
    }
    

    load_config(minerva_config_path);

    alog = aloha::aloha();
    
    std::string base_directory = minervafs_root_folder;
    std::string config_file_path = base_directory + minervafs_config;
    std::string temp_directory = base_directory + minervafs_temp;
    std::string indexing_directory = base_directory + "/.indexing";

    if ( std::filesystem::exists(config_file_path))
    {
        nlohmann::json config = tartarus::readers::json_reader(config_file_path);
        registry = minerva::registry(config); 
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

//    nlohmann::json indexing_config;
//    indexing_config["indexing_path"] = (indexing_directory);
    
    nlohmann::json minerva_config;
    minerva_config["fileout_path"] = (base_directory + "/");
    minerva_config["index_path"] = (indexing_directory);    
    minerva_config["versioning"] = minverva_versioning;
    minerva_config["major_group_length"] = 2;
    minerva_config["minor_group_length"] = 2;    


    minerva_config["code"] = coding_config; 
    
    if (minverva_versioning)
    {
        minerva_config["versioning"] = minerva_versioning_config;
    }

    if (minverva_compression)
    {
        minerva_config["compression"] = minverva_compression_config;
    }
    

    std::cout << minerva_config.dump() << std::endl;
    registry = minerva::registry(minerva_config);     
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

    auto cpp_path = std::string(path);
    auto file = files[cpp_path];
    auto permanent_path = get_permanent_path(path);
    std::cout << permanent_path << std::endl;

    if (file_remainder.find(cpp_path) != file_remainder.end())
    {
        auto remainder = file_remainder[cpp_path];
        file_remainder.erase(cpp_path);
        
        auto pairs = coder->encode(remainder);

        std::map<std::vector<uint8_t>, std::vector<uint8_t>> bases_to_store;
    
        for (const auto& pair : pairs)
        {
            std::vector<uint8_t> fingerprint;
            harpocrates::hashing::vectors::hash(pair.first, fingerprint, harpocrates::hashing::hash_type::SHA1);
            if (file.fingerprints.find(fingerprint) == file.fingerprints.end())
            {
                bases_to_store[fingerprint] = pair.first;
                file.fingerprints[fingerprint] = 1; 
            }

            auto bd_pair = std::make_pair(fingerprint, pair.second);
            file.bd_pairs.push_back(bd_pair);            
        }
        registry.store_bases(bases_to_store);
    }

    std::map<std::vector<uint8_t>, uint64_t> fingerprints;
    std::vector<std::pair<uint64_t, std::vector<uint8_t>>> pairs(file.bd_pairs.size());

    size_t i = 0;
    uint64_t j = 0;
    
    for (const auto& pair : file.bd_pairs)
    {
        if (fingerprints.find(pair.first) == fingerprints.end())
        {
            fingerprints[pair.first] = i;
            ++i;
        }

        auto bd_pair = std::make_pair(fingerprints[pair.first], pair.second);
        pairs.at(j) = bd_pair;
        ++j;
    }

    std::vector<std::vector<uint8_t>> fingerprints_index(fingerprints.size());

    for (auto it = fingerprints.begin(); it != fingerprints.end(); ++it)
    {
        fingerprints_index.at(it->second) = it->first;
    }

    fingerprints.clear();
    

    std::vector<uint8_t> out;

    minerva::serializer::convert_store_structure(fingerprints_index, pairs, coder->configuration(), file.file_size, out);
    
    registry.write_file(permanent_path, out);
    files.erase(cpp_path);
//    close(fd);
    //  sync();

    // Delete the temporary file
    std::string minerva_entry_temp_path = get_temporary_path(path);
    if (std::filesystem::exists(minerva_entry_temp_path))
    {
        if (unlink(minerva_entry_temp_path.c_str()) == -1)
        {
            std::cerr << "Could not delete temporary file after encoding (" << minerva_entry_temp_path << ")" << std::endl;
        }
    }

    //std::filesystem::remove(entry_path);
    alog.info("Encoding completed");
    return 1;    
    
}


// int encode(const char* path)
// {
//     alog.info("Start encoding");
//     std::string minerva_entry_temp_path = get_temporary_path(path);
//     size_t file_size = std::filesystem::file_size(minerva_entry_temp_path);
    
// //    std::vector<uint8_t> data = tartarus::readers::vector_disk_reader(minerva_entry_temp_path);

// //    nlohmann::json code_config = get_code_params(file_size);
//     auto code_config = coder->configuration(); 
//     size_t n = code_config["n"].get<size_t>();


//     size_t number_of_chunks;
//     off_t bytes_loaded = 0;
//     size_t chunks_to_load = 100;
//     size_t remainder; 

//     if ((remainder = file_size % n) != 0)
//     {
//         number_of_chunks = (file_size - remainder) / n;
//         if (number_of_chunks == 0)
//         {
//             number_of_chunks = 1; 
//         }
//     }
//     else
//     {
//         number_of_chunks = file_size / n; 
//     }


//     chunks_to_load = 100;

//     if (chunks_to_load > number_of_chunks)
//     {
//         chunks_to_load = number_of_chunks; 
//     }
    
// //    codewrapper::codewrapper* coder = get_hamming_codec(code_config); // TODO: Use a single instance
    
    
//     int fd = open(minerva_entry_temp_path.c_str(), O_RDWR);
//     if (fd == -1)
//     {
//         // TODO: Throw error
//         return -1; 
//     }


//     std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> bases_deviation_map;//(number_of_chunks); 
//     size_t bd_pair_index = 0; 
//     while ((size_t)bytes_loaded < file_size)
//     {
//         size_t count = n * chunks_to_load;
//         if ((bytes_loaded + count) >= file_size && remainder)
//         {
//             count = file_size - bytes_loaded; 
//         }
        
//         std::vector<uint8_t> data(count);
//         pread(fd, data.data(), count, bytes_loaded);
//         bytes_loaded += count;

//         auto coded_data = coder->encode(data);
//         std::map<std::vector<uint8_t>, std::vector<uint8_t>> bases_to_store;
//         for (const auto& pair : coded_data)
//         {
//             std::vector<uint8_t> fingerprint;
//             harpocrates::hashing::vectors::hash(pair.first, fingerprint, harpocrates::hashing::hash_type::SHA1);

            
//             if (bases_to_store.find(fingerprint) == bases_to_store.end())
//             {
//                 bases_to_store[fingerprint] = pair.first; 
//             }
            
//             auto bd_pair = std::make_pair(fingerprint, pair.second);
//             bases_deviation_map.push_back(bd_pair);
// //            bases_deviation_map.at(bd_pair_index) = bd_pair;
//             ++bd_pair_index;
//         }

//         registry.store_bases(bases_to_store);         
//     }

//     std::map<std::vector<uint8_t>, uint64_t> fingerprint_index;
//     std::vector<std::pair<uint64_t, std::vector<uint8_t>>> pairs(bases_deviation_map.size());
//     uint64_t i = 0;
//     size_t j = 0;

//     for (auto it = bases_deviation_map.begin(); it != bases_deviation_map.end(); ++it)
//     {
//         if (fingerprint_index.find(it->first) == fingerprint_index.end())
//         {
//             fingerprint_index[it->first] = i;
//             ++i;
//         }
//         auto pair = std::make_pair(fingerprint_index[it->first], it->second);
//         pairs.at(j) = pair;
//         ++j;
//     }
    
//     std::vector<std::vector<uint8_t>> fingerprints(fingerprint_index.size());
//     for (auto it = fingerprint_index.begin(); it != fingerprint_index.end(); ++it)
//     {
//         fingerprints.at(it->second) = it->first;
//     }
//     fingerprint_index.clear();
//     std::vector<uint8_t> output;

//     minerva::serializer::convert_store_structure(fingerprints, pairs, coder->configuration(), file_size, output);
//     fingerprints.clear();
//     pairs.clear();
//     registry.write_file(path, output);
//     output.clear();

//     close(fd);
//     sync();

//     std::filesystem::remove(minerva_entry_temp_path);
//     alog.info("Encoding completed");
//     return 0;

    
    
//     // codewrapper::codewrapper* code = get_hamming_codec(code_config);
//     // std::string filename = get_minerva_relative_path(path);
//     // auto coded = code->encode(data);

//     // if (!minerva_storage.store(filename, static_cast<uint32_t>(file_size), coded, code_config))
//     // {
//     //     std::cerr << "encode(" << path << "): Did not store file provided" << std::endl;
//     //     return -errno;
//     // }

// }

int decode(const char* path)
{
    std::string minerva_entry_path = get_permanent_path(path);
    std::string minerva_entry_temp_path = get_temporary_path(path);
    std::string filename = get_minerva_relative_path(path);

    auto input = registry.load_file(filename); //minerva_storage.load(filename);

    std::vector<std::vector<uint8_t>> fingerprints;
    std::vector<std::pair<uint64_t, std::vector<uint8_t>>> pairs;
    nlohmann::json config;
    size_t file_size = 0;

    minerva::serializer::convert_store_structure(input, fingerprints, pairs, config, file_size);
    input.clear();

    std::map<std::vector<uint8_t>, std::vector<uint8_t>> fingerprint_basis;
    for (const auto& fingerprint : fingerprints)
    {
        std::vector<uint8_t> basis;
        fingerprint_basis[fingerprint] = basis;
    }

    registry.load_bases(fingerprint_basis);
    
    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> coded_pairs(pairs.size());
    size_t i = 0;

    

    for (auto it = pairs.begin(); it != pairs.end(); ++it)
    {
        auto pair = std::make_pair(fingerprint_basis[fingerprints.at(it->first)], it->second);
        it->second.clear();
        coded_pairs.at(i) = pair;
        ++i;
    }

    fingerprints.clear();
    fingerprint_basis.clear();
    pairs.clear();

//    codewrapper::codewrapper code(config);
    auto data = coder->decode(coded_pairs);
    coded_pairs.clear();

//    codewrapper::codewrapper* code = get_hamming_codec(coded_data.config());
// //     // std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> dd = coded_data.basis_and_deviation_pairs();
// // //    auto data = code->decode(coded_data.basis_and_deviation_pairs());
//    auto data = code->decode(dd);
    
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

    if (!tartarus::writers::vector_disk_writer(minerva_entry_temp_path, std::vector<uint8_t>(data.begin(), data.begin() + file_size)))
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
