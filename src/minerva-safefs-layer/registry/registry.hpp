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
    registry(const nlohmann::json& configure);

    void store_bases(const std::map<std::vector<uint8_t>, std::vector<uint8_t>>& fingerprint_basis);

private:

    void store_basis(const std::vector<uint8_t>& fingerprint, const std::vector<uint8_t>& basis);
    bool basis_exists(const std::vector<uint8_t>& fingerprint);


    std::string convert_fingerprint_to_string(const std::vector<uint8_t>& fingerprint);
    
    std::string get_basis_path(const std::vector<uint8_t>& fingerprint);
    
private:

    bool m_in_memory;
    std::map<std::vector<uint8_t>, size_t> m_in_memory_registry;
    std::string m_index_path;
    size_t m_major_length;
    size_t m_minor_length;
};
}
