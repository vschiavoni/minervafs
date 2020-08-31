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
inline void convert_json(const nlohmann::json& input, std::vector<uint8_t>& out)
{
    std::string json_as_string = input.dump();
    out = std::vector<uint8_t>(json_as_string.length());
    for (size_t i = 0; i < json_as_string.length(); ++i)
    {
        out.at(i) = (uint8_t) json_as_string.at(i); 
    }
}

/// Convert vector to json
inline void convert_json(const std::vector<uint8_t>& input, nlohmann::json& out)
{
    std::string json_as_string;
    for (const auto& b : input)
    {
        json_as_string += (char) b; 
    }
    out = nlohmann::json::parse(json_as_string); 
}

/// Convert fingerprints to vector
inline void convert_fingerprints(const std::vector<std::vector<uint8_t>>& input, std::vector<uint8_t>& output)
{
    //output = std::vector<uint8_t>(input.at(0).size() * input.size());
    if (input.empty())
    {
        return;
    }
    output.reserve(input.at(0).size() * input.size());

    size_t offset = 0;
    for (const auto& fingerprint : input)
    {
//        std::memcpy(output.data() + offset, fingerprint.data(), fingerprint.size()); 
        output.insert(output.begin() + offset, fingerprint.begin(), fingerprint.end());
        offset += fingerprint.size(); 
    }

}

/// Convert vector to fingerprints
inline void convert_fingerprints(const std::vector<uint8_t>& input, size_t fingerprint_size, std::vector<std::vector<uint8_t>>& output)
{
    size_t number_of_fingerprints = input.size() / fingerprint_size;


    // Allocating space for all vectors, so we don't have to do it on the fly 
    output = std::vector<std::vector<uint8_t>>(number_of_fingerprints, std::vector<uint8_t>(fingerprint_size));

    for (size_t i = 0; i < number_of_fingerprints; ++i)
    {
        std::vector<uint8_t> fingerprint(fingerprint_size);
//        input.begin() + (i * fingerprint_size), input.end() + ((i * fingerprint_size) + fingerprint_size)
        std::memcpy(fingerprint.data(), input.data() + (i * fingerprint_size), fingerprint_size); 
            
        output.at(i) = fingerprint;                                          
    }
}

    

/// Converts an input of index and deviation pairs to a vector of uint8_t 
inline void convert_pairs(const std::vector<std::pair<uint64_t, std::vector<uint8_t>>>& input, std::vector<uint8_t>& output)
{
//    output = std::vector<uint8_t>((sizeof(input.at(0).first) + input.at(0).second.size()) * input.size());
    if (input.empty())
    {
        return;
    }
    output.reserve((sizeof(input.at(0).first) + input.at(0).second.size()) * input.size());
    
    size_t offset = 0;
    for (const auto& pair : input)
    {
        std::vector<uint8_t> index; 
        convert_natural_number(pair.first, index);

        output.insert(output.begin() + offset, index.begin(), index.end());
        offset += index.size();
        output.insert(output.begin() + offset, pair.second.begin(), pair.second.end());
        offset += pair.second.size();        
    }
}

/// Converts a vector of uin8_t to list of index and deviation pairs 
inline void convert_pairs(const std::vector<uint8_t>& input, size_t size_of_deviation, std::vector<std::pair<uint64_t, std::vector<uint8_t>>>& output)
{
    size_t offset = 0;

    while (offset < input.size())
    {
        // Copy index content to index vector
        std::vector<uint8_t> index(input.begin() + offset, input.begin() + offset + sizeof(uint64_t));
        offset += sizeof(uint64_t);

        std::vector<uint8_t> deviation(input.begin() + offset, input.begin() + offset + size_of_deviation);
        offset += size_of_deviation;

        uint64_t index_as_number;
        convert_natural_number(index, index_as_number);
        output.push_back(std::make_pair(index_as_number, deviation));
    }
}

inline void convert_store_structure(const std::vector<std::vector<uint8_t>>& fingerprints,
                                    const std::vector<std::pair<uint64_t, std::vector<uint8_t>>>& pairs,
                                    const nlohmann::json& coding_config,
                                    const size_t file_size,
                                    std::vector<uint8_t>& output)
{

    std::vector<uint8_t> file_size_data;
    convert_natural_number(file_size, file_size_data);


    std::vector<uint8_t> config_data;
    convert_json(coding_config, config_data);

    std::vector<uint8_t> config_size;
    convert_natural_number(config_data.size(), config_size); 
    
    std::vector<uint8_t> fingerprints_data;
    convert_fingerprints(fingerprints, fingerprints_data);

    std::vector<uint8_t> fingerprints_size;
    convert_natural_number(fingerprints_data.size(), fingerprints_size); 

    //FIXME this will bug if there are no fingerprints
    // What about writing zero here
    std::vector<uint8_t> fingerprint_size;
    if (!fingerprints.empty())
    {
        convert_natural_number(fingerprints.at(0).size(), fingerprint_size);
    }
    else
    {
        convert_natural_number((size_t) 0, fingerprint_size);
    }

    //FIXME this will bug if there are no fingerprints
    // What about writing zero here
    std::vector<uint8_t> deviation_size;
    if (!pairs.empty())
    {
        convert_natural_number(pairs.at(0).second.size(), deviation_size);
    }
    else
    {
        convert_natural_number((size_t) 0, deviation_size);
    }
    
    
    std::vector<uint8_t> pairs_data;
    convert_pairs(pairs, pairs_data);

    std::vector<uint8_t> pairs_size;
    convert_natural_number(pairs_data.size(), pairs_size);

    output.reserve(file_size_data.size() + 
                   config_data.size() + config_size.size() +
                   fingerprints_data.size() + fingerprints_size.size() + fingerprint_size.size() +
                   pairs_data.size() + pairs_size.size() +
                   deviation_size.size());
    
    output.insert(output.begin(), file_size_data.begin(), file_size_data.end());
    size_t offset = file_size_data.size();
    
    output.insert(output.begin() + offset, config_size.begin(), config_size.end());
    offset += config_size.size();

    output.insert(output.begin() + offset, fingerprints_size.begin(), fingerprints_size.end());
    offset += fingerprints_size.size(); 

    output.insert(output.begin() + offset, fingerprint_size.begin(), fingerprint_size.end());
    offset += fingerprint_size.size();

    output.insert(output.begin() + offset, pairs_size.begin(), pairs_size.end());
    offset += pairs_size.size();

    output.insert(output.begin() + offset, deviation_size.begin(), deviation_size.end());
    offset += deviation_size.size();    

    output.insert(output.begin() + offset, config_data.begin(), config_data.end());
    offset += config_data.size();

    output.insert(output.begin() + offset, fingerprints_data.begin(), fingerprints_data.end());
    offset += fingerprints_data.size();

    output.insert(output.begin() + offset, pairs_data.begin(), pairs_data.end());
}

