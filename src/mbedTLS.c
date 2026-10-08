/* @(#)highwire/mbedTLS.c
 *
 * This module is gateway to use the mbedTLS.ldg
 * Rajah Lone, 2026-09-24
 */

#include <string.h>
#include <stdio.h>

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

static const char *libname_mbedtls  = "mbedtls.ldg";
static const char *libname_polarssl = "polarssl.ldg";
static const char *cacert_filename  = "cacert.pem";

mbedtls_x509_crt *ldg_mbedtls_cacert = NULL;
static char      *load_buffer_cacert = NULL;
static char      *ldg_trusted_names  = NULL;

int32_t *ldg_mbedtls_wanted_ciphersuite = NULL;
static char load_ciphersuite_filename[256] = {0};

mbedtls_x509_crt *ldg_mbedtls_client_x509_cert = NULL; // client x509_cert and pk load and declaration for some websites?
my_pk_context *ldg_mbedtls_client_pk = NULL;          


/*-------------------------------------------------------------------*/

static void ldg_cacert_unload()
{
  if (ldg_mbedtls_cacert) { ldg_mbedtls_x509_crt_free(ldg_mbedtls_cacert); ldg_Free(ldg_mbedtls_cacert); }
  if (load_buffer_cacert) { ldg_Free(load_buffer_cacert); }

  ldg_mbedtls_cacert = NULL;
  load_buffer_cacert = NULL;
}

static void ldg_trusted_unload()
{
  if (ldg_trusted_names) { ldg_Free(ldg_trusted_names); }

  ldg_trusted_names = NULL;
}

static void ldg_csfile_unload()
{
  if (ldg_mbedtls_wanted_ciphersuite) { ldg_Free(ldg_mbedtls_wanted_ciphersuite); }

  ldg_mbedtls_wanted_ciphersuite = NULL;
}

/*-------------------------------------------------------------------*/

void ldg_mbedtls_init(WORD *gl)
{
  app_global_mbedtls = gl;

  ldg_mbedtls_load_csfile();
}

/*-------------------------------------------------------------------*/

