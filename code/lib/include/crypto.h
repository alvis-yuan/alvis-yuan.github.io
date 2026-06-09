/**
 * @file crypto.h
 * @brief 基于 OpenSSL EVP 的 Base64、对称加密与哈希封装（兼容 1.0.1g+）
 *
 * 算法层统一走 EVP 接口；OpenSSL 1.0.x 与 1.1.x 差异在 crypto.c 内部消化。
 *
 * @par 线程安全
 * - crypto_init() / crypto_cleanup() 非线程安全，应在进程启动/退出时各调用一次。
 * - 其余 API：不同上下文实例可并发使用；同一上下文实例需调用方串行访问。
 *
 * @par 返回值约定
 * - 命令型 API：成功返回 0，失败返回负 errno（如 -EINVAL、-ENOMEM）。
 * - 长度查询 / 数据输出 API：成功返回正字节数，失败返回负 errno。
 */

#ifndef CRYPTO_H
#define CRYPTO_H

#include <stddef.h>

#include "compile_checks.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 对称加密算法枚举。 */
typedef enum crypto_cipher_algo {
    CRYPTO_CIPHER_AES_128_CBC = 0,
    CRYPTO_CIPHER_AES_256_CBC,
} crypto_cipher_algo_t;

/** @brief 哈希算法枚举。 */
typedef enum crypto_hash_algo {
    CRYPTO_HASH_MD5 = 0,
    CRYPTO_HASH_SHA1,
    CRYPTO_HASH_SHA256,
    CRYPTO_HASH_SHA512,
} crypto_hash_algo_t;

/** @brief 不透明对称加密上下文。 */
typedef struct crypto_cipher_ctx crypto_cipher_ctx_t;

/** @brief 不透明流式哈希上下文。 */
typedef struct crypto_hash_ctx crypto_hash_ctx_t;

/**
 * @brief 初始化 OpenSSL 算法表（1.0.x 必须，1.1.x 可安全重复调用）。
 * @return 成功返回 0，失败返回负 errno。
 */
int crypto_init(void) __must_check;

/**
 * @brief 释放 OpenSSL 全局资源（可选，进程退出前调用）。
 */
void crypto_cleanup(void);

/**
 * @brief 计算 Base64 编码后所需缓冲区字节数（含结尾 '\\0'）。
 * @param src_len 原始数据长度。
 * @return 所需缓冲区大小；src_len 非法时返回 0。
 */
size_t crypto_base64_encode_size(size_t src_len);

/**
 * @brief 计算 Base64 解码后所需缓冲区字节数（上界估计）。
 * @param src_len Base64 文本长度（不含 '\\0'）。
 * @return 所需缓冲区大小；src_len 非法时返回 0。
 */
size_t crypto_base64_decode_size(size_t src_len);

/**
 * @brief Base64 编码。
 * @param src 原始数据。
 * @param src_len 原始数据长度。
 * @param dst 输出缓冲区。
 * @param dst_cap dst 容量，应 >= crypto_base64_encode_size(src_len)。
 * @return 成功返回编码后字符数（不含 '\\0'），失败返回负 errno。
 */
int crypto_base64_encode(const unsigned char *src,
                         size_t src_len,
                         char *dst,
                         size_t dst_cap) __must_check;

/**
 * @brief Base64 解码。
 * @param src Base64 文本（可含换行，内部忽略空白）。
 * @param src_len 文本长度；为 0 时按 strlen(src) 处理。
 * @param dst 输出缓冲区。
 * @param dst_cap dst 容量，应 >= crypto_base64_decode_size(src_len)。
 * @return 成功返回解码字节数，失败返回负 errno。
 */
int crypto_base64_decode(const char *src,
                         size_t src_len,
                         unsigned char *dst,
                         size_t dst_cap) __must_check;

/**
 * @brief 创建对称加密上下文。
 * @return 成功返回上下文，失败返回 NULL。
 */
crypto_cipher_ctx_t *crypto_cipher_ctx_create(void) __must_check;

/**
 * @brief 销毁对称加密上下文。
 * @param ctx 上下文；允许 NULL。
 */
void crypto_cipher_ctx_destroy(crypto_cipher_ctx_t *ctx);

/**
 * @brief 初始化加密方向。
 * @param ctx 上下文。
 * @param algo 算法。
 * @param key 密钥。
 * @param key_len 密钥长度（须与算法匹配）。
 * @param iv 初始化向量。
 * @param iv_len IV 长度（CBC 模式须等于块大小）。
 * @return 成功返回 0，失败返回负 errno。
 */
int crypto_cipher_encrypt_init(crypto_cipher_ctx_t *ctx,
                               crypto_cipher_algo_t algo,
                               const unsigned char *key,
                               size_t key_len,
                               const unsigned char *iv,
                               size_t iv_len) __must_check;

