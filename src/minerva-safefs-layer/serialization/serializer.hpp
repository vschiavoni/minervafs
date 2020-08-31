#ifndef MINERVA_SERIALIZER_HPP
#define MINERVA_SERIALIZER_HPP

#include "serializer.hpp"

#include <nlohmann/json.hpp>

#include <vector>
#include <map>
#include <cstdint>
#include <string>
#include <typeinfo>

namespace minerva
{
namespace serializer
{

/// Convert number to vector
template<typename Type>
inline void convert_natural_number(const Type input, std::vector<uint8_t>& output)
{
    output = std::vector<uint8_t>(sizeof(input));

    for (size_t i = 0; i < sizeof(input); ++i)
    {
        output.at(i) = (input >> (i * 8)); 
    }
}

/// Convert vector to number
template<typename Type>
inline void convert_natural_number(const std::vector<uint8_t>& input, Type& output)
{
    output = 0;
    for (size_t i = 0; i < input.size(); ++i)
    {
        Type next = input.at(i);
        next = next << (i * 8);
        output ^= next;
    }
}
    
/// Convert json to vector
    void convert_json(const nlohmann::json& input, std::vector<uint8_t>& out); 

/// Convert vector to json
    void convert_json(const std::vector<uint8_t>& input, nlohmann::json& out);
    
/// Convert fingerprints to vector
    void convert_fingerprints(const std::vector<std::vector<uint8_t>>& input, std::vector<uint8_t>& output);
    
/// Convert vector to fingerprints
    void convert_fingerprints(const std::vector<uint8_t>& input, size_t fingerprint_size, std::vector<std::vector<uint8_t>>& output);

/// Converts an input of index and deviation pairs to a vector of uint8_t 
    void convert_pairs(const std::vector<std::pair<uint64_t, std::vector<uint8_t>>>& input, std::vector<uint8_t>& output);
    
/// Converts a vector of uin8_t to list of index and deviation pairs 
    void convert_pairs(const std::vector<uint8_t>& input, size_t size_of_deviation, std::vector<std::pair<uint64_t, std::vector<uint8_t>>>& output);

    void convert_store_structure(const std::vector<std::vector<uint8_t>>& fingerprints,
                                 const std::vector<std::pair<uint64_t, std::vector<uint8_t>>>& pairs,
                                 const nlohmann::json& coding_config,
                                 const size_t file_size,
                                 std::vector<uint8_t>& output);

    void convert_store_structure(const std::vector<uint8_t>& input,
                                 std::vector<std::vector<uint8_t>>& fingerprints,
                                 std::vector<std::pair<uint64_t, std::vector<uint8_t>>>& pairs,
                                 nlohmann::json& config,
                                 size_t& file_size);

    void convert_index(std::map<std::vector<uint8_t>, uint64_t>& input, std::vector<uint8_t>& output);

    void convert_index(const std::vector<uint8_t>& input, std::map<std::vector<uint8_t>, uint64_t>& output);

} 
}





#endif /*MINERVA_SERIALIZER_HPP*/
