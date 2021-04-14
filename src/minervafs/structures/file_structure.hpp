#ifndef MINERVA_STRUCTURES_FILE_STRUCTURE
#define MINERVA_STRUCTURES_FILE_STRUCTURE

#include <cstdint>
#include <vector>
#include <map>
#include <utility>

namespace minerva
{
namespace structures
{
struct file_structure
{
    size_t file_size;
    std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> bd_pairs;
    std::map<std::vector<uint8_t>, uint8_t> fingerprints;
};
}
}

#endif /* MINERVA_STRUCTURES_FILE_STRUCTURE */
