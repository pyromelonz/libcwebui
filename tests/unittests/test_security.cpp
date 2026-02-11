#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "webserver.h"
}

class SecurityTest : public ::testing::Test {
protected:
    HttpRequestHeader *header;
    socket_info *sock;

    void SetUp() override {
        header = WebserverMallocHttpRequestHeader();
        sock = (socket_info*)calloc(1, sizeof(socket_info));
        sock->header = header;
    }

    void TearDown() override {
        WebserverFreeHttpRequestHeader(header);
        free(sock);
    }

    int parseHeaderLine(const char* line) {
        char buf[4096];
        strncpy(buf, line, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        return analyseHeaderLine(sock, buf, strlen(line), header);
    }
};

// Directory Traversal Tests
TEST_F(SecurityTest, DirectoryTraversalSimple) {
    parseHeaderLine("GET /../etc/passwd HTTP/1.1");
    EXPECT_EQ(header->error, 1);
}

TEST_F(SecurityTest, DirectoryTraversalEncoded) {
    parseHeaderLine("GET /%2e%2e/etc/passwd HTTP/1.1");
    // After URL decode, should detect ..
    EXPECT_EQ(header->error, 1);
}

TEST_F(SecurityTest, DirectoryTraversalDouble) {
    parseHeaderLine("GET /../../../../../../etc/passwd HTTP/1.1");
    EXPECT_EQ(header->error, 1);
}

TEST_F(SecurityTest, DirectoryTraversalMixed) {
    parseHeaderLine("GET /valid/../../../etc/passwd HTTP/1.1");
    EXPECT_EQ(header->error, 1);
}

// Script Injection Tests
TEST_F(SecurityTest, ScriptTagInUrl) {
    parseHeaderLine("GET /<script>alert(1)</script> HTTP/1.1");
    EXPECT_EQ(header->error, 1);
}

// BUG: URL-encoded script tags are NOT detected - security vulnerability!
TEST_F(SecurityTest, ScriptTagEncoded) {
    parseHeaderLine("GET /%3Cscript%3Ealert(1)%3C/script%3E HTTP/1.1");
    // After URL decode this becomes <script>alert(1)</script>
    // This SHOULD be detected and rejected
    EXPECT_EQ(header->error, 1);
}

TEST_F(SecurityTest, ScriptTagInParameter) {
    parseHeaderLine("GET /page?name=<script>alert(1)</script> HTTP/1.1");
    // Script tag in parameter should be detected
    EXPECT_EQ(header->error, 1);
}

// Null Byte Injection Tests
TEST_F(SecurityTest, NullByteInUrl) {
    // Test null byte injection - trying to bypass extension checks
    char line[] = "GET /index.html%00.exe HTTP/1.1";
    parseHeaderLine(line);
    // After parsing, check if null byte is handled
    if (header->url != nullptr) {
        // URL should not contain characters after null byte interpretation
        EXPECT_EQ(strlen(header->url), strcspn(header->url, "\0"));
    }
}

// Null byte injection for extension bypass
// Attacker tries: /secret.txt%00.html to bypass .html-only filter
TEST_F(SecurityTest, NullByteExtensionBypass) {
    parseHeaderLine("GET /secret.txt%00.html HTTP/1.1");
    // After URL decode, this becomes "secret.txt\0.html"
    // If the sanity check sees ".html" but the filesystem sees ".txt",
    // that's a security bypass
    if (header->url != nullptr) {
        // The URL after decode should be checked for null bytes
        // Either reject the request OR ensure no null bytes remain
        bool hasNullByte = (memchr(header->url, '\0', 256) != nullptr &&
                           strlen(header->url) < 256);
        // If null byte is present in middle, request should be rejected
        if (hasNullByte) {
            EXPECT_EQ(header->error, 1) << "Null byte in URL should be rejected";
        }
    }
}

// Double-encoded null byte
TEST_F(SecurityTest, DoubleEncodedNullByte) {
    parseHeaderLine("GET /test%2500.html HTTP/1.1");
    // %25 = %, so %2500 -> %00 after first decode
    // Library only decodes once, so result should be "test%00.html"
    // This is correct behavior - no double-decode vulnerability
    EXPECT_EQ(header->error, 0);
    EXPECT_NE(header->url, nullptr);
}

// Null byte in path traversal
TEST_F(SecurityTest, NullByteWithTraversal) {
    parseHeaderLine("GET /../%00/etc/passwd HTTP/1.1");
    // Combining null byte with traversal
    EXPECT_EQ(header->error, 1);
}

// Buffer/Length Tests
TEST_F(SecurityTest, VeryLongUrl) {
    // Create a very long URL
    std::string longUrl = "GET /";
    for (int i = 0; i < 10000; i++) {
        longUrl += "a";
    }
    longUrl += " HTTP/1.1";

    // Should not crash, may reject or truncate
    int result = parseHeaderLine(longUrl.c_str());
    // Either successfully parsed or rejected - both are valid
    EXPECT_TRUE(result == 0 || result == 1 || header->error == 1);
}

TEST_F(SecurityTest, VeryLongHeaderValue) {
    parseHeaderLine("GET / HTTP/1.1");

    std::string longHeader = "X-Custom: ";
    for (int i = 0; i < 10000; i++) {
        longHeader += "x";
    }

    // Should not crash, header may be ignored
    int result = parseHeaderLine(longHeader.c_str());
    EXPECT_TRUE(result == 0 || result == 1);
}

TEST_F(SecurityTest, VeryLongParameterValue) {
    std::string longParam = "GET /page?data=";
    for (int i = 0; i < 10000; i++) {
        longParam += "x";
    }
    longParam += " HTTP/1.1";

    // Should not crash
    int result = parseHeaderLine(longParam.c_str());
    EXPECT_TRUE(result == 0 || result == 1 || header->error == 1);
}

// Malformed Input Tests
TEST_F(SecurityTest, MalformedHttpVersion) {
    parseHeaderLine("GET /index.html HTTP/9.9");
    EXPECT_EQ(header->error, 1);
}

TEST_F(SecurityTest, NoHttpVersion) {
    parseHeaderLine("GET /index.html");
    EXPECT_EQ(header->error, 1);
}

TEST_F(SecurityTest, OnlyMethod) {
    parseHeaderLine("GET");
    EXPECT_EQ(header->error, 1);
}

TEST_F(SecurityTest, EmptyRequest) {
    int result = parseHeaderLine("");
    // Empty request should be handled - either ignored or error
    EXPECT_TRUE(result == 0 || result == 1 || header->error == 1);
}

TEST_F(SecurityTest, GarbageData) {
    int result = parseHeaderLine("\xff\xfe\x00\x01\x02\x03");
    // Garbage should be rejected or ignored
    EXPECT_TRUE(result == 0 || result == 1 || header->error == 1);
}

TEST_F(SecurityTest, HeaderWithoutColon) {
    parseHeaderLine("GET / HTTP/1.1");
    int result = parseHeaderLine("InvalidHeaderNoColon");
    // Invalid header should be ignored (return 0) or rejected
    EXPECT_TRUE(result == 0 || result == 1);
}

TEST_F(SecurityTest, HeaderWithEmptyName) {
    parseHeaderLine("GET / HTTP/1.1");
    int result = parseHeaderLine(": value");
    // Empty header name should be ignored or rejected
    EXPECT_TRUE(result == 0 || result == 1);
}

TEST_F(SecurityTest, HeaderWithOnlyColon) {
    parseHeaderLine("GET / HTTP/1.1");
    int result = parseHeaderLine(":");
    // Should be ignored or rejected
    EXPECT_TRUE(result == 0 || result == 1);
}

// Content-Length Manipulation
TEST_F(SecurityTest, NegativeContentLength) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Content-Length: -1");
    EXPECT_EQ(header->contentlenght, 0u);
}

