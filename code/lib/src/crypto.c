#include "crypto.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>

#include "utils.h"

/* ── OpenSSL 1.0.x / 1.1.x 兼容层 ─────────────────────────────── */

#if OPENSSL_VERSION_NUMBER < 0x10100000L

#define crypto_evp_md_ctx_new()       EVP_MD_CTX_create()
#define crypto_evp_md_ctx_free(ctx)   EVP_MD_CTX_destroy(ctx)

static EVP_CIPHER_CTX *crypto_evp_cipher_ctx_new(void)
{
    EVP_CIPHER_CTX *ctx = NULL;

    ctx = (EVP_CIPHER_CTX *)OPENSSL_malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    EVP_CIPHER_CTX_init(ctx);
    return ctx;
}

static void crypto_evp_cipher_ctx_free(EVP_CIPHER_CTX *ctx)
{
    if (ctx == NULL) {
        return;
    }

    EVP_CIPHER_CTX_cleanup(ctx);
    OPENSSL_free(ctx);
}

#else

#define crypto_evp_md_ctx_new()       EVP_MD_CTX_new()
#define crypto_evp_md_ctx_free(ctx)   EVP_MD_CTX_free(ctx)

static EVP_CIPHER_CTX *crypto_evp_cipher_ctx_new(void)
{
    return EVP_CIPHER_CTX_new();
}

static void crypto_evp_cipher_ctx_free(EVP_CIPHER_CTX *ctx)
{
    EVP_CIPHER_CTX_free(ctx);
}

#endif

/* ── 内部类型 ─────────────────────────────────────────────────── */

struct crypto_cipher_ctx {
    EVP_CIPHER_CTX *ossl;
    int             encrypt;
};

struct crypto_hash_ctx {
    EVP_MD_CTX *ossl;
    const EVP_MD *md;
};

static int crypto_openssl_initialized = 0;

static const EVP_CIPHER *crypto_cipher_to_evp(crypto_cipher_algo_t algo)
{
    switch (algo) {
    case CRYPTO_CIPHER_AES_128_CBC:
        return EVP_aes_128_cbc();
    case CRYPTO_CIPHER_AES_256_CBC:
        return EVP_aes_256_cbc();
    default:
        return NULL;
    }
}

static const EVP_MD *crypto_hash_to_evp(crypto_hash_algo_t algo)
{
    switch (algo) {
    case CRYPTO_HASH_MD5:
        return EVP_md5();
    case CRYPTO_HASH_SHA1:
        return EVP_sha1();
    case CRYPTO_HASH_SHA256:
        return EVP_sha256();
    case CRYPTO_HASH_SHA512:
        return EVP_sha512();
    default:
        return NULL;
    }
}

static int crypto_validate_cipher_key_iv(crypto_cipher_algo_t algo,
                                         size_t key_len,
                                         size_t iv_len)
{
    const EVP_CIPHER *cipher = crypto_cipher_to_evp(algo);

    if (cipher == NULL) {
        LogError("unsupported cipher algorithm: %d", (int)algo);
        return -EINVAL;
    }

    if (key_len != (size_t)EVP_CIPHER_key_length(cipher)) {
        LogError("invalid key length: got %zu expected %d",
                 key_len, EVP_CIPHER_key_length(cipher));
        return -EINVAL;
    }

    if (iv_len != (size_t)EVP_CIPHER_iv_length(cipher)) {
        LogError("invalid iv length: got %zu expected %d",
                 iv_len, EVP_CIPHER_iv_length(cipher));
        return -EINVAL;
    }

    return 0;
}

static int crypto_cipher_init(crypto_cipher_ctx_t *ctx,
                              crypto_cipher_algo_t algo,
                              const unsigned char *key,
                              size_t key_len,
                              const unsigned char *iv,
                              size_t iv_len,
                              int encrypt)
{
    const EVP_CIPHER *cipher = NULL;
    int rc = 0;

    if (ctx == NULL) {
        LogError("cipher context is NULL");
        return -EINVAL;
    }
    if (key == NULL) {
        LogError("key is NULL");
        return -EINVAL;
    }
    if (iv == NULL) {
        LogError("iv is NULL");
        return -EINVAL;
    }

    rc = crypto_validate_cipher_key_iv(algo, key_len, iv_len);
    if (rc != 0) {
        return rc;
    }

    cipher = crypto_cipher_to_evp(algo);
    if (cipher == NULL) {
        LogError("unsupported cipher algorithm: %d", (int)algo);
        return -EINVAL;
    }

    if (ctx->ossl == NULL) {
        ctx->ossl = crypto_evp_cipher_ctx_new();
        if (ctx->ossl == NULL) {
            LogError("failed to allocate EVP_CIPHER_CTX");
            return -ENOMEM;
        }
    }

    if (EVP_CipherInit_ex(ctx->ossl, cipher, NULL, key, iv, encrypt) != 1) {
        LogError("EVP_CipherInit_ex failed");
        return -EIO;
    }

    ctx->encrypt = encrypt;
    return 0;
}