/**
 * @brief 加密增量数据。
 * @param out_len 输出实际写入字节数。
 * @return 成功返回 0，失败返回负 errno。
 */
int crypto_cipher_encrypt_update(crypto_cipher_ctx_t *ctx,
                                 const unsigned char *in,
                                 size_t in_len,
                                 unsigned char *out,
                                 size_t out_cap,
                                 size_t *out_len) __must_check;

/**
 * @brief 完成加密并输出剩余填充块。
 * @param out_len 输出实际写入字节数。
 * @return 成功返回 0，失败返回负 errno。
 */
int crypto_cipher_encrypt_final(crypto_cipher_ctx_t *ctx,
                                unsigned char *out,
                                size_t out_cap,
                                size_t *out_len) __must_check;

/**
 * @brief 初始化解密方向。
 * @return 成功返回 0，失败返回负 errno。
 */
int crypto_cipher_decrypt_init(crypto_cipher_ctx_t *ctx,
                               crypto_cipher_algo_t algo,
                               const unsigned char *key,
                               size_t key_len,
                               const unsigned char *iv,
                               size_t iv_len) __must_check;

/**
 * @brief 解密增量数据。
 * @return 成功返回 0，失败返回负 errno。
 */
int crypto_cipher_decrypt_update(crypto_cipher_ctx_t *ctx,
                                 const unsigned char *in,
                                 size_t in_len,
                                 unsigned char *out,
                                 size_t out_cap,
                                 size_t *out_len) __must_check;

/**
 * @brief 完成解密并去除填充。
 * @return 成功返回 0，失败返回负 errno。
 */
int crypto_cipher_decrypt_final(crypto_cipher_ctx_t *ctx,
                                unsigned char *out,
                                size_t out_cap,
                                size_t *out_len) __must_check;

/**
 * @brief 一次性加密（内部 Init → Update → Final）。
 * @param out_len 输出密文长度。
 * @return 成功返回 0，失败返回负 errno。
 */
int crypto_cipher_encrypt(crypto_cipher_algo_t algo,
                          const unsigned char *key,
                          size_t key_len,
                          const unsigned char *iv,
                          size_t iv_len,
                          const unsigned char *in,
                          size_t in_len,
                          unsigned char *out,
                          size_t out_cap,
                          size_t *out_len) __must_check;

/**
 * @brief 一次性解密。
 * @return 成功返回 0，失败返回负 errno。
 */
int crypto_cipher_decrypt(crypto_cipher_algo_t algo,
                          const unsigned char *key,
                          size_t key_len,
                          const unsigned char *iv,
                          size_t iv_len,
                          const unsigned char *in,
                          size_t in_len,
                          unsigned char *out,
                          size_t out_cap,
                          size_t *out_len) __must_check;

/**
 * @brief 估算一次性加/解密输出缓冲区上界（明文长度 + 一个块）。
 * @param in_len 输入长度。
 * @return 建议 out_cap；in_len 非法时返回 0。
 */
size_t crypto_cipher_output_size(size_t in_len);

/**
 * @brief 获取哈希输出长度（字节）。
 * @param algo 哈希算法。
 * @return 摘要长度；未知算法返回 0。
 */
size_t crypto_hash_digest_size(crypto_hash_algo_t algo);

/**
 * @brief 创建流式哈希上下文。
 * @return 成功返回上下文，失败返回 NULL。
 */
crypto_hash_ctx_t *crypto_hash_ctx_create(void) __must_check;

/**
 * @brief 销毁流式哈希上下文。
 * @param ctx 上下文；允许 NULL。
 */
void crypto_hash_ctx_destroy(crypto_hash_ctx_t *ctx);

/**
 * @brief 初始化流式哈希。
 * @return 成功返回 0，失败返回负 errno。
 */
int crypto_hash_init(crypto_hash_ctx_t *ctx,
                     crypto_hash_algo_t algo) __must_check;

/**
 * @brief 向流式哈希追加数据。
 * @return 成功返回 0，失败返回负 errno。
 */
int crypto_hash_update(crypto_hash_ctx_t *ctx,
                       const void *data,
                       size_t len) __must_check;

/**
 * @brief 完成流式哈希并输出摘要。
 * @param out_len 实际摘要长度。
 * @return 成功返回 0，失败返回负 errno。
 */
int crypto_hash_final(crypto_hash_ctx_t *ctx,
                      unsigned char *out,
                      size_t out_cap,
                      size_t *out_len) __must_check;

/**
 * @brief 一次性计算哈希（内部 EVP_Digest）。
 * @param out_len 实际摘要长度。
 * @return 成功返回 0，失败返回负 errno。
 */
int crypto_hash_digest(crypto_hash_algo_t algo,
                       const void *data,
                       size_t len,
                       unsigned char *out,
                       size_t out_cap,
                       size_t *out_len) __must_check;

#ifdef __cplusplus
}
#endif

#endif /* CRYPTO_H */