inline void convert_store_structure(const std::vector<uint8_t>& input,
                                    std::vector<std::vector<uint8_t>>& fingerprints,
                                    std::vector<std::pair<uint64_t, std::vector<uint8_t>>>& pairs,
                                    nlohmann::json& config,
                                    size_t& file_size)
{
    convert_natural_number(std::vector<uint8_t>(input.begin(), input.begin() + sizeof(size_t)), file_size);
    
    size_t offset = sizeof(size_t);

    size_t config_size;
    convert_natural_number(std::vector<uint8_t>(input.begin() + offset, input.begin() + offset + sizeof(size_t)), config_size);
    offset += sizeof(size_t);

    size_t fingerprints_size;
    convert_natural_number(std::vector<uint8_t>(input.begin() + offset, input.begin() + offset + sizeof(size_t)), fingerprints_size);
    offset += sizeof(size_t);

    size_t fingerprint_size;
    convert_natural_number(std::vector<uint8_t>(input.begin() + offset, input.begin() + offset + sizeof(size_t)), fingerprint_size);
    offset += sizeof(size_t);

    size_t pairs_size;
    convert_natural_number(std::vector<uint8_t>(input.begin() + offset, input.begin() + offset + sizeof(size_t)), pairs_size);
    offset += sizeof(size_t);

    size_t deviation_size;
    convert_natural_number(std::vector<uint8_t>(input.begin() + offset, input.begin() + offset + sizeof(size_t)), deviation_size);
    offset += sizeof(size_t);
    

    convert_json(std::vector<uint8_t>(input.begin() + offset, input.begin() + offset + config_size), config);
    (void) config; 
    offset += config_size;
    
    if (fingerprints_size > 0)
    {
        convert_fingerprints(std::vector<uint8_t>(input.begin() + offset, input.begin() + offset + fingerprints_size), fingerprint_size, fingerprints);
        offset += fingerprints_size;
    }
    
    convert_pairs(std::vector<uint8_t>(input.begin() + offset, input.end()), deviation_size, pairs);
}

inline void convert_index(std::map<std::vector<uint8_t>, uint64_t>& input, std::vector<uint8_t>& output)
{
    if (input.size() == 0)
    {
        return; 
    }
    size_t size_of_fingerprint = input.begin()->first.size();


    output.reserve(sizeof(size_t) + input.size() * (size_of_fingerprint + sizeof(uint64_t)));
    
    std::vector<uint8_t> fingerprint_size;
    convert_natural_number(size_of_fingerprint, fingerprint_size);

    output.insert(output.begin(), fingerprint_size.begin(), fingerprint_size.end()); 
    size_t offset = sizeof(size_t);

    for (auto it = input.begin(); it != input.end(); ++it)
    {
        output.insert(output.begin() + offset, it->first.begin(), it->first.end());
        offset += it->first.size();

        std::vector<uint8_t> count_as_vector;
        convert_natural_number(it->second, count_as_vector);
        output.insert(output.begin() + offset, count_as_vector.begin(), count_as_vector.end());
        offset += count_as_vector.size(); 
    }

}

inline void convert_index(const std::vector<uint8_t>& input, std::map<std::vector<uint8_t>, uint64_t>& output)
{

    if (input.size() == 0)
    {
        return; 
    }
    size_t fingerprint_size;
    convert_natural_number(std::vector<uint8_t>(input.begin(), input.begin() + sizeof(size_t)), fingerprint_size);

    size_t offset = sizeof(size_t);

    while (offset < input.size())
    {
        std::vector<uint8_t> fingerprint;
        fingerprint.insert(fingerprint.begin(), input.begin() + offset, input.begin() + offset + fingerprint_size);
        offset += fingerprint.size();
        uint64_t count;
        convert_natural_number(std::vector<uint8_t>(input.begin() + offset, input.begin() + offset + sizeof(uint64_t)), count);
        offset += sizeof(uint64_t);
        
        output[fingerprint] = count; 
    }
} 
}
}




#endif /*MINERVA_SERIALIZER_HPP*/
