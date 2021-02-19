#include "utils.hpp"

#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
#include <iomanip>
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

std::string to_hexadecimal(std::vector<uint8_t> data)
{
    std::stringstream representation;
    for (uint8_t &number: data)
    {
        representation << std::hex << std::setfill('0') << std::setw(2) << (int) number;
    }
    return representation.str();
}