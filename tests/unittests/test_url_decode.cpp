#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "webserver.h"
}

class UrlDecodeTest : public ::testing::Test {
protected:
    char buffer[1024];

    void decodeAndExpect(const char* input, const char* expected) {
        strncpy(buffer, input, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        url_decode(buffer);
        EXPECT_STREQ(buffer, expected);
    }
};

// Basic decoding tests
TEST_F(UrlDecodeTest, NoEncoding) {
    decodeAndExpect("hello", "hello");
}

TEST_F(UrlDecodeTest, EmptyString) {
    decodeAndExpect("", "");
}

TEST_F(UrlDecodeTest, SpaceAsPlus) {
    decodeAndExpect("hello+world", "hello world");
}

TEST_F(UrlDecodeTest, SpaceAsPercent20) {
    decodeAndExpect("hello%20world", "hello world");
}

// Hex decoding tests
TEST_F(UrlDecodeTest, LowercaseHex) {
    decodeAndExpect("%3d", "=");
}

TEST_F(UrlDecodeTest, UppercaseHex) {
    decodeAndExpect("%3D", "=");
}

TEST_F(UrlDecodeTest, MultipleEncodings) {
    decodeAndExpect("%3D%26%3B", "=&;");
}

TEST_F(UrlDecodeTest, MixedEncodings) {
    decodeAndExpect("a%3Db%26c", "a=b&c");
}

// Special characters
TEST_F(UrlDecodeTest, Ampersand) {
    decodeAndExpect("%26", "&");
}

TEST_F(UrlDecodeTest, QuestionMark) {
    decodeAndExpect("%3F", "?");
}

TEST_F(UrlDecodeTest, Slash) {
    decodeAndExpect("%2F", "/");
}

TEST_F(UrlDecodeTest, Percent) {
    decodeAndExpect("%25", "%");
}

// Edge cases
TEST_F(UrlDecodeTest, IncompleteEncodingAtEnd) {
    // If only 1 character after %, should not decode
    decodeAndExpect("test%2", "test%2");
}

TEST_F(UrlDecodeTest, PercentAtEnd) {
    decodeAndExpect("test%", "test%");
}

TEST_F(UrlDecodeTest, MultiplePlusses) {
    decodeAndExpect("a+b+c+d", "a b c d");
}

TEST_F(UrlDecodeTest, MixedPlusAndPercent) {
    decodeAndExpect("hello+world%21", "hello world!");
}

TEST_F(UrlDecodeTest, NullByte) {
    decodeAndExpect("%00", "\0");
}

// toHex function tests
class ToHexTest : public ::testing::Test {};

TEST_F(ToHexTest, Digits) {
    EXPECT_EQ(toHex('0'), 0);
    EXPECT_EQ(toHex('5'), 5);
    EXPECT_EQ(toHex('9'), 9);
}

TEST_F(ToHexTest, LowercaseLetters) {
    EXPECT_EQ(toHex('a'), 10);
    EXPECT_EQ(toHex('f'), 15);
    EXPECT_EQ(toHex('c'), 12);
}

TEST_F(ToHexTest, UppercaseLetters) {
    EXPECT_EQ(toHex('A'), 10);
    EXPECT_EQ(toHex('F'), 15);
    EXPECT_EQ(toHex('C'), 12);
}

TEST_F(ToHexTest, InvalidChar) {
    EXPECT_EQ(toHex('g'), 0);
    EXPECT_EQ(toHex('z'), 0);
    EXPECT_EQ(toHex(' '), 0);
}
