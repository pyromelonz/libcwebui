#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "webserver.h"
}

class HeaderParserTest : public ::testing::Test {
protected:
    HttpRequestHeader *header;
    socket_info *sock;
    unsigned int bytes_parsed;

    void SetUp() override {
        header = WebserverMallocHttpRequestHeader();
        sock = (socket_info*)calloc(1, sizeof(socket_info));
        sock->header = header;
        bytes_parsed = 0;
    }

    void TearDown() override {
        WebserverFreeHttpRequestHeader(header);
        free(sock);
    }

    int parseHeaderLine(const char* line) {
        char buf[2048];
        snprintf(buf, sizeof(buf), "%s", line);
        return analyseHeaderLine(sock, buf, strlen(line), header);
    }

    int parseFullHeader(const char* rawHeader) {
        char buf[4096];
        strncpy(buf, rawHeader, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        return ParseHeader(sock, header, buf, strlen(rawHeader), &bytes_parsed);
    }
};

// GET request tests
// Note: The parser starts reading after "GET /" so the leading / is stripped
TEST_F(HeaderParserTest, SimpleGet) {
    parseHeaderLine("GET /index.html HTTP/1.1");
    EXPECT_EQ(header->method, HTTP_GET);
    EXPECT_STREQ(header->url, "index.html");
}

TEST_F(HeaderParserTest, GetWithQueryString) {
    parseHeaderLine("GET /search?q=test HTTP/1.1");
    EXPECT_EQ(header->method, HTTP_GET);
    EXPECT_STREQ(header->url, "search");

    ws_variable* param = getVariable(header->parameter_store, "q");
    ASSERT_NE(param, nullptr);

    char buf[100];
    getWSVariableString(param, buf, sizeof(buf));
    EXPECT_STREQ(buf, "test");
}

TEST_F(HeaderParserTest, GetWithMultipleParams) {
    parseHeaderLine("GET /api?a=1&b=2&c=3 HTTP/1.1");

    ws_variable* param_a = getVariable(header->parameter_store, "a");
    ws_variable* param_b = getVariable(header->parameter_store, "b");
    ws_variable* param_c = getVariable(header->parameter_store, "c");

    ASSERT_NE(param_a, nullptr);
    ASSERT_NE(param_b, nullptr);
    ASSERT_NE(param_c, nullptr);

    char buf[100];
    getWSVariableString(param_a, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1");
    getWSVariableString(param_b, buf, sizeof(buf));
    EXPECT_STREQ(buf, "2");
    getWSVariableString(param_c, buf, sizeof(buf));
    EXPECT_STREQ(buf, "3");
}

TEST_F(HeaderParserTest, GetWithUrlEncodedParam) {
    parseHeaderLine("GET /search?q=hello%20world HTTP/1.1");

    ws_variable* param = getVariable(header->parameter_store, "q");
    ASSERT_NE(param, nullptr);

    char buf[100];
    getWSVariableString(param, buf, sizeof(buf));
    EXPECT_STREQ(buf, "hello world");
}

// POST request tests
// Note: The parser starts reading after "POST /" so the leading / is stripped
TEST_F(HeaderParserTest, SimplePost) {
    parseHeaderLine("POST /submit HTTP/1.1");
    EXPECT_EQ(header->method, HTTP_POST);
    EXPECT_STREQ(header->url, "submit");
}

TEST_F(HeaderParserTest, PostWithQueryString) {
    parseHeaderLine("POST /api?action=create HTTP/1.1");
    EXPECT_EQ(header->method, HTTP_POST);
    EXPECT_STREQ(header->url, "api");

    ws_variable* param = getVariable(header->parameter_store, "action");
    ASSERT_NE(param, nullptr);

    char buf[100];
    getWSVariableString(param, buf, sizeof(buf));
    EXPECT_STREQ(buf, "create");
}

// OPTIONS request tests
// Note: The parser starts reading after "OPTIONS /" so the leading / is stripped
TEST_F(HeaderParserTest, OptionsRequest) {
    parseHeaderLine("OPTIONS /resource HTTP/1.1");
    EXPECT_EQ(header->method, HTTP_OPTIONS);
    EXPECT_STREQ(header->url, "resource");
}

// HTTP version tests
TEST_F(HeaderParserTest, Http11) {
    parseHeaderLine("GET /test HTTP/1.1");
    EXPECT_EQ(sock->header->isHttp1_1, 1);
}

TEST_F(HeaderParserTest, Http10) {
    parseHeaderLine("GET /test HTTP/1.0");
    EXPECT_EQ(sock->header->isHttp1_1, 0);
}

// Header field tests
TEST_F(HeaderParserTest, HostHeader) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Host: example.com:8080");

    EXPECT_STREQ(header->Host, "example.com:8080");
    EXPECT_STREQ(header->HostName, "example.com");
}

TEST_F(HeaderParserTest, HostHeaderWithoutPort) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Host: example.com");