static int crypto_cipher_do_update(crypto_cipher_ctx_t *ctx,
                                   const unsigned char *in,
                                   size_t in_len,
                                   unsigned char *out,
                                   size_t out_cap,
                                   size_t *out_len)
{
    int written = 0;

    if (ctx == NULL || ctx->ossl == NULL) {
        LogError("cipher context is not initialized");
        return -EINVAL;
    }
    if (out == NULL || out_len == NULL) {
        LogError("output buffer or out_len is NULL");
        return -EINVAL;
    }
    if (in_len > 0 && in == NULL) {
        LogError("input buffer is NULL");
        return -EINVAL;
    }

    *out_len = 0;

    if (EVP_CipherUpdate(ctx->ossl, out, &written, in, (int)in_len) != 1) {
        LogError("EVP_CipherUpdate failed");
        return -EIO;
    }

    if ((size_t)written > out_cap) {
        LogError("cipher update overflow: written=%d cap=%zu", written, out_cap);
        return -EOVERFLOW;
    }

    *out_len = (size_t)written;
    return 0;
}

static int crypto_cipher_do_final(crypto_cipher_ctx_t *ctx,
                                  unsigned char *out,
                                  size_t out_cap,
                                  size_t *out_len)
{
    int written = 0;

    if (ctx == NULL || ctx->ossl == NULL) {
        LogError("cipher context is not initialized");
        return -EINVAL;
    }
    if (out == NULL || out_len == NULL) {
        LogError("output buffer or out_len is NULL");
        return -EINVAL;
    }

    *out_len = 0;

    if (EVP_CipherFinal_ex(ctx->ossl, out, &written) != 1) {
        LogError("EVP_CipherFinal_ex failed");
        return -EIO;
    }

    if ((size_t)written > out_cap) {
        LogError("cipher final overflow: written=%d cap=%zu", written, out_cap);
        return -EOVERFLOW;
    }

    *out_len = (size_t)written;
    return 0;
}

int crypto_init(void)
{
    if (crypto_openssl_initialized) {
        return 0;
    }

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    OpenSSL_add_all_algorithms();
#endif

    crypto_openssl_initialized = 1;
    return 0;
}

void crypto_cleanup(void)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    if (crypto_openssl_initialized) {
        EVP_cleanup();
        crypto_openssl_initialized = 0;
    }
#else
    crypto_openssl_initialized = 0;
#endif
}

size_t crypto_base64_encode_size(size_t src_len)
{
    return src_len / 3 * 4 + (src_len % 3 != 0 ? 4 : 0) + 1;
}

size_t crypto_base64_decode_size(size_t src_len)
{
    return src_len / 4 * 3 + 3;
}

int crypto_base64_encode(const unsigned char *src,
                         size_t src_len,
                         char *dst,
                         size_t dst_cap)
{
    BIO *b64 = NULL;
    BIO *mem = NULL;
    BUF_MEM *buf_mem = NULL;
    long encoded_len = 0;
    int rc = 0;

    if (src == NULL) {
        LogError("source buffer is NULL");
        return -EINVAL;
    }
    if (dst == NULL) {
        LogError("destination buffer is NULL");
        return -EINVAL;
    }
    if (dst_cap < crypto_base64_encode_size(src_len)) {
        LogError("destination buffer too small: cap=%zu need=%zu",
                 dst_cap, crypto_base64_encode_size(src_len));
        return -EINVAL;
    }

    rc = crypto_init();
    if (rc != 0) {
        return rc;
    }

    b64 = BIO_new(BIO_f_base64());
    if (b64 == NULL) {
        LogError("BIO_new(BIO_f_base64) failed");
        return -ENOMEM;
    }

    mem = BIO_new(BIO_s_mem());
    if (mem == NULL) {
        LogError("BIO_new(BIO_s_mem) failed");
        BIO_free_all(b64);
        return -ENOMEM;
    }

    b64 = BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    if (BIO_write(b64, src, (int)src_len) <= 0) {
        LogError("BIO_write failed during base64 encode");
        BIO_free_all(b64);
        return -EIO;
    }

    if (BIO_flush(b64) != 1) {
        LogError("BIO_flush failed during base64 encode");
        BIO_free_all(b64);
        return -EIO;
    }

    BIO_get_mem_ptr(b64, &buf_mem);
    if (buf_mem == NULL || buf_mem->length == 0) {
        LogError("base64 encode produced empty output");
        BIO_free_all(b64);
        return -EIO;
    }

    encoded_len = (long)buf_mem->length;
    if ((size_t)encoded_len + 1 > dst_cap) {
        LogError("encoded output exceeds destination capacity");
        BIO_free_all(b64);
        return -EOVERFLOW;
    }

    memcpy(dst, buf_mem->data, (size_t)encoded_len);
    dst[encoded_len] = '\0';
    BIO_free_all(b64);

    return (int)encoded_len;
}

