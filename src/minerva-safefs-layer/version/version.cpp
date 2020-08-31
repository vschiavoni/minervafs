#include "version.hpp"

#include <tartarus/writers.hpp>
#include <tartarus/readers.hpp>

#include <filesystem>

namespace minerva
{
    version::version() {}

    version::version(const std::string& version_path)
    {
        m_version_path = version_path;
        if (version_path.size() > 0 && version_path.substr(version_path.size() - 1) != "/")
        {
            m_version_path = m_version_path + "/";
        }

        if (!std::filesystem::exists(m_version_path))
        {
            std::filesystem::create_directories(m_version_path);
        }
    }

    void version::store_version(const std::string& file_path, const std::vector<uint8_t>& data)
    {
        if (!std::filesystem::exists(m_version_path + file_path))
        {
            std::filesystem::create_directories(m_version_path + file_path);
        }
        
        std::string write_path = create_version_path(file_path);

        tartarus::writers::vector_disk_writer(write_path, data); 
    }

    std::vector<uint8_t> version::load_version(std::string file_path)
    {

        std::string read_path = current_version_path(file_path);
        return tartarus::readers::vector_disk_reader(read_path);

    }

    std::string version::current_version_path(const std::string& path)
    {

        if (!std::filesystem::exists(m_version_path + path))
        {

        }

        int current = 0;

        for (const auto& entry : std::filesystem::directory_iterator(m_version_path + path))
        {
            (void) entry;
            ++current;
        }

        if (path.at(path.size() - 1) != '/')
        {
            return m_version_path + path + "/" + std::to_string(current);
        }
        else
        {
            return m_version_path + path + std::to_string(current);
        }
    }

    uint32_t version::next_version(const std::string& path)
    {
        if (!std::filesystem::exists(m_version_path + path))
        {
            return 1;
        }

        uint32_t newest = 1;

        for (const auto& entry: std::filesystem::directory_iterator(m_version_path + path))
        {
            (void) entry;
            ++newest;
        }

        return newest;
    }

    std::string version::create_version_path(const std::string& path, const uint32_t version)
    {
        if (path.at(path.size() - 1) != '/')
        {
            return m_version_path + path + "/" + std::to_string(version);
        }
        else
        {
            return m_version_path + path + std::to_string(version);
        }
    }

    std::string version::create_version_path(const std::string& path)
    {
        return create_version_path(path, next_version(path)); 
    }    

    bool version::is_first_version(const std::string& path)
    {
        // If exists it is not the first version 
        return std::filesystem::exists(path) ? false : true;
    }    
}
