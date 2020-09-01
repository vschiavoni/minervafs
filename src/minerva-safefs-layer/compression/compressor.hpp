#ifndef MINERVA_COMPRESSOR_HPP
#define MINERVA_COMPRESSOR_HPP

#include <string>
#include <cstdint>
#include <vector>

namespace minerva
{
enum compressor_algorithm
{
    GZIP
};

    
class compressor
{

public:

    compressor(compressor_algorithm algorithm, size_t basis_size);

    void compress(const std::string& filename);
    void compress(std::vector<uint8_t>& data);

    void uncompress(std::vector<uint8_t>& data);

private:

    void compress_gzip(const std::string& filename);
    void compress_gzip(std::vector<uint8_t>& data);

    void uncompress_gzip(std::vector<uint8_t>& data);
    
private:

    compressor_algorithm m_algorithm;
    size_t m_basis_size;
};
}

#endif /* MINERVA_COMPRESSOR_HPP */