static size_t crypto_base64_strip_whitespace(const char *src,
                                             size_t src_len,
                                             char *tmp,
                                             size_t tmp_cap)
{
    size_t i = 0;
    size_t j = 0;

    for (i = 0; i < src_len; ++i) {
        unsigned char ch = (unsigned char)src[i];

        if (isspace(ch)) {
            continue;
        }
        if (j >= tmp_cap) {
            return (size_t)-1;
        }
        tmp[j++] = (char)ch;
    }

    return j;
}

int crypto_base64_decode(const char *src,
                         size_t src_len,
                         unsigned char *dst,
                         size_t dst_cap)
{
    char *clean = NULL;
    size_t clean_len = 0;
    BIO *b64 = NULL;
    BIO *mem = NULL;
    int decoded_len = 0;
    int rc = 0;

    if (src == NULL) {
        LogError("source string is NULL");
        return -EINVAL;
    }
    if (dst == NULL) {
        LogError("destination buffer is NULL");
        return -EINVAL;
    }

    if (src_len == 0) {
        src_len = strlen(src);
    }

    if (src_len == 0) {
        LogError("empty base64 input");
        return -EINVAL;
    }

    if (dst_cap < crypto_base64_decode_size(src_len)) {
        LogError("destination buffer too small: cap=%zu need=%zu",
                 dst_cap, crypto_base64_decode_size(src_len));
        return -EINVAL;
    }

    rc = crypto_init();
    if (rc != 0) {
        return rc;
    }

    clean = (char *)malloc(src_len + 1);
    if (clean == NULL) {
        LogError("failed to allocate whitespace strip buffer");
        return -ENOMEM;
    }

    clean_len = crypto_base64_strip_whitespace(src, src_len, clean, src_len);
    if (clean_len == (size_t)-1) {
        LogError("base64 input too long after whitespace strip");
        free(clean);
        return -EINVAL;
    }
    clean[clean_len] = '\0';

    b64 = BIO_new(BIO_f_base64());
    if (b64 == NULL) {
        LogError("BIO_new(BIO_f_base64) failed");
        free(clean);
        return -ENOMEM;
    }

    mem = BIO_new_mem_buf(clean, (int)clean_len);
    if (mem == NULL) {
        LogError("BIO_new_mem_buf failed");
        BIO_free_all(b64);
        free(clean);
        return -ENOMEM;
    }

    b64 = BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    decoded_len = BIO_read(b64, dst, (int)dst_cap);
    BIO_free_all(b64);
    free(clean);

    if (decoded_len < 0) {
        LogError("BIO_read failed during base64 decode");
        return -EIO;
    }

    return decoded_len;
}

crypto_cipher_ctx_t *crypto_cipher_ctx_create(void)
{
    crypto_cipher_ctx_t *ctx = NULL;

    ctx = (crypto_cipher_ctx_t *)calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        LogError("failed to allocate cipher context");
        return NULL;
    }

    return ctx;
}

void crypto_cipher_ctx_destroy(crypto_cipher_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    crypto_evp_cipher_ctx_free(ctx->ossl);
    ctx->ossl = NULL;
    free(ctx);
}

int crypto_cipher_encrypt_init(crypto_cipher_ctx_t *ctx,
                               crypto_cipher_algo_t algo,
                               const unsigned char *key,
                               size_t key_len,
                               const unsigned char *iv,
                               size_t iv_len)
{
    int rc = 0;

    rc = crypto_validate_cipher_key_iv(algo, key_len, iv_len);
    if (rc != 0) {
        return rc;
    }

    rc = crypto_init();
    if (rc != 0) {
        return rc;
    }

    return crypto_cipher_init(ctx, algo, key, key_len, iv, iv_len, 1);
}

