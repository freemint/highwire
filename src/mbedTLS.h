/* @(#)highwire/mbedTLS.h
 *
 * This module is gateway to use the mbedTLS.ldg
 * Rajah Lone, 2026-09-24
 */

#ifndef __LDG_MBEDTLS_H__
#define __LDG_MBEDTLS_H__

#include "hw-types.h"

#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

#include "ldg.h"

#define X509_CERT_INFO_BUFFER 2048

#define SECURE_PROTOCOL_SSLv3_0 0
#define SECURE_PROTOCOL_TLSv1_0 1
#define SECURE_PROTOCOL_TLSv1_1 2
#define SECURE_PROTOCOL_TLSv1_2 3
#define SECURE_PROTOCOL_TLSv1_3 4

#define SECURE_PROTOCOL_MIN SECURE_PROTOCOL_SSLv3_0
#define SECURE_PROTOCOL_MAX SECURE_PROTOCOL_TLSv1_3

#define BADCERT_EXPIRED      0x01
#define BADCERT_REVOKED      0x02
#define BADCERT_CN_MISMATCH  0x04
#define BADCERT_NOT_TRUSTED  0x08

#define MBEDTLS_ERR_X509_CERT_VERIFY_FAILED -0x2700  /* Certificate verification failed */

extern UWORD        cfg_SecProtMin;  /* wanted minimum protocol for HTTPS    */
extern UWORD        cfg_SecProtMax;  /* wanted maximum protocol for HTTPS    */
extern UWORD        cfg_SrvCertVer;  /* verify webserver certificate with cacert.pem: 1 = enabled, 0 = disabled */

typedef struct { mbedtls_ctr_drbg_context drbg_ctx; mbedtls_entropy_context entr_ctx; } rng_context_t; // th-otto
typedef struct { mbedtls_pk_context pk; rng_context_t rng; } my_pk_context; // th-otto
typedef struct { mbedtls_ssl_config conf; mbedtls_ssl_context ssl; } my_ssl_context; // th-otto

typedef struct
{
  const char* __CDECL (*ldg_mbedtls_get_version)();
  
  void __CDECL (*ldg_mbedtls_set_aes_global)(short *gl);
  void __CDECL (*ldg_mbedtls_force_tcp_layer)(int32_t value);

  size_t __CDECL (*ldg_mbedtls_get_sizeof_x509_crt)();
  size_t __CDECL (*ldg_mbedtls_get_sizeof_pk_context)();
  size_t __CDECL (*ldg_mbedtls_get_sizeof_entropy_context)();
  size_t __CDECL (*ldg_mbedtls_get_sizeof_ctr_drbg_context)();
  size_t __CDECL (*ldg_mbedtls_get_sizeof_ssl_context)();

  void __CDECL (*ldg_mbedtls_x509_crt_init)(mbedtls_x509_crt *crt);
  int32_t __CDECL (*ldg_mbedtls_x509_crt_parse)(mbedtls_x509_crt *chain, const char *buf, size_t len);
  int32_t __CDECL (*ldg_mbedtls_x509_crt_info)(char *buf, size_t size, const mbedtls_x509_crt *crt);
  void __CDECL (*ldg_mbedtls_x509_crt_free)(mbedtls_x509_crt *crt);

  void __CDECL (*ldg_mbedtls_pk_init)(my_pk_context *pk);
  int32_t __CDECL (*ldg_mbedtls_pk_parse)(my_pk_context *pk, const char *key, size_t keylen);
  void __CDECL (*ldg_mbedtls_pk_free)(my_pk_context *pk);

  int32_t __CDECL (*ldg_mbedtls_entropy_init)(mbedtls_entropy_context *entr_ctx, mbedtls_ctr_drbg_context *drbg_ctx, const char *app_name);
  void __CDECL (*ldg_mbedtls_entropy_free)(mbedtls_entropy_context *entr_ctx, mbedtls_ctr_drbg_context *drbg_ctx);
  
  int32_t __CDECL (*ldg_mbedtls_ssl_init)(my_ssl_context *ssl, mbedtls_ctr_drbg_context *drbg_ctx, int32_t *server_fd, const char *servername, mbedtls_x509_crt *cacert, mbedtls_x509_crt *cert, my_pk_context *pk);
  void __CDECL (*ldg_mbedtls_ssl_set_minmax_version)(my_ssl_context *ssl, int32_t min, int32_t max);
  void __CDECL (*ldg_mbedtls_ssl_set_ciphersuite)(my_ssl_context *ssl, const int32_t *wished_ciphersuites);
  int32_t __CDECL (*ldg_mbedtls_ssl_handshake)(my_ssl_context *ssl);
  const char* __CDECL (*ldg_mbedtls_ssl_get_version)(my_ssl_context *ssl);
  const char* __CDECL (*ldg_mbedtls_ssl_get_ciphersuite)(my_ssl_context *ssl);
  int32_t __CDECL (*ldg_mbedtls_ssl_get_verify_result)(my_ssl_context *ssl);
  const mbedtls_x509_crt* __CDECL (*ldg_mbedtls_ssl_get_peer_cert)(my_ssl_context *ssl);
  int32_t __CDECL (*ldg_mbedtls_ssl_read)(my_ssl_context *ssl, char *buf, size_t len);
  int32_t __CDECL (*ldg_mbedtls_ssl_write)(my_ssl_context *ssl, const char *buf, size_t len);
  int32_t __CDECL (*ldg_mbedtls_ssl_close_notify)(my_ssl_context *ssl);
  void __CDECL (*ldg_mbedtls_ssl_free)(my_ssl_context *ssl);

} LDG_MBEDTLS_FTAB;