    EXPECT_STREQ(header->Host, "example.com");
    EXPECT_STREQ(header->HostName, "example.com");
}

TEST_F(HeaderParserTest, ContentLength) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Content-Length: 12345");

    EXPECT_EQ(header->contentlenght, 12345u);
}

TEST_F(HeaderParserTest, ContentLengthNegative) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Content-Length: -1");

    EXPECT_EQ(header->contentlenght, 0u);
}

// NOTE: This test exposes a memory leak bug - UserAgent is not freed in WebserverResetHttpRequestHeader
TEST_F(HeaderParserTest, UserAgentHeader) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("User-Agent: Mozilla/5.0");

    EXPECT_STREQ(header->UserAgent, "Mozilla/5.0");
}

TEST_F(HeaderParserTest, AcceptEncodingHeader) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Accept-Encoding: gzip, deflate");

    EXPECT_STREQ(header->Accept_Encoding, "gzip, deflate");
}

TEST_F(HeaderParserTest, OriginHeader) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Origin: https://example.com");

    EXPECT_STREQ(header->Origin, "https://example.com");
}

TEST_F(HeaderParserTest, RefererHeader) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Referer: https://previous.com/page");

    EXPECT_STREQ(header->Referer, "https://previous.com/page");
}

// Case insensitivity tests
TEST_F(HeaderParserTest, HeaderCaseInsensitive) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("HOST: example.com");

    EXPECT_STREQ(header->Host, "example.com");
}

TEST_F(HeaderParserTest, ContentTypeCaseInsensitive) {
    parseHeaderLine("POST / HTTP/1.1");
    parseHeaderLine("content-length: 100");

    EXPECT_EQ(header->contentlenght, 100u);
}

// Security tests
TEST_F(HeaderParserTest, ScriptInjectionInUrl) {
    parseHeaderLine("GET /<script>alert(1)</script> HTTP/1.1");

    EXPECT_EQ(header->error, 1);
}

TEST_F(HeaderParserTest, DirectoryTraversal) {
    parseHeaderLine("GET /../../etc/passwd HTTP/1.1");

    EXPECT_EQ(header->error, 1);
}

// Content-Type header tests
TEST_F(HeaderParserTest, ContentTypeFormUrlEncoded) {
    parseHeaderLine("POST / HTTP/1.1");
    parseHeaderLine("Content-Type: application/x-www-form-urlencoded");

    EXPECT_EQ(header->contenttype, APPLICATION_X_WWW_FORM_URLENCODED);
}

TEST_F(HeaderParserTest, ContentTypeMultipart) {
    parseHeaderLine("POST / HTTP/1.1");
    parseHeaderLine("Content-Type: multipart/form-data; boundary=----WebKitFormBoundary");

    EXPECT_EQ(header->contenttype, MULTIPART_FORM_DATA);
    EXPECT_NE(header->boundary, nullptr);
    EXPECT_TRUE(strstr(header->boundary, "WebKitFormBoundary") != nullptr);
}

// ParseHeader full tests
// The parser returns the number of bytes parsed per line, needs to be called repeatedly
TEST_F(HeaderParserTest, ParseCompleteHeader) {
    char rawHeader[] =
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";

    char* buf = rawHeader;
    unsigned int len = strlen(rawHeader);
    int result;

    // Parse first line (GET request)
    result = ParseHeader(sock, header, buf, len, &bytes_parsed);
    EXPECT_GT(result, 0);  // First line parsed, more data
    buf += bytes_parsed;
    len -= bytes_parsed;

    // Parse second line (Host header)
    result = ParseHeader(sock, header, buf, len, &bytes_parsed);
    buf += bytes_parsed;
    len -= bytes_parsed;

    // Parse empty line (end of header)
    result = ParseHeader(sock, header, buf, len, &bytes_parsed);
    EXPECT_EQ(result, -2);  // Header complete, no more data

    EXPECT_EQ(header->method, HTTP_GET);
    EXPECT_STREQ(header->url, "index.html");  // Leading / is stripped
    EXPECT_STREQ(header->Host, "example.com");
}

