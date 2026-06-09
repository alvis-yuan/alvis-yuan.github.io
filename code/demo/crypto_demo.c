/**
 * @file crypto_demo.c
 * @brief 演示 crypto 模块全部公开 API
 *
 * 覆盖接口：
 *   crypto_init / crypto_cleanup
 *   crypto_base64_encode / crypto_base64_decode
 *   crypto_cipher_encrypt / crypto_cipher_decrypt（一次性）
 *   crypto_cipher_*_init / *_update / *_final（流式加解密）
 *   crypto_hash_digest（一次性哈希）
 *   crypto_hash_init / crypto_hash_update / crypto_hash_final（流式哈希）
 *
 * 编译运行：make crypto && out/bin/crypto
 */

#include "crypto.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"

enum {
    CRYPTO_DEMO_HEX_BUF_SIZE = 256,
};

static int crypto_demo_check(bool cond, const char *msg)
{
    if (!cond) {
        LogError("%s", msg);
        return -1;
    }

    return 0;
}

static void crypto_demo_hex_print(const char *label,
                                  const unsigned char *buf,
                                  size_t len)
{
    size_t i = 0;

    printf("%s (%zu bytes): ", label, len);
    for (i = 0; i < len; ++i) {
        printf("%02x", buf[i]);
    }
    printf("\n");
}

static int crypto_demo_run_base64(void)
{
    const unsigned char plain[] = "Hello, OpenSSL 1.0.1g compatible crypto!";
    char encoded[128] = {0};
    unsigned char decoded[128] = {0};
    int enc_len = 0;
    int dec_len = 0;

    LogInfo("=== [1] Base64 encode / decode ===");

    enc_len = crypto_base64_encode(plain, sizeof(plain) - 1, encoded, sizeof(encoded));
    if (enc_len < 0) {
        LogError("crypto_base64_encode failed: %d", enc_len);
        return -1;
    }

    LogInfo("plain: %s", (const char *)plain);
    LogInfo("encoded: %s", encoded);

    dec_len = crypto_base64_decode(encoded, 0, decoded, sizeof(decoded));
    if (dec_len < 0) {
        LogError("crypto_base64_decode failed: %d", dec_len);
        return -1;
    }

    decoded[dec_len] = '\0';
    if (crypto_demo_check(dec_len == (int)(sizeof(plain) - 1), "decoded length mismatch") != 0) {
        return -1;
    }
    if (crypto_demo_check(memcmp(decoded, plain, sizeof(plain) - 1) == 0,
                          "decoded content mismatch") != 0) {
        return -1;
    }

    LogInfo("decoded: %s", (const char *)decoded);
    return 0;
}

static int crypto_demo_run_cipher_once(void)
{
    const unsigned char key[16] = {
        0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
        0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
    };
    const unsigned char iv[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    };
    const unsigned char plain[] = "AES-128-CBC one-shot encrypt/decrypt demo.";
    unsigned char cipher[128] = {0};
    unsigned char recovered[128] = {0};
    size_t cipher_len = 0;
    size_t plain_len = 0;
    int rc = 0;

    LogInfo("=== [2] AES-128-CBC one-shot cipher ===");

    rc = crypto_cipher_encrypt(CRYPTO_CIPHER_AES_128_CBC,
                               key, sizeof(key),
                               iv, sizeof(iv),
                               plain, sizeof(plain) - 1,
                               cipher, sizeof(cipher),
                               &cipher_len);
    if (rc != 0) {
        LogError("crypto_cipher_encrypt failed: %d", rc);
        return -1;
    }

    crypto_demo_hex_print("ciphertext", cipher, cipher_len);

    rc = crypto_cipher_decrypt(CRYPTO_CIPHER_AES_128_CBC,
                               key, sizeof(key),
                               iv, sizeof(iv),
                               cipher, cipher_len,
                               recovered, sizeof(recovered),
                               &plain_len);
    if (rc != 0) {
        LogError("crypto_cipher_decrypt failed: %d", rc);
        return -1;
    }

    recovered[plain_len] = '\0';
    if (crypto_demo_check(plain_len == sizeof(plain) - 1, "recovered length mismatch") != 0) {
        return -1;
    }
    if (crypto_demo_check(memcmp(recovered, plain, sizeof(plain) - 1) == 0,
                          "recovered content mismatch") != 0) {
        return -1;
    }

    LogInfo("recovered: %s", (const char *)recovered);
    return 0;
}