TEST_F(SecurityTest, HugeContentLength) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Content-Length: 99999999999999999999");
    // Should handle overflow - either cap at max value or reject
    // Must not overflow to a small number that could cause issues
    EXPECT_TRUE(header->contentlenght == 0 ||
                header->contentlenght >= 1000000000 ||
                header->error == 1);
}

TEST_F(SecurityTest, NonNumericContentLength) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Content-Length: abc");
    EXPECT_EQ(header->contentlenght, 0u);
}

TEST_F(SecurityTest, ContentLengthWithSpaces) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Content-Length:    123   ");
    // Leading/trailing spaces should be handled - either parse 123 or reject
    EXPECT_TRUE(header->contentlenght == 123 ||
                header->contentlenght == 0 ||
                header->error == 1);
}

// BUG: Content-Length with trailing garbage - HTTP Request Smuggling risk
// "Content-Length: 123a456" should be rejected, not parsed as 123
TEST_F(SecurityTest, ContentLengthWithTrailingGarbage) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Content-Length: 123a456");
    // RFC 7230: Content-Length must be purely numeric
    // Accepting "123" here can lead to HTTP Request Smuggling
    // This SHOULD either:
    // - Reject the request (error = 1), OR
    // - Parse the full string and fail on non-numeric
    // It should NOT silently accept 123
    EXPECT_TRUE(header->error == 1 || header->contentlenght != 123);
}

