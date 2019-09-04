#include "utils.hpp"


std::string stringify_code_params(codes::code_params params)
{
    std::string message = params.code_name +
        ", n = " + std::to_string(params.n) +
        ", k = " + std::to_string(params.k) +
        ", m = " + std::to_string(params.m) +
        ", mgf = " + std::to_string(params.mgf) +
        ", d = " + std::to_string(params.d) +
        ", r = " + std::to_string(params.r);
    return message;
}