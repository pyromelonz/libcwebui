#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "webserver.h"
}

// getInt tests - reads 4 bytes in little-endian order
class GetIntTest : public ::testing::Test {};

TEST_F(GetIntTest, Zero) {
    unsigned char buffer[] = {0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(getInt(buffer), 0u);
}

TEST_F(GetIntTest, One) {
    unsigned char buffer[] = {0x01, 0x00, 0x00, 0x00};
    EXPECT_EQ(getInt(buffer), 1u);
}

TEST_F(GetIntTest, MaxValue) {
    unsigned char buffer[] = {0xFF, 0xFF, 0xFF, 0xFF};
    EXPECT_EQ(getInt(buffer), 0xFFFFFFFFu);
}

TEST_F(GetIntTest, HighByteWithValue127) {
    unsigned char buffer[] = {0x00, 0x00, 0x00, 0x7F};
    EXPECT_EQ(getInt(buffer), 0x7F000000u);
}

TEST_F(GetIntTest, LittleEndian256) {
    unsigned char buffer[] = {0x00, 0x01, 0x00, 0x00};
    EXPECT_EQ(getInt(buffer), 256u);
}

TEST_F(GetIntTest, LittleEndian65536) {
    unsigned char buffer[] = {0x00, 0x00, 0x01, 0x00};
    EXPECT_EQ(getInt(buffer), 65536u);
}

TEST_F(GetIntTest, LittleEndian16777216) {
    unsigned char buffer[] = {0x00, 0x00, 0x00, 0x01};
    EXPECT_EQ(getInt(buffer), 16777216u);
}

TEST_F(GetIntTest, MixedBytes) {
    // 0x12345678 in little-endian: 0x78, 0x56, 0x34, 0x12
    unsigned char buffer[] = {0x78, 0x56, 0x34, 0x12};
    EXPECT_EQ(getInt(buffer), 0x12345678u);
}

TEST_F(GetIntTest, AnotherMixedValue) {
    // 0xDEADBEEF in little-endian: 0xEF, 0xBE, 0xAD, 0xDE
    unsigned char buffer[] = {0xEF, 0xBE, 0xAD, 0xDE};
    EXPECT_EQ(getInt(buffer), 0xDEADBEEFu);
}
