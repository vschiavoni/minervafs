#ifndef MINERVA_VERSION_HPP
#define MINERVA_VERSION_HPP

#include <nlohmann/json.hpp>

#include <string>
#include <cstdint>
#include <vector>

namespace minerva
{
class version
{

public:
    version();
    version(const std::string& version_path);

    void store_version(const std::string& file_path, const std::vector<uint8_t>& data);

    std::vector<uint8_t> load_version(std::string file_path);

private:

    std::string current_version_path(const std::string& path);
    uint32_t next_version(const std::string& path);

    std::string create_version_path(const std::string& path, const uint32_t version);
    std::string create_version_path(const std::string& path);

    bool is_first_version(const std::string& path);

private:

    std::string m_version_path;
    
};
}

#endif /* MINERVA_VERSION_HPP */
