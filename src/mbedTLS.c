/* @(#)highwire/mbedTLS.c
 *
 * This module is gateway to use the mbedTLS.ldg
 * Rajah Lone, 2026-09-24
 */


#include "hw-types.h"
#include "Logging.h"
#include "inet.h"
#include "mbedTLS.h"

#include "ldg.h"
#include "gem.h"

#ifdef __GEMLIB__
#define ldg_global	aes_global
#else
#ifdef __PUREC__	/* For Pure C users using PCGEMLIB */
#define ldg_global	((short*)&_GemParBlk.global[0])
#endif
#endif

/*-------------------------------------------------------------------*/

LDG *libmbedtls = NULL;

LDG_MBEDTLS_FTAB *mbedtls_ftab = NULL;

WORD *app_global_mbedtls = NULL;

// TODO: SSL protocols choices with min and max, from preferences in highwire.cfg, if 0-3: load polarssl.ldg, if 3-4: load mbedtls.ldg

mbedtls_x509_crt *ldg_mbedtls_cacert = NULL; // TODO: if in highwire.cfg, load cacert.pem from mbedtls.ldg folder
int32_t *ldg_mbedtls_wanted_ciphersuite = NULL; // TODO: if in highwire.cfg, load choosen cs file

mbedtls_x509_crt *ldg_mbedtls_client_x509_cert = NULL; // TODO: client x509_cert and pk load and declaration for some websites?
my_pk_context *ldg_mbedtls_client_pk = NULL;          

/*-------------------------------------------------------------------*/

void ldg_mbedtls_init(WORD *gl) { app_global_mbedtls = gl; }

/*-------------------------------------------------------------------*/

LDG *ldg_mbedtls_load()
{
  if (libmbedtls != NULL) { return libmbedtls; }
 
  const char *libname = "mbedtls.ldg";
  
  if (app_global_mbedtls == NULL) { app_global_mbedtls = ldg_global; }

  if (mbedtls_ftab == NULL) { mbedtls_ftab = ldg_Calloc(1, sizeof(LDG_MBEDTLS_FTAB)); }
  
  if (mbedtls_ftab != NULL)
  {
    libmbedtls = ldg_open(libname, app_global_mbedtls); // reluctant to use (CHAR *)

    if (libmbedtls != NULL)
    {
      mbedtls_ftab->ldg_mbedtls_get_version = ldg_find("get_version", libmbedtls);
	  
      mbedtls_ftab->ldg_mbedtls_set_aes_global = ldg_find("set_aes_global", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_force_tcp_layer = ldg_find("force_tcp_layer", libmbedtls);
    
      mbedtls_ftab->ldg_mbedtls_get_sizeof_x509_crt = ldg_find("get_sizeof_x509_crt_struct", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_get_sizeof_pk_context = ldg_find("get_sizeof_pk_context_struct", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_get_sizeof_entropy_context = ldg_find("get_sizeof_entropy_context_struct", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_get_sizeof_ctr_drbg_context = ldg_find("get_sizeof_ctr_drbg_context_struct", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_get_sizeof_ssl_context = ldg_find("get_sizeof_ssl_context_struct", libmbedtls);
    
      mbedtls_ftab->ldg_mbedtls_x509_crt_init = ldg_find("ldg_x509_crt_init", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_x509_crt_parse = ldg_find("ldg_x509_crt_parse", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_x509_crt_info = ldg_find("ldg_x509_crt_info", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_x509_crt_free = ldg_find("ldg_x509_crt_free", libmbedtls);

      mbedtls_ftab->ldg_mbedtls_pk_init = ldg_find("ldg_pk_init", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_pk_parse = ldg_find("ldg_pk_parse", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_pk_free = ldg_find("ldg_pk_free", libmbedtls);

      mbedtls_ftab->ldg_mbedtls_entropy_init = ldg_find("ldg_entropy_init", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_entropy_free = ldg_find("ldg_entropy_free", libmbedtls);

      mbedtls_ftab->ldg_mbedtls_ssl_init = ldg_find("ldg_ssl_init", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_ssl_set_minmax_version = ldg_find("ldg_ssl_set_minmax_version", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_ssl_set_ciphersuite = ldg_find("ldg_ssl_set_ciphersuite", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_ssl_handshake = ldg_find("ldg_ssl_handshake", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_ssl_get_version = ldg_find("ldg_ssl_get_version", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_ssl_get_ciphersuite = ldg_find("ldg_ssl_get_ciphersuite", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_ssl_get_verify_result = ldg_find("ldg_ssl_get_verify_result", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_ssl_get_peer_cert = ldg_find("ldg_ssl_get_peer_cert", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_ssl_read = ldg_find("ldg_ssl_read", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_ssl_write = ldg_find("ldg_ssl_write", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_ssl_close_notify = ldg_find("ldg_ssl_close_notify", libmbedtls);
      mbedtls_ftab->ldg_mbedtls_ssl_free = ldg_find("ldg_ssl_free", libmbedtls);

      if (logging_is_on) { logprintf(LOG_LMAGENTA, "%s (%s) loaded\n", libname, ldg_mbedtls_get_version()); }
      
      ldg_mbedtls_set_aes_global(app_global_mbedtls);
      
      if (ldg_has_inet())
      {
        ldg_mbedtls_force_tcp_layer(inet_type() == 2 ? 2 : 1);
        
        inet_mbedtls_set(mbedtls_ftab); logprintf(LOG_LMAGENTA, "internet module for %s is informed of %s functions\n", inet_info(), libname);
      }
    }
  }
  
	return libmbedtls;
}

/*-------------------------------------------------------------------*/

BOOL ldg_has_mbedtls()
{
  if ((inet_type() < 1) || (inet_type() > 2)) { return FALSE; }
  
  if (libmbedtls == NULL) { ldg_mbedtls_load(); }
  
  return libmbedtls ? TRUE : FALSE;
}

/*-------------------------------------------------------------------*/

void ldg_mbedtls_unload()
{
	if (libmbedtls != NULL)
	{
		if (app_global_mbedtls == NULL) { app_global_mbedtls = ldg_global; }
    
    ldg_Free(mbedtls_ftab);
    
		ldg_close(libmbedtls, app_global_mbedtls);
	}
}
