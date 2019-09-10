#include "utils.hpp"

#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
#include <string>


std::string stringify_code_params(codes::code_params params)
{
    std::string message = params.code_name +
        ", n = " + std::to_string(params.n) +
        ", k = " + std::to_string(params.k) +
        ", m = " + std::to_string(params.m) +
        ", mgf = " + std::to_string(params.mgf) +
        ", d = " + std::to_string(params.d) +
        ", r = " + std::to_string(params.r);
    return message;
}

std::string get_binary_directory()
{
    char buffer[4096];
    std::string proc_path = "/proc/" + std::to_string(getpid()) + "/exe";
    ssize_t read = readlink(proc_path.c_str(), buffer, 4096);
    buffer[read] = '\0';
    std::filesystem::path full_path = std::filesystem::path(buffer);
    return full_path.parent_path().string();
}