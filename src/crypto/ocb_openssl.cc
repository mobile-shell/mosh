#include "src/include/config.h"
#include "src/crypto/ae.h"

#include <cstring>
#include <openssl/crypto.h>
#include <openssl/evp.h>

struct _ae_ctx {
  EVP_CIPHER_CTX *enc_ctx;
  EVP_CIPHER_CTX *dec_ctx;
  int tag_len;
};

int ae_clear(ae_ctx* ctx) {
  EVP_CIPHER_CTX_free(ctx->enc_ctx);
  EVP_CIPHER_CTX_free(ctx->dec_ctx);
  OPENSSL_cleanse(ctx, sizeof(*ctx));
  return AE_SUCCESS;
}

int ae_ctx_sizeof() {
  return sizeof(_ae_ctx);
}

// If direction is 1, initializes encryption. If 0, initializes
// decryption. See the documentation of EVP_CipherInit_ex
static int ae_evp_cipher_init(EVP_CIPHER_CTX **in_ctx, int direction,
			      const unsigned char *key,
			      int nonce_len, int tag_len) {
  // Create an OpenSSL EVP context. It does not yet have any specific
  // cipher associated with it.
  if (!(*in_ctx = EVP_CIPHER_CTX_new())) {
    return -3;
  }
  EVP_CIPHER_CTX *ctx = *in_ctx;
  // Although OCB-AES has the same initialization process between
  // encryption and decryption, an EVP_CIPHER_CTX must be initialized
  // for a specific direction.
  if (EVP_CipherInit_ex(ctx, EVP_aes_128_ocb(),
			/*impl=*/NULL, /*key=*/key, /*iv=*/NULL,
			direction) != 1) {
    return -3;
  }
  // Attempt to set the nonce length. If it fails, the length must not
  // be supported. However, that should have been handled by the
  // pre-condition check above.
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN,
			  nonce_len, NULL) != 1) {
    return -3;
  }
  // A NULL tag length means that EVP_CTRL_AEAD_SET_TAG is only being
  // used to set the length
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG,
			  tag_len, NULL) != 1) {
    return -3;
  }
  return AE_SUCCESS;
}

int ae_init(ae_ctx *ctx, const void *key, int key_len, int nonce_len,
	    int tag_len) {
  // Pre-condition: Only nonces of length 12 are supported. The
  // documentation of `ae_init` in ae.h specifies that `ctx` is
  // untouched if an invalid configuration is requested. Delegating
  // this to OpenSSL would happen too late; `ctx` has already been
  // modified.
  if (nonce_len != 12) {
    return AE_NOT_SUPPORTED;
  }
  // Pre-condition: Only AES-128 is supported.
  if (key_len != 16) {
    return AE_NOT_SUPPORTED;
  }
  int r = AE_SUCCESS;
  if ((r = ae_evp_cipher_init(&ctx->enc_ctx, 1,
			      reinterpret_cast<const unsigned char *>(key),
			      nonce_len, tag_len))!= AE_SUCCESS) {
    return r;
  }
  if ((r = ae_evp_cipher_init(&ctx->dec_ctx, 0,
			      reinterpret_cast<const unsigned char *>(key),
			      nonce_len, tag_len)) != AE_SUCCESS) {
    return r;
  }
  ctx->tag_len = tag_len;
  return AE_SUCCESS;
}