int crypto_cipher_encrypt_update(crypto_cipher_ctx_t *ctx,
                                 const unsigned char *in,
                                 size_t in_len,
                                 unsigned char *out,
                                 size_t out_cap,
                                 size_t *out_len)
{
    return crypto_cipher_do_update(ctx, in, in_len, out, out_cap, out_len);
}

int crypto_cipher_encrypt_final(crypto_cipher_ctx_t *ctx,
                                unsigned char *out,
                                size_t out_cap,
                                size_t *out_len)
{
    return crypto_cipher_do_final(ctx, out, out_cap, out_len);
}

int crypto_cipher_decrypt_init(crypto_cipher_ctx_t *ctx,
                               crypto_cipher_algo_t algo,
                               const unsigned char *key,
                               size_t key_len,
                               const unsigned char *iv,
                               size_t iv_len)
{
    int rc = 0;

    rc = crypto_validate_cipher_key_iv(algo, key_len, iv_len);
    if (rc != 0) {
        return rc;
    }

    rc = crypto_init();
    if (rc != 0) {
        return rc;
    }

    return crypto_cipher_init(ctx, algo, key, key_len, iv, iv_len, 0);
}

int crypto_cipher_decrypt_update(crypto_cipher_ctx_t *ctx,
                                 const unsigned char *in,
                                 size_t in_len,
                                 unsigned char *out,
                                 size_t out_cap,
                                 size_t *out_len)
{
    return crypto_cipher_do_update(ctx, in, in_len, out, out_cap, out_len);
}

int crypto_cipher_decrypt_final(crypto_cipher_ctx_t *ctx,
                                unsigned char *out,
                                size_t out_cap,
                                size_t *out_len)
{
    return crypto_cipher_do_final(ctx, out, out_cap, out_len);
}

static int crypto_cipher_once(int encrypt,
                              crypto_cipher_algo_t algo,
                              const unsigned char *key,
                              size_t key_len,
                              const unsigned char *iv,
                              size_t iv_len,
                              const unsigned char *in,
                              size_t in_len,
                              unsigned char *out,
                              size_t out_cap,
                              size_t *out_len)
{
    crypto_cipher_ctx_t *ctx = NULL;
    size_t chunk_len = 0;
    size_t final_len = 0;
    size_t total = 0;
    int rc = 0;

    if (in_len > 0 && in == NULL) {
        LogError("input buffer is NULL");
        return -EINVAL;
    }
    if (out == NULL || out_len == NULL) {
        LogError("output buffer or out_len is NULL");
        return -EINVAL;
    }
    if (out_cap < crypto_cipher_output_size(in_len)) {
        LogError("output buffer too small: cap=%zu need=%zu",
                 out_cap, crypto_cipher_output_size(in_len));
        return -EINVAL;
    }

    ctx = crypto_cipher_ctx_create();
    if (ctx == NULL) {
        return -ENOMEM;
    }

    if (encrypt) {
        rc = crypto_cipher_encrypt_init(ctx, algo, key, key_len, iv, iv_len);
    } else {
        rc = crypto_cipher_decrypt_init(ctx, algo, key, key_len, iv, iv_len);
    }
    if (rc != 0) {
        goto out;
    }

    rc = crypto_cipher_do_update(ctx, in, in_len, out, out_cap, &chunk_len);
    if (rc != 0) {
        goto out;
    }

    total = chunk_len;
    rc = crypto_cipher_do_final(ctx,
                                out + total,
                                out_cap - total,
                                &final_len);
    if (rc != 0) {
        goto out;
    }

    total += final_len;
    *out_len = total;
    rc = 0;

out:
    crypto_cipher_ctx_destroy(ctx);
    return rc;
}

int crypto_cipher_encrypt(crypto_cipher_algo_t algo,
                          const unsigned char *key,
                          size_t key_len,
                          const unsigned char *iv,
                          size_t iv_len,
                          const unsigned char *in,
                          size_t in_len,
                          unsigned char *out,
                          size_t out_cap,
                          size_t *out_len)
{
    return crypto_cipher_once(1, algo, key, key_len, iv, iv_len,
                              in, in_len, out, out_cap, out_len);
}

int crypto_cipher_decrypt(crypto_cipher_algo_t algo,
                          const unsigned char *key,
                          size_t key_len,
                          const unsigned char *iv,
                          size_t iv_len,
                          const unsigned char *in,
                          size_t in_len,
                          unsigned char *out,
                          size_t out_cap,
                          size_t *out_len)
{
    return crypto_cipher_once(0, algo, key, key_len, iv, iv_len,
                              in, in_len, out, out_cap, out_len);
}