TEST_F(HeaderParserTest, ParseHeaderIncomplete) {
    const char* rawHeader = "GET /test HTTP/1.1\r\n";

    int result = parseFullHeader(rawHeader);

    // Should indicate more data needed
    EXPECT_GT(result, 0);
}

TEST_F(HeaderParserTest, ParseHeaderTooShort) {
    const char* rawHeader = "G";

    int result = parseFullHeader(rawHeader);

    EXPECT_EQ(result, -6);  // Not enough data
}

// Parameter edge cases
TEST_F(HeaderParserTest, ParamWithoutValue) {
    parseHeaderLine("GET /test?flag HTTP/1.1");

    ws_variable* param = getVariable(header->parameter_store, "flag");
    ASSERT_NE(param, nullptr);
}

TEST_F(HeaderParserTest, ParamWithEmptyValue) {
    parseHeaderLine("GET /test?key= HTTP/1.1");

    ws_variable* param = getVariable(header->parameter_store, "key");
    ASSERT_NE(param, nullptr);

    char buf[100];
    getWSVariableString(param, buf, sizeof(buf));
    EXPECT_STREQ(buf, "");
}

TEST_F(HeaderParserTest, ParamWithSpecialChars) {
    parseHeaderLine("GET /test?data=%3D%26 HTTP/1.1");

    ws_variable* param = getVariable(header->parameter_store, "data");
    ASSERT_NE(param, nullptr);

    char buf[100];
    getWSVariableString(param, buf, sizeof(buf));
    EXPECT_STREQ(buf, "=&");
}

// Invalid method test
TEST_F(HeaderParserTest, InvalidMethod) {
    parseHeaderLine("INVALID /test HTTP/1.1");

    EXPECT_EQ(header->error, 1);
    EXPECT_STREQ(header->error_method, "INVALID");
}

// If-Modified-Since header tests
TEST_F(HeaderParserTest, IfModifiedSinceHeader) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("If-Modified-Since: Wed, 21 Oct 2015 07:28:00 GMT");

    EXPECT_NE(header->If_Modified_Since, nullptr);
    EXPECT_STREQ(header->If_Modified_Since, "Wed, 21 Oct 2015 07:28:00 GMT");
}

TEST_F(HeaderParserTest, IfModifiedSinceEmpty) {
    parseHeaderLine("GET / HTTP/1.1");
    // No If-Modified-Since header
    EXPECT_EQ(header->If_Modified_Since, nullptr);
}

// ETag header tests
TEST_F(HeaderParserTest, ETagHeader) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("ETag: \"abc123\"");

    // Note: etag field stores the ETag value if parsed
    // Check if parsing doesn't crash
    EXPECT_EQ(header->error, 0);
}

// Authorization header tests
TEST_F(HeaderParserTest, AuthorizationBasic) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Authorization: Basic dXNlcjpwYXNz");

    EXPECT_NE(header->Authorization, nullptr);
    EXPECT_STREQ(header->Authorization, "Basic dXNlcjpwYXNz");
}

TEST_F(HeaderParserTest, AuthorizationBearer) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Authorization: Bearer eyJhbGciOiJIUzI1NiJ9");

    EXPECT_NE(header->Authorization, nullptr);
    EXPECT_TRUE(strstr(header->Authorization, "Bearer") != nullptr);
}

// Accept header tests
TEST_F(HeaderParserTest, AcceptHeader) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Accept: text/html, application/json");

    EXPECT_NE(header->Accept, nullptr);
    EXPECT_STREQ(header->Accept, "text/html, application/json");
}

// Parameter parsing edge cases
TEST_F(HeaderParserTest, DuplicateParamKeys) {
    parseHeaderLine("GET /test?key=first&key=second HTTP/1.1");

    ws_variable* param = getVariable(header->parameter_store, "key");
    ASSERT_NE(param, nullptr);
    // Second value should overwrite first
    char buf[100];
    getWSVariableString(param, buf, sizeof(buf));
    EXPECT_STREQ(buf, "second");
}

