#include "utils.hpp"

#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
#include <string>

std::string get_binary_directory()
{
    char buffer[4096];
    std::string proc_path = "/proc/" + std::to_string(getpid()) + "/exe";
    ssize_t read = readlink(proc_path.c_str(), buffer, 4096);
    buffer[read] = '\0';
    std::filesystem::path full_path = std::filesystem::path(buffer);
    return full_path.parent_path().string();
}