static int crypto_demo_run_cipher_stream(void)
{
    const unsigned char key[32] = "0123456789abcdef0123456789abcdef";
    const unsigned char iv[16] = "0123456789abcdef";
    const char *part1 = "stream cipher part-1; ";
    const char *part2 = "part-2 finish.";
    crypto_cipher_ctx_t *enc = NULL;
    crypto_cipher_ctx_t *dec = NULL;
    unsigned char cipher[256] = {0};
    unsigned char recovered[256] = {0};
    size_t out_len = 0;
    size_t total_cipher = 0;
    size_t total_plain = 0;
    int rc = 0;

    LogInfo("=== [3] AES-256-CBC streaming cipher ===");

    enc = crypto_cipher_ctx_create();
    dec = crypto_cipher_ctx_create();
    if (enc == NULL || dec == NULL) {
        LogError("failed to create cipher contexts");
        rc = -1;
        goto out;
    }

    rc = crypto_cipher_encrypt_init(enc, CRYPTO_CIPHER_AES_256_CBC,
                                    key, sizeof(key), iv, sizeof(iv));
    if (rc != 0) {
        goto out;
    }

    rc = crypto_cipher_encrypt_update(enc,
                                      (const unsigned char *)part1, strlen(part1),
                                      cipher, sizeof(cipher), &out_len);
    if (rc != 0) {
        goto out;
    }
    total_cipher = out_len;

    rc = crypto_cipher_encrypt_update(enc,
                                      (const unsigned char *)part2, strlen(part2),
                                      cipher + total_cipher,
                                      sizeof(cipher) - total_cipher,
                                      &out_len);
    if (rc != 0) {
        goto out;
    }
    total_cipher += out_len;

    rc = crypto_cipher_encrypt_final(enc,
                                     cipher + total_cipher,
                                     sizeof(cipher) - total_cipher,
                                     &out_len);
    if (rc != 0) {
        goto out;
    }
    total_cipher += out_len;

    crypto_demo_hex_print("stream ciphertext", cipher, total_cipher);

    rc = crypto_cipher_decrypt_init(dec, CRYPTO_CIPHER_AES_256_CBC,
                                    key, sizeof(key), iv, sizeof(iv));
    if (rc != 0) {
        goto out;
    }

    rc = crypto_cipher_decrypt_update(dec, cipher, total_cipher,
                                     recovered, sizeof(recovered), &out_len);
    if (rc != 0) {
        goto out;
    }
    total_plain = out_len;

    rc = crypto_cipher_decrypt_final(dec,
                                     recovered + total_plain,
                                     sizeof(recovered) - total_plain,
                                     &out_len);
    if (rc != 0) {
        goto out;
    }
    total_plain += out_len;

    recovered[total_plain] = '\0';
    LogInfo("stream recovered: %s", (const char *)recovered);

    if (crypto_demo_check(total_plain == strlen(part1) + strlen(part2),
                          "stream recovered length mismatch") != 0) {
        rc = -1;
    }

out:
    crypto_cipher_ctx_destroy(enc);
    crypto_cipher_ctx_destroy(dec);
    return rc;
}

static int crypto_demo_run_hash_once(void)
{
    const char *message = "one-shot SHA-256 digest demo";
    unsigned char digest[64] = {0};
    size_t digest_len = 0;
    int rc = 0;

    LogInfo("=== [4] SHA-256 one-shot hash ===");

    rc = crypto_hash_digest(CRYPTO_HASH_SHA256,
                            message, strlen(message),
                            digest, sizeof(digest),
                            &digest_len);
    if (rc != 0) {
        LogError("crypto_hash_digest failed: %d", rc);
        return -1;
    }

    crypto_demo_hex_print("SHA-256", digest, digest_len);
    return 0;
}

static int crypto_demo_run_hash_stream(void)
{
    const char *chunk1 = "stream hash chunk-1 ";
    const char *chunk2 = "chunk-2 end";
    crypto_hash_ctx_t *ctx = NULL;
    unsigned char digest[64] = {0};
    size_t digest_len = 0;
    int rc = 0;

    LogInfo("=== [5] SHA-512 streaming hash ===");

    ctx = crypto_hash_ctx_create();
    if (ctx == NULL) {
        LogError("crypto_hash_ctx_create failed");
        return -1;
    }

    rc = crypto_hash_init(ctx, CRYPTO_HASH_SHA512);
    if (rc != 0) {
        goto out;
    }

    rc = crypto_hash_update(ctx, chunk1, strlen(chunk1));
    if (rc != 0) {
        goto out;
    }

    rc = crypto_hash_update(ctx, chunk2, strlen(chunk2));
    if (rc != 0) {
        goto out;
    }

    rc = crypto_hash_final(ctx, digest, sizeof(digest), &digest_len);
    if (rc != 0) {
        goto out;
    }

    crypto_demo_hex_print("SHA-512", digest, digest_len);

out:
    crypto_hash_ctx_destroy(ctx);
    return rc;
}

int main(void)
{
    int rc = 0;

    rc = crypto_init();
    if (rc != 0) {
        LogError("crypto_init failed: %d", rc);
        return 1;
    }

    if (crypto_demo_run_base64() != 0) {
        rc = -1;
    }
    if (crypto_demo_run_cipher_once() != 0) {
        rc = -1;
    }
    if (crypto_demo_run_cipher_stream() != 0) {
        rc = -1;
    }
    if (crypto_demo_run_hash_once() != 0) {
        rc = -1;
    }
    if (crypto_demo_run_hash_stream() != 0) {
        rc = -1;
    }

    crypto_cleanup();

    if (rc == 0) {
        LogInfo("=== all crypto demos passed ===");
        return 0;
    }

    LogError("=== crypto demo failed ===");
    return 1;
}