#define ldg_mbedtls_get_version() (*mbedtls_ftab->ldg_mbedtls_get_version)()

#define ldg_mbedtls_set_aes_global(a) (*mbedtls_ftab->ldg_mbedtls_set_aes_global)(a)
#define ldg_mbedtls_force_tcp_layer(a) (*mbedtls_ftab->ldg_mbedtls_force_tcp_layer)(a)

#define ldg_mbedtls_get_sizeof_x509_crt() (*mbedtls_ftab->ldg_mbedtls_get_sizeof_x509_crt)()
#define ldg_mbedtls_get_sizeof_pk_context() (*mbedtls_ftab->ldg_mbedtls_get_sizeof_pk_context)()
#define ldg_mbedtls_get_sizeof_entropy_context() (*mbedtls_ftab->ldg_mbedtls_get_sizeof_entropy_context)()
#define ldg_mbedtls_get_sizeof_ctr_drbg_context() (*mbedtls_ftab->ldg_mbedtls_get_sizeof_ctr_drbg_context)()
#define ldg_mbedtls_get_sizeof_ssl_context() (*mbedtls_ftab->ldg_mbedtls_get_sizeof_ssl_context)()

#define ldg_mbedtls_x509_crt_init(a) (*mbedtls_ftab->ldg_mbedtls_x509_crt_init)(a)
#define ldg_mbedtls_x509_crt_parse(a,b,c) (*mbedtls_ftab->ldg_mbedtls_x509_crt_parse)(a,b,c)
#define ldg_mbedtls_x509_crt_info(a,b,c) (*mbedtls_ftab->ldg_mbedtls_x509_crt_info)(a,b,c)
#define ldg_mbedtls_x509_crt_free(a) (*mbedtls_ftab->ldg_mbedtls_x509_crt_free)(a)

#define ldg_mbedtls_pk_init(a) (*mbedtls_ftab->ldg_mbedtls_pk_init)(a)
#define ldg_mbedtls_pk_parse(a,b) (*mbedtls_ftab->ldg_mbedtls_pk_parse)(a,b)
#define ldg_mbedtls_pk_free(a) (*mbedtls_ftab->ldg_mbedtls_pk_free)(a)

#define ldg_mbedtls_entropy_init(a,b,c) (*mbedtls_ftab->ldg_mbedtls_entropy_init)(a,b,c)
#define ldg_mbedtls_entropy_free(a,b) (*mbedtls_ftab->ldg_mbedtls_entropy_free)(a,b)

#define ldg_mbedtls_ssl_init(a,b,c,d,e,f,g) (*mbedtls_ftab->ldg_mbedtls_ssl_init)(a,b,c,d,e,f,g)
#define ldg_mbedtls_ssl_set_minmax_version(a,b,c) (*mbedtls_ftab->ldg_mbedtls_ssl_set_minmax_version)(a,b,c)
#define ldg_mbedtls_ssl_set_ciphersuite(a,b) (*mbedtls_ftab->ldg_mbedtls_ssl_set_ciphersuite)(a,b)
#define ldg_mbedtls_ssl_handshake(a) (*mbedtls_ftab->ldg_mbedtls_ssl_handshake)(a)
#define ldg_mbedtls_ssl_get_version(a) (*mbedtls_ftab->ldg_mbedtls_ssl_get_version)(a)
#define ldg_mbedtls_ssl_get_ciphersuite(a) (*mbedtls_ftab->ldg_mbedtls_ssl_get_ciphersuite)(a)
#define ldg_mbedtls_ssl_get_verify_result(a) (*mbedtls_ftab->ldg_mbedtls_ssl_get_verify_result)(a)
#define ldg_mbedtls_ssl_get_peer_cert(a) (*mbedtls_ftab->ldg_mbedtls_ssl_get_peer_cert)(a)
#define ldg_mbedtls_ssl_free(a) (*mbedtls_ftab->ldg_mbedtls_ssl_free)(a)

extern LDG_MBEDTLS_FTAB *mbedtls_ftab;

extern mbedtls_x509_crt *ldg_mbedtls_cacert;
extern int32_t *ldg_mbedtls_wanted_ciphersuite;

extern mbedtls_x509_crt *ldg_mbedtls_client_x509_cert;
extern my_pk_context *ldg_mbedtls_client_pk;

void ldg_mbedtls_init(WORD *gl);
LDG *ldg_mbedtls_load(void);
BOOL ldg_has_mbedtls(void);
void ldg_mbedtls_unload(void);

void ldg_mbedtls_verify_certs(int mode);
void ldg_mbedtls_set_trusted_domains(const char *list);
int16_t ldg_mbedtls_is_trusted_domain(char *name);

void ldg_mbedtls_set_csfile(const char *pathname);
void ldg_mbedtls_load_csfile(void);

#endif /* __LDG_MBEDTLS_H__ */
