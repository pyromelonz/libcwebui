#include <gtest/gtest.h>
#include <cstring>
#include <openssl/sha.h>

extern "C" {
#include "webserver.h"
}

class Sha1Test : public ::testing::Test {
protected:
    void compareSha1(const unsigned char* data, int len) {
        unsigned char hash_my[SSL_SHA_DIG_LEN];
        unsigned char hash_openssl[SSL_SHA_DIG_LEN];

        WebserverSHA1((unsigned char*)data, len, hash_my);
        SHA1(data, len, hash_openssl);

        EXPECT_EQ(memcmp(hash_my, hash_openssl, SSL_SHA_DIG_LEN), 0)
            << "SHA1 mismatch for input of length " << len;
    }
};

TEST_F(Sha1Test, ShortString) {
    unsigned char data[] = "abc";
    compareSha1(data, sizeof(data) - 1);
}

TEST_F(Sha1Test, LongString) {
    unsigned char data[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    compareSha1(data, sizeof(data) - 1);
}

TEST_F(Sha1Test, SingleChar) {
    unsigned char data[] = "a";
    compareSha1(data, sizeof(data) - 1);
}

TEST_F(Sha1Test, EmptyString) {
    unsigned char data[] = "";
    compareSha1(data, 0);
}

TEST_F(Sha1Test, BinaryData) {
    unsigned char data[] = {0x00, 0x01, 0x02, 0xff, 0xfe, 0x80};
    compareSha1(data, sizeof(data));
}
