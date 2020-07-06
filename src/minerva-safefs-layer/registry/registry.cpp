#include "registry.hpp"

#include <tartarus/writer.hpp>

#include <filesystem>

#include <sstream>

namespace minerva
{

    void registry::store_bases(const std::map<std::vector<uint8_t>, std::vector<uint8_t>>& fingerprint_basis)
    {

        for (auto it = fingerprint_basis.begin(); it != fingerprint_basis.end(); ++it)
        {
            if (!basis_exists(it->first))
            {
                store_basis(it->first, it->second);
            }
        }
    }

    void registry::store_basis(const std::vector<uint8_t>& fingerprint, const std::vector<uint8_t>& basis);
    {
        tartarus::vector_disk_writer(get_basis_path(fingerprint), basis);
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
