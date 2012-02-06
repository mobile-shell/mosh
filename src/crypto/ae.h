/* ---------------------------------------------------------------------------
 *
 * AEAD API 0.12 - 13 July 2011
 *
 * This file gives an interface appropriate for many authenticated
 * encryption with associated data (AEAD) implementations. It does not try
 * to accommodate all possible options or limitations that an implementation
 * might have -- you should consult the documentation of your chosen
 * implementation to find things like RFC 5116 constants, alignment
 * requirements, whether the incremental interface is supported, etc.
 *
 * This file is in the public domain. It is provided "as is", without
 * warranty of any kind. Use at your own risk.
 *
 * Comments are welcome: Ted Krovetz <ted@krovetz>.
 *
 * ------------------------------------------------------------------------ */

#ifndef _AE_H_
#define _AE_H_

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 *
 * Constants
 *
 * ----------------------------------------------------------------------- */

/* Return status codes: Negative return values indicate an error occurred.
 * For full explanations of error values, consult the implementation's
 * documentation.                                                          */
#define AE_SUCCESS       ( 0)  /* Indicates successful completion of call  */
#define AE_INVALID       (-1)  /* Indicates bad tag during decryption      */
#define AE_NOT_SUPPORTED (-2)  /* Indicates unsupported option requested   */

/* Flags: When data can be processed "incrementally", these flags are used
 * to indicate whether the submitted data is the last or not.               */
#define AE_FINALIZE      (1)   /* This is the last of data                  */
#define AE_PENDING       (0)   /* More data of is coming                    */

/* --------------------------------------------------------------------------
 *
 * AEAD opaque structure definition
 *
 * ----------------------------------------------------------------------- */

typedef struct _ae_ctx ae_ctx;

/* --------------------------------------------------------------------------
 *
 * Data Structure Routines
 *
 * ----------------------------------------------------------------------- */

ae_ctx* ae_allocate  (void *misc);  /* Allocate ae_ctx, set optional ptr   */
void    ae_free      (ae_ctx *ctx); /* Deallocate ae_ctx struct            */
int     ae_clear     (ae_ctx *ctx); /* Undo initialization                 */
int     ae_ctx_sizeof(void);        /* Return sizeof(ae_ctx)               */
/* ae_allocate() allocates an ae_ctx structure, but does not initialize it.
 * ae_free() deallocates an ae_ctx structure, but does not zeroize it.
 * ae_clear() zeroes sensitive values associated with an ae_ctx structure
 * and deallocates any auxiliary structures allocated during ae_init().
 * ae_ctx_sizeof() returns sizeof(ae_ctx), to aid in any static allocations.
 */

/* --------------------------------------------------------------------------
 *
 * AEAD Routines
 *
 * ----------------------------------------------------------------------- */

int ae_init(ae_ctx     *ctx,
            const void *key,
            int         key_len,
            int         nonce_len,
            int         tag_len);
/* --------------------------------------------------------------------------
 *
 * Initialize an ae_ctx context structure.
 *
 * Parameters:
 *  ctx       - Pointer to an ae_ctx structure to be initialized
 *  key       - Pointer to user-supplied key
 *  key_len   - Length of key supplied, in bytes
 *  nonce_len - Length of nonces to be used for this key, in bytes
 *  tag_len   - Length of tags to be produced for this key, in bytes
 *
 * Returns:
 *  AE_SUCCESS       - Success. Ctx ready for use.
 *  AE_NOT_SUPPORTED - An unsupported length was supplied. Ctx is untouched.
 *  Otherwise        - Error. Check implementation documentation for codes.
 *
 * ----------------------------------------------------------------------- */

int ae_encrypt(ae_ctx     *ctx,
               const void *nonce,
               const void *pt,
               int         pt_len,
               const void *ad,
               int         ad_len,
               void       *ct,
               void       *tag,
               int         final);
/* --------------------------------------------------------------------------
 *
 * Encrypt plaintext; provide for authentication of ciphertext/associated data.
 *
 * Parameters:
 *  ctx    - Pointer to an ae_ctx structure initialized by ae_init.
 *  nonce  - Pointer to a nonce_len (defined in ae_init) byte nonce.
 *  pt     - Pointer to plaintext bytes to be encrypted.
 *  pt_len - number of bytes pointed to by pt.
 *  ad     - Pointer to associated data.
 *  ad_len - number of bytes pointed to by ad.
 *  ct     - Pointer to buffer to receive ciphertext encryption.
 *  tag    - Pointer to receive authentication tag; or NULL
 *           if tag is to be bundled into the ciphertext.
 *  final  - Non-zero if this call completes the plaintext being encrypted.
 *
 * If nonce!=NULL then a message is being initiated. If final!=0
 * then a message is being finalized. If final==0 or nonce==NULL
 * then the incremental interface is being used. If nonce!=NULL and
 * ad_len<0, then use same ad as last message.
 *
 * Returns:
 *  non-negative     - Number of bytes written to ct.
 *  AE_NOT_SUPPORTED - Usage mode unsupported (eg, incremental and/or sticky).
 *  Otherwise        - Error. Check implementation documentation for codes.
 *
 * ----------------------------------------------------------------------- */

int ae_decrypt(ae_ctx     *ctx,
               const void *nonce,
               const void *ct,
               int         ct_len,
               const void *ad,
               int         ad_len,
               void       *pt,
               const void *tag,
               int         final);
/* --------------------------------------------------------------------------
 *
 * Decrypt ciphertext; provide authenticity of plaintext and associated data.
 *
 * Parameters:
 *  ctx    - Pointer to an ae_ctx structure initialized by ae_init.
 *  nonce  - Pointer to a nonce_len (defined in ae_init) byte nonce.
 *  ct     - Pointer to ciphertext bytes to be decrypted.
 *  ct_len - number of bytes pointed to by ct.
 *  ad     - Pointer to associated data.
 *  ad_len - number of bytes pointed to by ad.
 *  pt     - Pointer to buffer to receive plaintext decryption.
 *  tag    - Pointer to tag_len (defined in ae_init) bytes; or NULL
 *           if tag is bundled into the ciphertext. Non-NULL tag is only
 *           read when final is non-zero.
 *  final  - Non-zero if this call completes the ciphertext being decrypted.
 *
 * If nonce!=NULL then "ct" points to the start of a ciphertext. If final!=0
 * then "in" points to the final piece of ciphertext. If final==0 or nonce==
 * NULL then the incremental interface is being used. If nonce!=NULL and
 * ad_len<0, then use same ad as last message.
 *
 * Returns:
 *  non-negative     - Number of bytes written to pt.
 *  AE_INVALID       - Authentication failure.
 *  AE_NOT_SUPPORTED - Usage mode unsupported (eg, incremental and/or sticky).
 *  Otherwise        - Error. Check implementation documentation for codes.
 *
 * NOTE !!! NOTE !!! -- The ciphertext should be assumed possibly inauthentic
 *                      until it has been completely written and it is
 *                      verified that this routine did not return AE_INVALID.
 *
 * ----------------------------------------------------------------------- */

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif

#endif /* _AE_H_ */
