#include <gtest/gtest.h>
#include <cstring>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

extern "C" {
#include "webserver.h"
}

class Base64Test : public ::testing::Test {
protected:
    void sslBase64Encode(const unsigned char *input, int length, unsigned char *output, int out_length) {
        BIO *bmem, *b64;
        BUF_MEM *bptr;

        memset(output, 0, out_length);

        b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

        bmem = BIO_new(BIO_s_mem());
        BIO_set_flags(bmem, BIO_FLAGS_BASE64_NO_NL);

        b64 = BIO_push(b64, bmem);
        BIO_write(b64, input, length);
        BIO_flush(b64);
        BIO_get_mem_ptr(b64, &bptr);

        if ((int)bptr->length < out_length) {
            memcpy(output, bptr->data, bptr->length);
        }

        BIO_free_all(b64);
    }

    void compareBase64(const unsigned char* data, int len) {
        unsigned char hash_my[200];
        unsigned char hash_openssl[200];

        WebserverBase64Encode((unsigned char*)data, len, hash_my, 200);
        sslBase64Encode(data, len, hash_openssl, 200);

        EXPECT_STREQ((char*)hash_my, (char*)hash_openssl)
            << "Base64 mismatch for input of length " << len;
    }
};

TEST_F(Base64Test, ShortString) {
    unsigned char data[] = "abc";
    compareBase64(data, sizeof(data) - 1);
}

TEST_F(Base64Test, LongString) {
    unsigned char data[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    compareBase64(data, sizeof(data) - 1);
}

TEST_F(Base64Test, SingleChar) {
    unsigned char data[] = "a";
    compareBase64(data, sizeof(data) - 1);
}

TEST_F(Base64Test, TwoChars) {
    unsigned char data[] = "ab";
    compareBase64(data, sizeof(data) - 1);
}

TEST_F(Base64Test, BinaryData) {
    unsigned char data[] = {0x00, 0x01, 0x02, 0xff, 0xfe, 0x80};
    compareBase64(data, sizeof(data));
}

TEST_F(Base64Test, AllBytes) {
    // Test with bytes that might cause padding issues
    unsigned char data1[] = {0x14, 0xfb, 0x9c, 0x03, 0xd9, 0x7e};
    compareBase64(data1, sizeof(data1));

    unsigned char data2[] = {0x14, 0xfb, 0x9c, 0x03, 0xd9};
    compareBase64(data2, sizeof(data2));

    unsigned char data3[] = {0x14, 0xfb, 0x9c, 0x03};
    compareBase64(data3, sizeof(data3));
}
