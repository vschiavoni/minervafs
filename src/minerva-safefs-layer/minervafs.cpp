#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 39
#endif

#include "minerva_layer.hpp"

#include <fuse3/fuse.h>

#include <iostream> // Only for test
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <filesystem>


static struct fuse_operations minerva_operations;

int main(int argc, char* argv[])
{
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
    
    std::vector<std::string> args(argc);

    for (size_t index = 0; index < static_cast<size_t>(argc); ++index)
    {
        args[index] = (std::string(argv[index]));
    }

    for (const auto& elm : args)
    {
        std::cout << elm << "\n"; 
    }

    std::cout << "\n\n";

    auto arg_itr = std::find(std::begin(args), std::end(args), "-cfg");

    if (arg_itr != std::end(args) && (arg_itr + 1) != std::end(args))
    {
        auto config_file_path = *(arg_itr + 1);
        std::cout << "config: " <<  config_file_path << "\n";

        if (!std::filesystem::exists(config_file_path))
        {
            std::cout << "Config file does not exists" << std::endl; 
        }
        else
        {
            std::cout << "Mounting with log file: " << config_file_path << "\n";
        }
            

        int new_argc = argc - 2; 
        char* new_argv[new_argc]; 
        
        size_t cfg_index = 0;
        size_t new_args_index = 0;
        for (size_t index = 0; index < args.size(); ++index)
        {
            if (args[index] == "-cfg")
            {
                cfg_index = index;
            }
            else if (index != cfg_index && index != (cfg_index + 1))
            {
                new_argv[new_args_index] = argv[index];
                new_args_index = new_args_index + 1;
                std::cout << new_args_index << "\n\n";
            } 
        }
        return fuse_main(new_argc, new_argv, &minerva_operations, NULL);        
    }
    else
    {
        return fuse_main(argc, argv, &minerva_operations, NULL);
        // 
    }
    

    

    std::cout << "a\n";


}