size_t crypto_cipher_output_size(size_t in_len)
{
    return in_len + (size_t)EVP_MAX_BLOCK_LENGTH;
}

size_t crypto_hash_digest_size(crypto_hash_algo_t algo)
{
    const EVP_MD *md = crypto_hash_to_evp(algo);

    if (md == NULL) {
        return 0;
    }

    return (size_t)EVP_MD_size(md);
}

crypto_hash_ctx_t *crypto_hash_ctx_create(void)
{
    crypto_hash_ctx_t *ctx = NULL;

    ctx = (crypto_hash_ctx_t *)calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        LogError("failed to allocate hash context");
        return NULL;
    }

    ctx->ossl = crypto_evp_md_ctx_new();
    if (ctx->ossl == NULL) {
        LogError("failed to allocate EVP_MD_CTX");
        free(ctx);
        return NULL;
    }

    return ctx;
}

void crypto_hash_ctx_destroy(crypto_hash_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    crypto_evp_md_ctx_free(ctx->ossl);
    ctx->ossl = NULL;
    free(ctx);
}

int crypto_hash_init(crypto_hash_ctx_t *ctx, crypto_hash_algo_t algo)
{
    const EVP_MD *md = NULL;
    int rc = 0;

    if (ctx == NULL || ctx->ossl == NULL) {
        LogError("hash context is NULL");
        return -EINVAL;
    }

    md = crypto_hash_to_evp(algo);
    if (md == NULL) {
        LogError("unsupported hash algorithm: %d", (int)algo);
        return -EINVAL;
    }

    rc = crypto_init();
    if (rc != 0) {
        return rc;
    }

    if (EVP_DigestInit_ex(ctx->ossl, md, NULL) != 1) {
        LogError("EVP_DigestInit_ex failed");
        return -EIO;
    }

    ctx->md = md;
    return 0;
}

int crypto_hash_update(crypto_hash_ctx_t *ctx,
                       const void *data,
                       size_t len)
{
    if (ctx == NULL || ctx->ossl == NULL || ctx->md == NULL) {
        LogError("hash context is not initialized");
        return -EINVAL;
    }
    if (len > 0 && data == NULL) {
        LogError("hash input data is NULL");
        return -EINVAL;
    }

    if (EVP_DigestUpdate(ctx->ossl, data, len) != 1) {
        LogError("EVP_DigestUpdate failed");
        return -EIO;
    }

    return 0;
}

int crypto_hash_final(crypto_hash_ctx_t *ctx,
                      unsigned char *out,
                      size_t out_cap,
                      size_t *out_len)
{
    unsigned int digest_len = 0;

    if (ctx == NULL || ctx->ossl == NULL || ctx->md == NULL) {
        LogError("hash context is not initialized");
        return -EINVAL;
    }
    if (out == NULL || out_len == NULL) {
        LogError("output buffer or out_len is NULL");
        return -EINVAL;
    }

    if (out_cap < (size_t)EVP_MD_size(ctx->md)) {
        LogError("hash output buffer too small: cap=%zu need=%d",
                 out_cap, EVP_MD_size(ctx->md));
        return -EINVAL;
    }

    if (EVP_DigestFinal_ex(ctx->ossl, out, &digest_len) != 1) {
        LogError("EVP_DigestFinal_ex failed");
        return -EIO;
    }

    *out_len = (size_t)digest_len;
    ctx->md = NULL;
    return 0;
}

int crypto_hash_digest(crypto_hash_algo_t algo,
                       const void *data,
                       size_t len,
                       unsigned char *out,
                       size_t out_cap,
                       size_t *out_len)
{
    const EVP_MD *md = NULL;
    unsigned int digest_len = 0;
    int rc = 0;

    if (len > 0 && data == NULL) {
        LogError("hash input data is NULL");
        return -EINVAL;
    }
    if (out == NULL || out_len == NULL) {
        LogError("output buffer or out_len is NULL");
        return -EINVAL;
    }

    md = crypto_hash_to_evp(algo);
    if (md == NULL) {
        LogError("unsupported hash algorithm: %d", (int)algo);
        return -EINVAL;
    }

    if (out_cap < (size_t)EVP_MD_size(md)) {
        LogError("hash output buffer too small: cap=%zu need=%d",
                 out_cap, EVP_MD_size(md));
        return -EINVAL;
    }

    rc = crypto_init();
    if (rc != 0) {
        return rc;
    }

    if (EVP_Digest(data, len, out, &digest_len, md, NULL) != 1) {
        LogError("EVP_Digest failed");
        return -EIO;
    }

    *out_len = (size_t)digest_len;
    return 0;
}