TEST_F(HeaderParserTest, ManyParameters) {
    parseHeaderLine("GET /test?a=1&b=2&c=3&d=4&e=5 HTTP/1.1");

    char buf[100];
    ws_variable* param;

    param = getVariable(header->parameter_store, "a");
    ASSERT_NE(param, nullptr);
    getWSVariableString(param, buf, sizeof(buf));
    EXPECT_STREQ(buf, "1");

    param = getVariable(header->parameter_store, "e");
    ASSERT_NE(param, nullptr);
    getWSVariableString(param, buf, sizeof(buf));
    EXPECT_STREQ(buf, "5");
}

TEST_F(HeaderParserTest, ParamIncompletePercent) {
    // Incomplete percent encoding at end
    parseHeaderLine("GET /test?data=test%2 HTTP/1.1");

    ws_variable* param = getVariable(header->parameter_store, "data");
    ASSERT_NE(param, nullptr);
    // Should handle gracefully
}

TEST_F(HeaderParserTest, ParamInvalidPercent) {
    // Invalid hex chars in percent encoding
    parseHeaderLine("GET /test?data=%GG HTTP/1.1");

    ws_variable* param = getVariable(header->parameter_store, "data");
    ASSERT_NE(param, nullptr);
    // Should handle gracefully without crash
}

TEST_F(HeaderParserTest, ParamOnlyAmpersands) {
    parseHeaderLine("GET /test?&&& HTTP/1.1");
    // Should handle gracefully without crash
    EXPECT_EQ(header->error, 0);
}

TEST_F(HeaderParserTest, ParamEmptyName) {
    parseHeaderLine("GET /test?=value HTTP/1.1");
    // Empty parameter name - should handle gracefully
    EXPECT_EQ(header->error, 0);
}

// CORS-related headers
TEST_F(HeaderParserTest, AccessControlRequestMethod) {
    parseHeaderLine("OPTIONS / HTTP/1.1");
    parseHeaderLine("Access-Control-Request-Method: POST");

    // Check if header is parsed (depends on implementation)
    EXPECT_EQ(header->method, HTTP_OPTIONS);
}

TEST_F(HeaderParserTest, AccessControlRequestHeaders) {
    parseHeaderLine("OPTIONS / HTTP/1.1");
    parseHeaderLine("Access-Control-Request-Headers: X-Custom-Header");

    EXPECT_EQ(header->method, HTTP_OPTIONS);
}

// WebSocket headers
TEST_F(HeaderParserTest, SecWebSocketKey) {
    parseHeaderLine("GET /ws HTTP/1.1");
    parseHeaderLine("Upgrade: websocket");
    parseHeaderLine("Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==");

    EXPECT_NE(header->SecWebSocketKey, nullptr);
    EXPECT_STREQ(header->SecWebSocketKey, "dGhlIHNhbXBsZSBub25jZQ==");
}

TEST_F(HeaderParserTest, SecWebSocketVersion) {
    parseHeaderLine("GET /ws HTTP/1.1");
    parseHeaderLine("Sec-WebSocket-Version: 13");

    EXPECT_EQ(header->SecWebSocketVersion, 13);
}

TEST_F(HeaderParserTest, SecWebSocketProtocol) {
    parseHeaderLine("GET /ws HTTP/1.1");
    parseHeaderLine("Sec-WebSocket-Protocol: chat, superchat");

    EXPECT_NE(header->SecWebSocketProtocol, nullptr);
}

// Range header tests
TEST_F(HeaderParserTest, RangeHeader) {
    parseHeaderLine("GET /file.bin HTTP/1.1");
    parseHeaderLine("Range: bytes=0-1023");

    // Check if range is parsed (if supported)
    EXPECT_EQ(header->error, 0);
}

// Connection header tests
TEST_F(HeaderParserTest, ConnectionKeepAlive) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Connection: keep-alive");

    EXPECT_NE(header->Connection, nullptr);
    EXPECT_STREQ(header->Connection, "keep-alive");
}

TEST_F(HeaderParserTest, ConnectionClose) {
    parseHeaderLine("GET / HTTP/1.1");
    parseHeaderLine("Connection: close");

    EXPECT_NE(header->Connection, nullptr);
    EXPECT_STREQ(header->Connection, "close");
}

TEST_F(HeaderParserTest, ConnectionUpgrade) {
    parseHeaderLine("GET /ws HTTP/1.1");
    parseHeaderLine("Connection: Upgrade");
    parseHeaderLine("Upgrade: websocket");

    EXPECT_NE(header->Connection, nullptr);
    EXPECT_STREQ(header->Connection, "Upgrade");
}
