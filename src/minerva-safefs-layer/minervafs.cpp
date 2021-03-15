#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 39
#endif

#include "minerva_layer.hpp"

#include <fuse3/fuse.h>

#include <iostream> // Only for test
#include <string>
#include <cstring>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <filesystem>


static struct fuse_operations minerva_operations;

int main(int argc, char* argv[])
{

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv); 

    set_file_format(MSGPACK);
    minerva_operations.init = minerva_init;
    minerva_operations.destroy = minerva_destroy;
    minerva_operations.opendir = minerva_opendir;
    minerva_operations.readdir = minerva_readdir;
    minerva_operations.getattr = minerva_getattr;
    minerva_operations.access = minerva_access;
    minerva_operations.releasedir = minerva_releasedir;
    minerva_operations.write = minerva_write;
    minerva_operations.create = minerva_create;
    minerva_operations.open = minerva_open;
    minerva_operations.read = minerva_read;
    minerva_operations.release = minerva_release;
    minerva_operations.mknod  = minerva_mknod;
    minerva_operations.mkdir = minerva_mkdir;
    minerva_operations.rmdir = minerva_rmdir;
    minerva_operations.truncate = minerva_truncate;
    minerva_operations.chmod = minerva_chmod;
    minerva_operations.flush = minerva_flush;
    minerva_operations.rename = minerva_rename;
    minerva_operations.unlink = minerva_unlink;
    minerva_operations.utimens = minerva_utimens;
    minerva_operations.listxattr = minerva_listxattr;

    for (int i = 0; i < argc; ++i)
    {
        std::cout << std::string(argv[i]) << "\n"
    }
    std::cout << "\n"; 
    
    int cfg_index = 0; 
    for (int index = 0; index < argc; ++index)
    {
        if (0 == std::strcmp(argv[index], "--cfg"))
        {
            cfg_index = index;
            break; 
        }
    }

    if (cfg_index != 0)
    {
        if (cfg_index == argc - 1)
        {
            // TODO: report error the --cfg is the last option
            // and throw exception             
        }

        std::string config_file_path = std::string(argv[cfg_index + 1]);

        if (!std::fileystem::exists(config_file_path))
        {
            // TODO: report error of missing config file
            // and throw exception
        }

        // remove --cfg and config file path
        if (cfg_index != (argc - 2))
        {
            for (int index = cfg_index + 2; index < argc; ++index)
            {
                argv[index - 2] = argv[index]; 
            }
        }
        argc = argc - 2;

        for (int i = 0; i < argc; ++i)
        {
            std::cout << std::string(argv[i]) << "\n"
                }
        std::cout << "\n";         
    }    

    return fuse_main(argc, argv, &minerva_operations, NULL);
}
