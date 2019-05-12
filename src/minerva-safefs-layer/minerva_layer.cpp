#include "minerva_layer.hpp"

#include <pwd.h>
#include <unistd.h>

#include <cstring> 
#include <string>

#include <vector> 

const static std::string minerva_path = ".minerva";
const static std::string basis_path = ".basis";
const static std::string registry_path = ".registry";

std::string get_home_directory()
{
   char* homedir;
    if ((homedir = getenv("HOME")) != NULL)
    {
        homedir = getpwuid(getuid())->pw_dir;
        return std::string(homedir, strlen(homedir));
    }
    else
    {
        return "";
    }
}

void exec_command(std::string cmd, std::string input, std::vector<std::string> params)
{
    std::string full = cmd + " ";
    for(const auto& param : params)
    {
        full = full + param + " ";
    }
    full = full + input;
    std::system(full.c_str());
}

void mkdir(std::string path)
{
    std::vector<std::string> params;
    exec_command("mkdir", path, params);
}

void touch(std::string path)
{
    std::vector<std::string> params;
    exec_command("touch", path, params);    
}

int minerva_init(struct fuse_operations** fuse_operations)
{

    std::string homedir = get_home_directory();

    // TODO: Check if exists before setup
    mkdir((homedir + "/" + minerva_path));
    mkdir((homedir + "/" + minerva_path + "/" + basis_path));
    touch((homedir + "/" + minerva_path + "/" + registry_path));

    // TODO:  If exists load inforamtion

    // TODO: SETUP minerva
}
