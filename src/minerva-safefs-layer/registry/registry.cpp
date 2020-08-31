#include "registry.hpp"

#include <tartarus/writers.hpp>
#include <tartarus/readers.hpp>

#include <filesystem>

#include <sstream>

namespace minerva
{
    registry::registry() {}
    registry::registry(const nlohmann::json& configure) {(void) configure;}

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