LDG *ldg_mbedtls_load()
{
  if (libmbedtls != NULL) { return libmbedtls; }
   
  if (app_global_mbedtls == NULL) { app_global_mbedtls = ldg_global; }

  if (mbedtls_ftab == NULL) { mbedtls_ftab = ldg_Calloc(1, sizeof(LDG_MBEDTLS_FTAB)); }
  
  if (mbedtls_ftab != NULL)
  {
    const char *libname = libname_mbedtls;

    if (cfg_SecProtMin > cfg_SecProtMax)
    {
      UWORD tmp = cfg_SecProtMax;
      cfg_SecProtMax = cfg_SecProtMin;
      cfg_SecProtMin = tmp;
    }

    if (cfg_SecProtMax > SECURE_PROTOCOL_TLSv1_2)
    {
      cfg_SecProtMin = SECURE_PROTOCOL_TLSv1_2;
    }
    else if ((cfg_SecProtMin < SECURE_PROTOCOL_TLSv1_2) && (cfg_SecProtMax <= SECURE_PROTOCOL_TLSv1_2))
    {
      libname = libname_polarssl;
    }

   libmbedtls = ldg_open(libname, app_global_mbedtls); // reluctant to use (CHAR *)
  
    if (!libmbedtls) // if polarssl.ldg is not installed, try to load mbedtls.ldg instead and fix min/max
    {
      cfg_SecProtMin = SECURE_PROTOCOL_TLSv1_2;
      cfg_SecProtMax = max(cfg_SecProtMax, SECURE_PROTOCOL_TLSv1_2);
      cfg_SecProtMax = min(cfg_SecProtMax, SECURE_PROTOCOL_MAX);
      
      libname = libname_mbedtls;
      
      libmbedtls = ldg_open(libname, app_global_mbedtls);
    }

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

      if (logging_is_on) { logprintf(LOG_LMAGENTA, "%s (%s) loaded with protocols min:%d, max:%d\n", libname, ldg_mbedtls_get_version(), cfg_SecProtMin, cfg_SecProtMax); }
      
      ldg_mbedtls_set_aes_global(app_global_mbedtls);
      
      if (ldg_has_inet())
      {
        ldg_mbedtls_force_tcp_layer(inet_type() == 2 ? 2 : 1);
        
        inet_mbedtls_set(mbedtls_ftab); if (logging_is_on) { logprintf(LOG_LMAGENTA, "internet module for %s is informed of %s functions\n", inet_info(), libname); }
        
        if (cfg_SrvCertVer) // loading cacert.pem for certificates verifications
        {
          char pathname[256];
          
          strcpy(pathname, libname);
          if (ldg_libpath(pathname, app_global_mbedtls))
          {
            int16_t i;
            for (i = strlen(pathname); i > -1; i--) { if (pathname[i] == '\\') { pathname[i] = '\0'; break; } if (i == 0) { pathname[0] = '\0'; } }
            if (strlen(pathname) == 0) { strcpy(pathname, "c:\\gemsys\\ldg\\"); }
            if (strlen(pathname) + strlen(cacert_filename) < 256) { strcat(pathname, cacert_filename); }
          }
          
        	FILE *f = fopen(pathname, "r");
          if (f)
          {
            size_t s;
            
            fseek(f, 0, SEEK_END);
            s = ftell(f);
            fseek(f, 0, SEEK_SET);
            
            if (s > 32)
            {
              load_buffer_cacert = (char *)ldg_Calloc(1, (int32_t)(s + (size_t)32)); // with additionnal nullbytes, the buffer is filled with a C string
              
              ldg_mbedtls_cacert = (mbedtls_x509_crt *)ldg_Calloc(1, ldg_mbedtls_get_sizeof_x509_crt());
              
              if (load_buffer_cacert && ldg_mbedtls_cacert)
              {
                if (fread(load_buffer_cacert, sizeof(int8_t), s, f) == s)
                {
                  ldg_mbedtls_x509_crt_init(ldg_mbedtls_cacert);
                  int32_t v = 0;
                  
                  if ((v = ldg_mbedtls_x509_crt_parse(ldg_mbedtls_cacert, load_buffer_cacert, (int32_t)(s + (size_t)4))) == 0)
                  {
                    if (logging_is_on) { logprintf(LOG_LMAGENTA, "%s loaded for webservers certificates verification\n", pathname); }
                  }
                  else
                  {
                    if (logging_is_on) { logprintf(LOG_LMAGENTA, "could not parse %s as certificate suite (error %d)\n", pathname, v); }
                    ldg_cacert_unload();
                  }
                }
                else
                {
                  if (logging_is_on) { logprintf(LOG_LMAGENTA, "could not read %s (%d bytes) as expected\n", pathname, (int32_t)s); }
                  ldg_cacert_unload();
                }
              }
              else
              {
                if (logging_is_on) { logprintf(LOG_LMAGENTA, "could not allocate memory (%d bytes) for %s\n", (int32_t)s, pathname); }
                ldg_cacert_unload();
              }
            }

            fclose (f);
          }
          else if (logging_is_on) { logprintf(LOG_LMAGENTA, "could not find %s for cabundle\n", pathname); }
        }
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
  ldg_trusted_unload();
  ldg_csfile_unload();

	if (libmbedtls != NULL)
	{
		if (app_global_mbedtls == NULL) { app_global_mbedtls = ldg_global; }
    
    ldg_cacert_unload();
    
    ldg_Free(mbedtls_ftab);
    
		ldg_close(libmbedtls, app_global_mbedtls);
  }
}

/*-------------------------------------------------------------------*/

void ldg_mbedtls_verify_certs(int mode)
{
  if (mode > 0) { cfg_SrvCertVer = 1; } else { cfg_SrvCertVer = 0; }
}

/*-------------------------------------------------------------------*/

void ldg_mbedtls_set_trusted_domains(const char *list)
{
  if (list)
  {
    size_t len = strlen(list);
    
    ldg_trusted_names = (char *)ldg_Calloc(1, len + (size_t)16);
    
    if (ldg_trusted_names)
    {
      int16_t i;
      
      strcat(ldg_trusted_names, "|");
      strcat(ldg_trusted_names, list);
      strcat(ldg_trusted_names, "|");
      
      for (i = 0; ldg_trusted_names[i] != '\0'; i++) { if (ldg_trusted_names[i] == ',') { ldg_trusted_names[i] = '|'; } }
    }
    else if (logging_is_on) { logprintf(LOG_LMAGENTA, "could not allocate memory (%d bytes) for exempted domains list\n", (int32_t)(len + (size_t)16)); }
  }
}

/*-------------------------------------------------------------------*/

int16_t ldg_mbedtls_is_trusted_domain(char *name)
{
  if (ldg_trusted_names)
  {
    char framed[96];
    memset(framed, 0, 96);
    
    strcat(framed, "|");
    strncat(framed, name, 94);
    strcat(framed, "|");

    if (strstr(ldg_trusted_names, framed)) { return 1; }
  }
  return 0;
}

/*-------------------------------------------------------------------*/

void ldg_mbedtls_set_csfile(const char *pathname) { if (strlen(pathname) < 256) { strcpy(load_ciphersuite_filename, pathname); } }

void ldg_mbedtls_load_csfile()
{
  if (strlen(load_ciphersuite_filename) == 0) { return; }
  
  FILE *f = fopen(load_ciphersuite_filename, "r");
  if (f)
  {
    size_t s;
            
    fseek(f, 0, SEEK_END);
    s = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (s > 0)
    {
      ldg_mbedtls_wanted_ciphersuite = (int32_t *)ldg_Calloc(1, (int32_t)(s + (size_t)32)); // with additionnal nullbytes, just in case

      if (ldg_mbedtls_wanted_ciphersuite)
      {
        if (fread(ldg_mbedtls_wanted_ciphersuite, sizeof(int8_t), s, f) == s)
        {
          if (logging_is_on) { logprintf(LOG_LMAGENTA, "%s loaded as ciphersuite\n", load_ciphersuite_filename); }
        }
        else
        {
          if (logging_is_on) { logprintf(LOG_LMAGENTA, "could not read %s (%d bytes) as expected\n", load_ciphersuite_filename, (int32_t)s); }
          ldg_csfile_unload();
        }
      }
      else if (logging_is_on) { logprintf(LOG_LMAGENTA, "could not allocate memory (%d bytes) for %s\n", (int32_t)s, load_ciphersuite_filename); }
    }

    fclose (f);
  }
  else if (logging_is_on) { logprintf(LOG_LMAGENTA, "could not find %s for ciphersuite\n", load_ciphersuite_filename); }
}
