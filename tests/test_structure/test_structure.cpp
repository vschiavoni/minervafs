#include <gtest/gtest.h>

#include <minerva-safefs-layer/serialization/serializer.hpp>

#include <nlohmann/json.hpp>

#include <cstdint>


TEST(test_serialization, convert_natural_number)
{

    size_t input = 1024;
    
    std::vector<uint8_t> data;

    minerva::serializer::convert_natural_number(input, data);
    
    EXPECT_EQ(sizeof(input), data.size());

    size_t output;
    
    minerva::serializer::convert_natural_number(data, output);

    EXPECT_EQ(input, output);
}

TEST(test_serialization, convert_json)
{
    nlohmann::json input;
    input["test"] = 1;

    std::vector<uint8_t> data;
    minerva::serializer::convert_json(input, data);

    nlohmann::json output;

    minerva::serializer::convert_json(data, output);

    EXPECT_EQ(input, output);
}

int main(int argc, char **argv) {
    srand(static_cast<uint32_t>(time(0)));
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


