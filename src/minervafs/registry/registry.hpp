#include "../version/version.hpp"
#include "../compression/compressor.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>

#include <vector>
#include <map>

namespace minerva
{
class registry
{
public:
    registry();    
    registry(const nlohmann::json& config);

    void write_file(const std::string& path, const std::vector<uint8_t>& data);
    std::vector<uint8_t> load_file(const std::string& path);
    
    void store_bases(const std::map<std::vector<uint8_t>, std::vector<uint8_t>>& fingerprint_basis);
    void load_bases(std::map<std::vector<uint8_t>, std::vector<uint8_t>>& fingerprint_basis);
    void delete_bases(std::vector<std::vector<uint8_t>>& fingerprints); 

private:
    
    bool basis_exists(const std::vector<uint8_t>& fingerprint);

    std::string convert_fingerprint_to_string(const std::vector<uint8_t>& fingerprint);
    
    std::string get_basis_path(const std::vector<uint8_t>& fingerprint);
    
private:

    bool m_in_memory;

    bool m_versioning;
    bool m_compression; 

    minerva::version m_version;
    minerva::compressor m_compressor; 
    
    std::map<std::vector<uint8_t>, size_t> m_in_memory_registry;
    
    std::string m_index_path;
    std::string m_fileout_path;
    
    size_t m_major_length;
    size_t m_minor_length;
};
}
