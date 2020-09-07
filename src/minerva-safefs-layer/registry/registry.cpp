#include "registry.hpp"

#include <tartarus/writers.hpp>
#include <tartarus/readers.hpp>

#include <filesystem>

#include <sstream>
#include <iostream>

#include <stdexcept>

namespace minerva
{
    registry::registry() {}

    registry::registry(const nlohmann::json& config)
    {

        if (config.find("fileout_path") == config.end())             
        {
            throw std::runtime_error("Missing fileout");
            // TODO: Throw exception
        }

        if (
            config.find("index_path") == config.end())             
        {
            throw std::runtime_error("index");
            // TODO: Throw exception
        }

        if (
            config.find("major_group_length") == config.end() ||
            config.find("minor_group_length") == config.end())             
        {
            throw std::runtime_error("major minor config");
            // TODO: Throw exception
        }        

        if (config.find("fileout_path") == config.end() ||
            config.find("index_path") == config.end() ||
            config.find("major_group_length") == config.end() ||
            config.find("minor_group_length") == config.end())             
        {
            throw std::runtime_error("Missing config");
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

        if (config.find("compression") != config.end())
        {

            auto compression_config = config["compression"].get<nlohmann::json>();

            if (compression_config.find("basis_size") == compression_config.end())
            {
                m_compressor = minerva::compressor(compression_config["algorithm"].get<minerva::compressor_algorithm>());
            }
            else
            {
                m_compressor = minerva::compressor(compression_config["algorithm"].get<minerva::compressor_algorithm>(), compression_config["basis_size"].get<size_t>());
            }
            m_compression = true; 
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
        else
        {
//            tartarus::writers::vector_disk_writer(m_fileout_path + "/" + path, data, true);
            tartarus::writers::vector_disk_writer(m_fileout_path + "/" + path, data);
        }
        
    }


    std::vector<uint8_t> registry::load_file(const std::string& path)
    {
        if (m_versioning)
        {
            return m_version.load_version(path);
        }
        
        return tartarus::readers::vector_disk_reader(m_fileout_path + "/" + path);
        
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
            std::filesystem::path basis_path(get_basis_path(it->first));
            if (!std::filesystem::exists(basis_path.parent_path()))
            {
                std::filesystem::create_directories(basis_path.parent_path());
            }
            
//            tartarus::writers::vector_disk_writer(basis_path.string(), it->second, true);

            auto basis = it->second;
            if (m_compression)
            {
                std::cout << "compress" << std::endl;
                m_compressor.compress(basis);
            }
            
            tartarus::writers::vector_disk_writer(basis_path.string(), basis);
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
            auto basis = tartarus::readers::vector_disk_reader(get_basis_path(it->first));
            if (m_compression)
            {
                std::cout << "uncompress" << std::endl;                
                m_compressor.uncompress(basis);
            }
            
            fingerprint_basis[it->first] = basis;
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