int ae_encrypt(ae_ctx *ctx, const void *nonce_ptr, const void *pt_ptr,
	       int pt_len, const void *ad_ptr, int ad_len, void *ct_ptr,
	       void *tag, int final) {
  const unsigned char *nonce =
    reinterpret_cast<const unsigned char *>(nonce_ptr);
  const unsigned char *pt = reinterpret_cast<const unsigned char *>(pt_ptr);
  const unsigned char* ad = reinterpret_cast<const unsigned char *>(ad_ptr);
  unsigned char* ct = reinterpret_cast<unsigned char *>(ct_ptr);
  // Streaming mode is not supported; nonce must always be provided.
  if (final != AE_FINALIZE) {
    return AE_NOT_SUPPORTED;
  }
  if (nonce == NULL) {
    return AE_NOT_SUPPORTED;
  }
  if (EVP_EncryptInit_ex(ctx->enc_ctx, /*type=*/NULL, /*impl=*/NULL,
			 /*key=*/NULL, nonce) != 1) {
    return -3;
  }
  int len = 0;
  if (ad != NULL && ad_len > 0 &&
      EVP_EncryptUpdate(ctx->enc_ctx, /*out=*/NULL, &len, ad, ad_len) != 1) {
     return -3;
  }
  len = 0;
  if (pt != NULL && pt_len > 0 &&
      EVP_EncryptUpdate(ctx->enc_ctx, ct, &len, pt, pt_len) != 1) {
    return -3;
  }
  int ciphertext_len = len;
  if (EVP_EncryptFinal_ex(ctx->enc_ctx, ct + ciphertext_len, &len) != 1) {
    return -3;
  }
  ciphertext_len += len;
  // If `tag` is provided, the authentication tag goes
  // there. Otherwise, it is appended after the ciphertext.
  void *tag_location = tag != NULL ? tag : ct + ciphertext_len;
  if (EVP_CIPHER_CTX_ctrl(ctx->enc_ctx, EVP_CTRL_AEAD_GET_TAG,
			  ctx->tag_len, tag_location) != 1) {
    return -3;
  }
  if (tag == NULL) {
    ciphertext_len += ctx->tag_len;
  }
  return ciphertext_len;
}


int ae_decrypt(ae_ctx *ctx, const void *nonce_ptr, const void *ct_ptr,
	       int ct_len, const void *ad_ptr, int ad_len, void *pt_ptr,
	       const void *tag, int final) {
  const unsigned char *nonce =
    reinterpret_cast<const unsigned char *>(nonce_ptr);
  const unsigned char *ct = reinterpret_cast<const unsigned char *>(ct_ptr);
  const unsigned char* ad = reinterpret_cast<const unsigned char *>(ad_ptr);
  unsigned char* pt = reinterpret_cast<unsigned char *>(pt_ptr);
  if (ct_len < ctx->tag_len) {
    return AE_INVALID;
  }
  // If an external tag is not provided, then the tag is assumed to be
  // the final bytes of the cipher text. Subtract it off now so the
  // plaintext does not accidentally try to decrypt the AEAD tag (and
  // then cause an authentication failure).
  if (tag == NULL) {
    ct_len -= ctx->tag_len;
  }
  // Like encryption, nonce must always be provided and streaming is not supported.
  if (final != AE_FINALIZE) {
    return AE_NOT_SUPPORTED;
  }
  if (nonce == NULL) {
    return AE_NOT_SUPPORTED;
  }
  if (EVP_DecryptInit_ex(ctx->dec_ctx, /*type=*/NULL, /*impl=*/NULL,
			 /*key=*/NULL, nonce) != 1) {
    return -3;
  }
  int len = 0;
  if (ad != NULL && ad_len > 0 &&
      EVP_DecryptUpdate(ctx->dec_ctx, /*out=*/NULL, &len, ad, ad_len) != 1) {
     return -3;
  }
  len = 0;
  if (ct != NULL && ct_len > 0 &&
      EVP_DecryptUpdate(ctx->dec_ctx, pt, &len, ct, ct_len) != 1) {
    return -3;
  }
  int plaintext_len = len;
  // If `tag` is provided, the authentication is read from
  // there. Otherwise, it's the last bytes of the ciphertext. (This is
  // a convention, not a requirement of OCB mode).
  const void *tag_location = tag != NULL ? tag : ct + ct_len;
  if (EVP_CIPHER_CTX_ctrl(ctx->dec_ctx, EVP_CTRL_AEAD_SET_TAG,
			  ctx->tag_len, (void *)tag_location) != 1) {
    return -3;
  }
  if (EVP_DecryptFinal_ex(ctx->dec_ctx, pt + plaintext_len, &len) != 1) {
    return AE_INVALID;
  }
  plaintext_len += len;
  return plaintext_len;
}
