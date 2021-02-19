#pragma once

#include <string>
#include <vector>

#include <codewrapper/codewrapper.hpp>

/**
 * A structure representing the folder organization of the minervafs backend folder
 */
struct minerva_hierarchy {
    std::string root_folder;
    std::string basis_folder;
    std::string temp_folder;
    std::string registry_folder;
    std::string identifier_register;
    std::string minervafs_config;
    std::vector<std::string> to_be_ignored;
};

/**
 * Returns the path of the directory hosting the running binary
 * @return Path to the directory containing the binary invoking the function
 */
std::string get_binary_directory();

/**
 * Returns a string representing a vector of bytes as a hexadecimal string
 * @param data The vector to represent
 * @returns A hexadecimal representation of the vector
 */
std::string to_hexadecimal(std::vector<uint8_t> data);