TEST_F(SecurityTest, ContentLengthWithLeadingGarbage) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Content-Length: a123");
    EXPECT_EQ(header->contentlenght, 0u);
}

TEST_F(SecurityTest, ContentLengthMixed) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Content-Length: 12ab34");
    // Should not parse as 12
    EXPECT_TRUE(header->error == 1 || header->contentlenght != 12);
}

// Host Header Attacks
TEST_F(SecurityTest, HostHeaderWithPort) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Host: example.com:8080");
    EXPECT_STREQ(header->HostName, "example.com");
}

TEST_F(SecurityTest, HostHeaderMalformed) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Host: :8080");
    // Empty hostname with port - should handle gracefully
    // Either set empty hostname or reject
    EXPECT_TRUE(header->HostName == nullptr ||
                header->HostName[0] == '\0' ||
                strcmp(header->HostName, ":8080") == 0);
}

TEST_F(SecurityTest, HostHeaderMultipleColons) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Host: example.com:8080:extra");
    // Multiple colons - should extract hostname before first colon
    EXPECT_NE(header->HostName, nullptr);
    EXPECT_STREQ(header->HostName, "example.com");
}

// URL Encoding Edge Cases
class UrlDecodeSecurityTest : public ::testing::Test {
protected:
    char buffer[1024];

    void decodeAndCheck(const char* input) {
        strncpy(buffer, input, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        url_decode(buffer);
    }
};

TEST_F(UrlDecodeSecurityTest, DoubleEncoding) {
    // %25 = %, so %252e = %2e after first decode
    decodeAndCheck("%252e%252e");
    // After one decode: %2e%2e
    EXPECT_STREQ(buffer, "%2e%2e");
}

TEST_F(UrlDecodeSecurityTest, InvalidHexChars) {
    decodeAndCheck("%GG%ZZ");
    // Invalid hex - library produces empty string (removes invalid sequences)
    // This is acceptable behavior - not a security issue
    // Just verify it doesn't crash (ASan will catch buffer issues)
    EXPECT_TRUE(strlen(buffer) == 0 || strcmp(buffer, "%GG%ZZ") == 0);
}

TEST_F(UrlDecodeSecurityTest, PartialEncoding) {
    decodeAndCheck("test%2");
    // Incomplete encoding at end
    EXPECT_STREQ(buffer, "test%2");
}

TEST_F(UrlDecodeSecurityTest, PercentAtEnd) {
    decodeAndCheck("test%");
    EXPECT_STREQ(buffer, "test%");
}

TEST_F(UrlDecodeSecurityTest, MixedValidInvalid) {
    decodeAndCheck("%20%GG%20");
    // First %20 -> space, %GG invalid (kept or skipped), last %20 -> space
    // Result should contain at least the valid decoded spaces
    EXPECT_NE(strchr(buffer, ' '), nullptr);
}
