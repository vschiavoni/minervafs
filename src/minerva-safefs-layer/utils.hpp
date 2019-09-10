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
 * Returns a text representation of coding parameters
 * @param params The paramters to 
 */
std::string stringify_code_params(codes::code_params params);