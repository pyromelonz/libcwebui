#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "webserver.h"
}

class CookieParserTest : public ::testing::Test {
protected:
    HttpRequestHeader *header;
    char value[WEBSERVER_GUID_LENGTH];

    void SetUp() override {
        header = WebserverMallocHttpRequestHeader();
        memset(value, 0, sizeof(value));
    }

    void TearDown() override {
        WebserverFreeHttpRequestHeader(header);
    }

    void parseCookieString(const char* cookie_line) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "Cookie: %s", cookie_line);
        parseCookies(buf, strlen(buf), header);
    }

    int getCookieCount() {
        return ws_list_size(&header->cookie_list);
    }
};

// Simple cookie tests
TEST_F(CookieParserTest, SimpleCookie) {
    parseCookieString("session=abc123");

    EXPECT_EQ(checkCookie((char*)"session", value, header), 1);
    EXPECT_STREQ(value, "abc123");
}

TEST_F(CookieParserTest, CookieWithEqualsInValue) {
    parseCookieString("data=a=b=c");

    EXPECT_EQ(checkCookie((char*)"data", value, header), 1);
    EXPECT_STREQ(value, "a=b=c");
}

// Multiple cookies tests
TEST_F(CookieParserTest, MultipleCookiesFirst) {
    parseCookieString("a=1; b=2; c=3");

    EXPECT_EQ(checkCookie((char*)"a", value, header), 1);
    EXPECT_STREQ(value, "1");
}

TEST_F(CookieParserTest, MultipleCookiesMiddle) {
    parseCookieString("a=1; b=2; c=3");

    EXPECT_EQ(checkCookie((char*)"b", value, header), 1);
    EXPECT_STREQ(value, "2");
}

TEST_F(CookieParserTest, MultipleCookiesLast) {
    parseCookieString("a=1; b=2; c=3");

    EXPECT_EQ(checkCookie((char*)"c", value, header), 1);
    EXPECT_STREQ(value, "3");
}

TEST_F(CookieParserTest, MultipleCookiesCount) {
    parseCookieString("a=1; b=2; c=3");

    EXPECT_EQ(getCookieCount(), 3);
}

// Cookie without value
TEST_F(CookieParserTest, CookieWithoutValue) {
    parseCookieString("flag");

    EXPECT_EQ(checkCookie((char*)"flag", value, header), 1);
    EXPECT_STREQ(value, "");
}

TEST_F(CookieParserTest, CookieWithoutValueCount) {
    parseCookieString("flag");

    EXPECT_EQ(getCookieCount(), 1);
}

// Mixed: with and without value
TEST_F(CookieParserTest, MixedCookiesWithValue) {
    parseCookieString("a=1; flag; b=2");

    EXPECT_EQ(checkCookie((char*)"a", value, header), 1);
    EXPECT_STREQ(value, "1");
}

TEST_F(CookieParserTest, MixedCookiesFlag) {
    parseCookieString("a=1; flag; b=2");

    EXPECT_EQ(checkCookie((char*)"flag", value, header), 1);
    EXPECT_STREQ(value, "");
}

TEST_F(CookieParserTest, MixedCookiesAfterFlag) {
    parseCookieString("a=1; flag; b=2");

    EXPECT_EQ(checkCookie((char*)"b", value, header), 1);
    EXPECT_STREQ(value, "2");
}

TEST_F(CookieParserTest, MixedCookiesCount) {
    parseCookieString("a=1; flag; b=2");

    EXPECT_EQ(getCookieCount(), 3);
}

// URL-encoded values
TEST_F(CookieParserTest, UrlEncodedSpace) {
    parseCookieString("name=hello%20world");

    EXPECT_EQ(checkCookie((char*)"name", value, header), 1);
    EXPECT_STREQ(value, "hello world");
}

TEST_F(CookieParserTest, UrlEncodedSpecialChars) {
    parseCookieString("data=%3D%26%3B");

    EXPECT_EQ(checkCookie((char*)"data", value, header), 1);
    EXPECT_STREQ(value, "=&;");
}

// Whitespace handling
TEST_F(CookieParserTest, MultipleSpacesAfterSemicolon) {
    parseCookieString("a=1;  b=2");

    EXPECT_EQ(checkCookie((char*)"b", value, header), 1);
    EXPECT_STREQ(value, "2");
}

// Empty string
TEST_F(CookieParserTest, EmptyString) {
    parseCookieString("");

    EXPECT_EQ(getCookieCount(), 0);
}

// Cookie not found
TEST_F(CookieParserTest, CookieNotFound) {
    parseCookieString("a=1; b=2");

    EXPECT_EQ(checkCookie((char*)"nonexistent", value, header), 0);
}

// Long values
TEST_F(CookieParserTest, LongValue) {
    parseCookieString("token=abcdefghijklmnopqrstuvwxyz0123456789");

    EXPECT_EQ(checkCookie((char*)"token", value, header), 1);
    EXPECT_STREQ(value, "abcdefghijklmnopqrstuvwxyz0123456789");
}
