#include "compressor.hpp"

#include <zlib.h>

#include <stdexcept>

namespace minerva
{
    compressor::compressor(compressor_algorithm algorithm, size_t basis_size) : m_algorithm(algorithm),
                                                                                m_basis_size(basis_size) {}

    void compressor::compress(const std::string& filename)
    {
        switch (m_algorithm)
        {
        case compressor_algorithm::GZIP:
            break;
        default:
            compress_gzip(filename);
        }
    }

    void compressor::compress(std::vector<uint8_t>& data)
    {
        switch (m_algorithm)
        {
        case compressor_algorithm::GZIP:
            compress_gzip(data);            
            break;
        default:
            compress_gzip(data);
        }
    }

    void compressor::uncompress(std::vector<uint8_t>& data)
    {
        switch (m_algorithm)
        {
        case compressor_algorithm::GZIP:
            uncompress_gzip(data);
            break;
        default:
            uncompress_gzip(data);
        }        
    }

    void compressor::compress_gzip(const std::string& filename)
    {
        (void) filename;
    }


    // https://numberduck.com/Blog/?nPostId=3
    void compressor::compress_gzip(std::vector<uint8_t>& data)
    {
        std::vector<uint8_t> compressed_data(data.size());

        unsigned long compressed_size = compressed_data.size(); 
        
        int result = compress2(compressed_data.data(), &compressed_size, data.data(), (unsigned long) data.size(), 9);

        if (result != Z_OK)
        {
            throw std::runtime_error("Failed to compress data");
        }
        data = std::vector<uint8_t>(compressed_data.begin(), compressed_data.begin() + compressed_size);
    }

    void compressor::uncompress_gzip(std::vector<uint8_t>& data)
    {
        std::vector<uint8_t> uncompressed_data(m_basis_size);

        unsigned long uncompressed_size = m_basis_size;
        int result = ::uncompress(uncompressed_data.data(), &uncompressed_size, data.data(), (unsigned long)data.size());

        if (result != Z_OK)
        {
            throw std::runtime_error("Failed to uncompress data");
        }

        data = uncompressed_data;
    }
    
}

