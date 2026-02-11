#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "webserver.h"
}

// stringfind tests
class StringFindTest : public ::testing::Test {};

TEST_F(StringFindTest, PatternFound) {
    EXPECT_GT(stringfind("hello world", "world"), 0);
}

TEST_F(StringFindTest, PatternAtStart) {
    EXPECT_GT(stringfind("hello world", "hello"), 0);
}

TEST_F(StringFindTest, PatternNotFound) {
    EXPECT_EQ(stringfind("hello world", "xyz"), 0);
}

TEST_F(StringFindTest, NullBuffer) {
    EXPECT_EQ(stringfind(nullptr, "test"), 0);
}

TEST_F(StringFindTest, EmptyBuffer) {
    EXPECT_EQ(stringfind("", "test"), 0);
}

TEST_F(StringFindTest, EmptyPattern) {
    // Empty pattern matches immediately
    EXPECT_EQ(stringfind("hello", ""), 0);
}

TEST_F(StringFindTest, SingleCharPattern) {
    EXPECT_GT(stringfind("hello", "e"), 0);
}

// stringnfind tests
class StringNFindTest : public ::testing::Test {};

TEST_F(StringNFindTest, PatternFound) {
    EXPECT_GT(stringnfind("hello world", "world", 11), 0);
}

TEST_F(StringNFindTest, PatternNotFoundInLimit) {
    // "world" starts at position 6, but we only search up to 5
    EXPECT_EQ(stringnfind("hello world", "world", 5), -1);
}

TEST_F(StringNFindTest, NullBuffer) {
    EXPECT_EQ(stringnfind(nullptr, "test", 10), -1);
}

TEST_F(StringNFindTest, NullPattern) {
    EXPECT_EQ(stringnfind("hello", nullptr, 5), -1);
}

TEST_F(StringNFindTest, ZeroLength) {
    EXPECT_EQ(stringnfind("hello", "h", 0), -1);
}

TEST_F(StringNFindTest, PatternLongerThanLimit) {
    EXPECT_EQ(stringnfind("hello", "hello world", 5), -1);
}

// Webserver_strncpy tests
// Note: This function null-terminates at dest_size-1, not after the copied content
// The count parameter specifies how many chars to copy (excluding null terminator)
class WebserverStrncpyTest : public ::testing::Test {
protected:
    char dest[100];

    void SetUp() override {
        memset(dest, 0, sizeof(dest));
    }
};

TEST_F(WebserverStrncpyTest, BasicCopy) {
    // Copy 5 chars + null terminator (count=6 includes null)
    Webserver_strncpy(dest, sizeof(dest), "hello", 6);
    EXPECT_STREQ(dest, "hello");
}

TEST_F(WebserverStrncpyTest, CopyWithNullTermination) {
    // Copy up to 5 chars, function puts \0 at dest[9]
    Webserver_strncpy(dest, 10, "hello world", 5);
    // The function copies 5 chars but null terminates at position 9
    EXPECT_EQ(dest[0], 'h');
    EXPECT_EQ(dest[4], 'o');
    EXPECT_EQ(dest[9], '\0');
}

TEST_F(WebserverStrncpyTest, DestSmallerThanCount) {
    Webserver_strncpy(dest, 3, "hello", 5);
    // Should copy only 3 chars and null terminate
    EXPECT_EQ(dest[2], '\0');
}

TEST_F(WebserverStrncpyTest, NullDest) {
    // Should not crash
    Webserver_strncpy(nullptr, 10, "hello", 5);
}

TEST_F(WebserverStrncpyTest, NullSrc) {
    // Should not crash
    Webserver_strncpy(dest, sizeof(dest), nullptr, 5);
}

// Webserver_strlen tests
class WebserverStrlenTest : public ::testing::Test {};

TEST_F(WebserverStrlenTest, EmptyString) {
    char str[] = "";
    EXPECT_EQ(Webserver_strlen(str), 0u);
}

TEST_F(WebserverStrlenTest, ShortString) {
    char str[] = "hello";
    EXPECT_EQ(Webserver_strlen(str), 5u);
}

TEST_F(WebserverStrlenTest, LongString) {
    char str[] = "this is a longer test string";
    EXPECT_EQ(Webserver_strlen(str), strlen(str));
}

// getHTMLDateFormat tests
class HTMLDateFormatTest : public ::testing::Test {
protected:
    char buffer[1000];
};

TEST_F(HTMLDateFormatTest, BasicDate) {
    unsigned int len = getHTMLDateFormat(buffer, 15, 6, 2024, 14, 30);
    EXPECT_GT(len, 0u);
    EXPECT_STREQ(buffer, "15 Jun 2024 14:30 GMT");
}

TEST_F(HTMLDateFormatTest, January) {
    getHTMLDateFormat(buffer, 1, 1, 2024, 0, 0);
    EXPECT_TRUE(strstr(buffer, "Jan") != nullptr);
}

TEST_F(HTMLDateFormatTest, December) {
    getHTMLDateFormat(buffer, 25, 12, 2024, 23, 59);
    EXPECT_TRUE(strstr(buffer, "Dec") != nullptr);
}

TEST_F(HTMLDateFormatTest, LeadingZeros) {
    getHTMLDateFormat(buffer, 5, 3, 2024, 9, 5);
    EXPECT_STREQ(buffer, "05 Mar 2024 09:05 GMT");
}

// convertBinToHexString tests
class ConvertBinToHexTest : public ::testing::Test {
protected:
    char hexOutput[100];
};

TEST_F(ConvertBinToHexTest, SingleByte) {
    unsigned char bin[] = {0xAB};
    convertBinToHexString(bin, 1, hexOutput, 2);
    EXPECT_STREQ(hexOutput, "AB");
}

TEST_F(ConvertBinToHexTest, MultipleBytesLowerValues) {
    unsigned char bin[] = {0x01, 0x02, 0x03};
    convertBinToHexString(bin, 3, hexOutput, 6);
    EXPECT_STREQ(hexOutput, "010203");
}

TEST_F(ConvertBinToHexTest, AllZeros) {
    unsigned char bin[] = {0x00, 0x00};
    convertBinToHexString(bin, 2, hexOutput, 4);
    EXPECT_STREQ(hexOutput, "0000");
}

TEST_F(ConvertBinToHexTest, AllFs) {
    unsigned char bin[] = {0xFF, 0xFF};
    convertBinToHexString(bin, 2, hexOutput, 4);
    EXPECT_STREQ(hexOutput, "FFFF");
}

TEST_F(ConvertBinToHexTest, MixedValues) {
    unsigned char bin[] = {0xDE, 0xAD, 0xBE, 0xEF};
    convertBinToHexString(bin, 4, hexOutput, 8);
    EXPECT_STREQ(hexOutput, "DEADBEEF");
}
