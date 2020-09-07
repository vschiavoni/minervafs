#include "compressor.hpp"

#include <zlib.h>

#include <stdexcept>
#include <iostream>

namespace minerva
{
    compressor::compressor(){}
    
    compressor::compressor(compressor_algorithm algorithm, size_t basis_size) : m_algorithm(algorithm),
                                                                                m_basis_size(basis_size)
    {
        std::cout << "COMPRESSION!" << std::endl; 
    }

    compressor::compressor(compressor_algorithm algorithm) : m_algorithm(algorithm), m_basis_size(0)
    {
        std::cout << "COMPRESSION!" << std::endl; 
    }

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

        if (m_basis_size == 0)
        {
            m_basis_size = data.size(); 
        }
        
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
        unsigned long compressed_size = data.size() + 15;

        std::vector<uint8_t> compressed_data(compressed_size);
        
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

