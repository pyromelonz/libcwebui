#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "webserver.h"
}

// Server Config Tests
class ServerConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        initConfig();
    }

    void TearDown() override {
        freeConfig();
    }
};

TEST_F(ServerConfigTest, InitSetsDefaults) {
    // Check default port
    int port = getConfigInt("port");
    EXPECT_EQ(port, 80);
}

TEST_F(ServerConfigTest, InitSetsSSLPort) {
    int sslPort = getConfigInt("ssl_port");
    // Default is -1 (disabled)
    EXPECT_EQ(sslPort, -1);
}

TEST_F(ServerConfigTest, InitSetsMaxPostSize) {
    int maxPost = getConfigInt("max_post_size");
    EXPECT_GT(maxPost, 0);
}

TEST_F(ServerConfigTest, InitSetsSessionTimeout) {
    int timeout = getConfigInt("session_timeout");
    EXPECT_GT(timeout, 0);
}

TEST_F(ServerConfigTest, SetAndGetInt) {
    setConfigInt("test_value", 12345);
    int result = getConfigInt("test_value");
    EXPECT_EQ(result, 12345);
}

TEST_F(ServerConfigTest, SetAndGetText) {
    setConfigText("test_string", "hello world");
    char* result = getConfigText("test_string");
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "hello world");
}

TEST_F(ServerConfigTest, OverwriteIntValue) {
    setConfigInt("overwrite_test", 100);
    EXPECT_EQ(getConfigInt("overwrite_test"), 100);

    setConfigInt("overwrite_test", 200);
    EXPECT_EQ(getConfigInt("overwrite_test"), 200);
}

TEST_F(ServerConfigTest, OverwriteTextValue) {
    setConfigText("text_overwrite", "first");
    EXPECT_STREQ(getConfigText("text_overwrite"), "first");

    setConfigText("text_overwrite", "second");
    EXPECT_STREQ(getConfigText("text_overwrite"), "second");
}

TEST_F(ServerConfigTest, GetNonExistentInt) {
    int result = getConfigInt("nonexistent_key");
    EXPECT_EQ(result, 0);
}

TEST_F(ServerConfigTest, GetNonExistentText) {
    char* result = getConfigText("nonexistent_key");
    EXPECT_EQ(result, nullptr);
}

TEST_F(ServerConfigTest, SetIntZero) {
    setConfigInt("zero_value", 0);
    // Note: getConfigInt returns 0 for not found, so this is ambiguous
    EXPECT_EQ(getConfigInt("zero_value"), 0);
}

TEST_F(ServerConfigTest, SetIntNegative) {
    setConfigInt("negative_value", -42);
    EXPECT_EQ(getConfigInt("negative_value"), -42);
}

TEST_F(ServerConfigTest, SetEmptyText) {
    setConfigText("empty_string", "");
    char* result = getConfigText("empty_string");
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "");
}

TEST_F(ServerConfigTest, SetLongText) {
    // Create a long string
    std::string longText(1000, 'x');
    setConfigText("long_text", longText.c_str());
    char* result = getConfigText("long_text");
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, longText.c_str());
}

TEST_F(ServerConfigTest, MultipleConfigs) {
    setConfigInt("config_a", 1);
    setConfigInt("config_b", 2);
    setConfigInt("config_c", 3);
    setConfigText("config_d", "four");
    setConfigText("config_e", "five");

    EXPECT_EQ(getConfigInt("config_a"), 1);
    EXPECT_EQ(getConfigInt("config_b"), 2);
    EXPECT_EQ(getConfigInt("config_c"), 3);
    EXPECT_STREQ(getConfigText("config_d"), "four");
    EXPECT_STREQ(getConfigText("config_e"), "five");
}

// Test that changing port works
TEST_F(ServerConfigTest, ChangePort) {
    setConfigInt("port", 8080);
    EXPECT_EQ(getConfigInt("port"), 8080);
}

TEST_F(ServerConfigTest, ChangeSSLPort) {
    setConfigInt("ssl_port", 8443);
    EXPECT_EQ(getConfigInt("ssl_port"), 8443);
}
