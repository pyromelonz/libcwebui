#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "webserver.h"
}

// Tests for builtinConditions function
// These tests call actual libcwebui functions
class ConditionsTest : public ::testing::Test {
protected:
    FUNCTION_PARAS func;
    http_request request;
    socket_info sock;

    void SetUp() override {
        memset(&func, 0, sizeof(FUNCTION_PARAS));
        memset(&request, 0, sizeof(http_request));
        memset(&sock, 0, sizeof(socket_info));

        // Link request to socket (needed for LOG macro)
        request.socket = &sock;
        sock.socket = 0;  // fd 0 for logging
    }

    void setConditionName(const char* name) {
        func.parameter[0].text = const_cast<char*>(name);
        func.parameter[0].length = strlen(name);
        func.parameter_count = 1;
    }
};

// is_true should always return CONDITION_TRUE
TEST_F(ConditionsTest, IsTrue) {
    setConditionName("is_true");

    CONDITION_RETURN result = builtinConditions(&request, &func);
    EXPECT_EQ(result, CONDITION_TRUE);
}

// is_false should always return CONDITION_FALSE
TEST_F(ConditionsTest, IsFalse) {
    setConditionName("is_false");

    CONDITION_RETURN result = builtinConditions(&request, &func);
    EXPECT_EQ(result, CONDITION_FALSE);
}

// is_64bit depends on architecture
TEST_F(ConditionsTest, Is64Bit) {
    setConditionName("is_64bit");

    CONDITION_RETURN result = builtinConditions(&request, &func);

#ifdef __LP64__
    EXPECT_EQ(result, CONDITION_TRUE);
#else
    EXPECT_EQ(result, CONDITION_FALSE);
#endif
}

// is_websockets_available depends on compile flag
TEST_F(ConditionsTest, IsWebsocketsAvailable) {
    setConditionName("is_websockets_available");

    CONDITION_RETURN result = builtinConditions(&request, &func);

#ifdef WEBSERVER_USE_WEBSOCKETS
    EXPECT_EQ(result, CONDITION_TRUE);
#else
    EXPECT_EQ(result, CONDITION_FALSE);
#endif
}

// is_ssl_available depends on compile flag
TEST_F(ConditionsTest, IsSslAvailable) {
    setConditionName("is_ssl_available");

    CONDITION_RETURN result = builtinConditions(&request, &func);

#ifdef WEBSERVER_USE_SSL
    EXPECT_EQ(result, CONDITION_TRUE);
#else
    EXPECT_EQ(result, CONDITION_FALSE);
#endif
}

// Unknown condition should return CONDITION_ERROR
TEST_F(ConditionsTest, UnknownCondition) {
    setConditionName("nonexistent_condition_xyz");

    CONDITION_RETURN result = builtinConditions(&request, &func);
    EXPECT_EQ(result, CONDITION_ERROR);
}

// Empty condition name
TEST_F(ConditionsTest, EmptyConditionName) {
    setConditionName("");

    CONDITION_RETURN result = builtinConditions(&request, &func);
    EXPECT_EQ(result, CONDITION_ERROR);
}

// Condition names are case-sensitive
TEST_F(ConditionsTest, CaseSensitiveCondition) {
    setConditionName("IS_TRUE");  // uppercase

    CONDITION_RETURN result = builtinConditions(&request, &func);
    // Should not match "is_true"
    EXPECT_EQ(result, CONDITION_ERROR);
}

TEST_F(ConditionsTest, CaseSensitiveCondition2) {
    setConditionName("Is_True");  // mixed case

    CONDITION_RETURN result = builtinConditions(&request, &func);
    EXPECT_EQ(result, CONDITION_ERROR);
}

// Partial condition name should not match
TEST_F(ConditionsTest, PartialConditionName) {
    setConditionName("is_tru");  // missing 'e'

    CONDITION_RETURN result = builtinConditions(&request, &func);
    EXPECT_EQ(result, CONDITION_ERROR);
}

// Condition with extra characters should not match
TEST_F(ConditionsTest, ConditionWithSuffix) {
    setConditionName("is_true_extra");

    CONDITION_RETURN result = builtinConditions(&request, &func);
    EXPECT_EQ(result, CONDITION_ERROR);
}
