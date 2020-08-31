#include "registry.hpp"


#include <tartarus/writers.hpp>
#include <tartarus/readers.hpp>

#include <filesystem>

#include <sstream>

namespace minerva
{
    registry::registry() {}
    registry::registry(const nlohmann::json& config)
    {

        if (config.find("fileout_path") == config.end() ||
            config.find("index_path") == config.end() ||
            config.find("major_group_length") == config.end() ||
            config.find("minor_group_length") == config.end())             
        {
            // TODO: Throw exception
        }
        else
        {
            m_fileout_path = config["fileout_path"].get<std::string>();
            m_index_path = config["index_path"].get<std::string>();
            m_major_length = config["major_group_length"].get<size_t>();
            m_minor_length = config["minor_group_length"].get<size_t>();            
        }

        if (config.find("version_path") == config.end())
        {
            m_versioning = false;
        }
        else
        {
            // TODO load version config and create versioning
            m_versioning = true;
            m_version = minerva::version(config["version"].get<std::string>());
        }
        
        if (config.find("in_memory") == config.end())
        {
            m_in_memory = false; 
        }
        else
        {
            m_in_memory = config["in_memory"].get<bool>(); 
        }
    }

    void registry::write_file(const std::string& path, const std::vector<uint8_t>& data)
    {
        if (m_versioning)
        {
            m_version.store_version(path, data);
        }
    }

    
//    std::vector<uint8_t> registry::load_file(const std::string& path);    

    void registry::store_bases(const std::map<std::vector<uint8_t>, std::vector<uint8_t>>& fingerprint_basis)
    {

        std::map<std::vector<uint8_t>, std::vector<uint8_t>> to_store;

        for (auto it = fingerprint_basis.begin(); it != fingerprint_basis.end(); ++it)
        {
            if (!basis_exists(it->first))
            {
                to_store[it->first] = it->second; 
            }
        }

        for (auto it = to_store.begin(); it != to_store.end(); ++it)
        {
            tartarus::writers::vector_disk_writer(get_basis_path(it->first), it->second);
            if (m_in_memory)
            {
                m_in_memory_registry[it->first] = 1; 
            }
        }
    }

    void registry::load_bases(std::map<std::vector<uint8_t>, std::vector<uint8_t>>& fingerprint_basis)
    {
        for (auto it = fingerprint_basis.begin(); it != fingerprint_basis.end(); ++it)
        {
            fingerprint_basis[it->first] = tartarus::readers::vector_disk_reader(get_basis_path(it->first));    
        }
    }

    void registry::delete_bases(std::vector<std::vector<uint8_t>>& fingerprints)
    {
        for (const auto& fingerprint : fingerprints)
        {
            std::filesystem::remove(get_basis_path(fingerprint)); 
        }
    }
    
    bool registry::basis_exists(const std::vector<uint8_t>& fingerprint)
    {
        if (m_in_memory)
        {
            return m_in_memory_registry.find(fingerprint) != m_in_memory_registry.end() ? true : false;
        }

        return std::filesystem::exists(get_basis_path(fingerprint)) ? true : false;
    }

    std::string registry::convert_fingerprint_to_string(const std::vector<uint8_t>& fingerprint)
    {
        std::stringstream ss;

        for (const auto& elm : fingerprint)
        {
            ss << std::hex << (int) elm;
        }
        
        return ss.str();
    }

    std::string registry::get_basis_path(const std::vector<uint8_t>& fingerprint)
    {
        std::string fingerprint_str = convert_fingerprint_to_string(fingerprint);

        return m_index_path + "/" + fingerprint_str.substr(0, m_major_length) +
            "/" + fingerprint_str.substr((fingerprint.size() - 1 - m_minor_length)) + "/" + fingerprint_str;
        
    }
}
