
#include "inet.h"
#include "Logging.h"
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

LDG *libinet = NULL;

LDG_INET_FTAB *inet_ftab = NULL;

WORD *app_global_inet = NULL;

/*-------------------------------------------------------------------*/

void ldg_inet_init(WORD *gl)
{
  app_global_inet = gl;
  
  if (inet_ftab == NULL) { inet_ftab = ldg_Calloc(1, sizeof(LDG_INET_FTAB)); }

  if (inet_ftab != NULL)
  {
    inet_ftab->host_addr = no_inet_host_addr;
    inet_ftab->connect = no_inet_connect;
      
    inet_ftab->send = no_inet_send;
    inet_ftab->recv = no_inet_recv;
    inet_ftab->close = no_inet_close;
      
    inet_ftab->instat = no_inet_instat;
    inet_ftab->select = no_inet_select;
      
    inet_ftab->info = no_inet_info;
    inet_ftab->type = no_inet_type;
    inet_ftab->mbedtls_set = no_inet_mbedtls_set;
  }
}

/*-------------------------------------------------------------------*/

LDG *ldg_inet_load()
{
  if (libinet) { return libinet; }
 
  const char *libname = "modules\\network.ldg"; // TODO: choose automaticaly the module instead of leaving the user rename files?
  
  if (inet_ftab != NULL)
  {
    libinet = ldg_open(libname, app_global_inet); // reluctant to use (CHAR *)

    if (libinet != NULL)
    {
      inet_ftab->host_addr = ldg_find("inet_host_addr", libinet);
      inet_ftab->connect = ldg_find("inet_connect", libinet);
      
      inet_ftab->send = ldg_find("inet_send", libinet);
      inet_ftab->recv = ldg_find("inet_recv", libinet);
      inet_ftab->close = ldg_find("inet_close", libinet);
      
      inet_ftab->instat = ldg_find("inet_instat", libinet);
      inet_ftab->select = ldg_find("inet_select", libinet);
      
      inet_ftab->info = ldg_find("inet_info", libinet);
      inet_ftab->type = ldg_find("inet_type", libinet);
      inet_ftab->mbedtls_set = ldg_find("inet_mbedtls_set", libinet);
 
      if (logging_is_on) { logprintf(LOG_LMAGENTA, "%s (%s) loaded\n", libname, inet_info()); }
    }
  }
  
	return libinet;
}

/*-------------------------------------------------------------------*/

BOOL ldg_has_inet() { return (libinet != NULL); }

/*-------------------------------------------------------------------*/

void ldg_inet_unload()
{
	if (libinet != NULL)
	{
		if (app_global_inet == NULL) { app_global_inet = ldg_global; }
    
    ldg_Free(inet_ftab);
    
		ldg_close(libinet, app_global_inet);
	}
}

/*-------------------------------------------------------------------*/

// if no module is loaded (ie no use of internet calls, html browser only), the inetfab points to these noop functions

int16_t no_inet_host_addr(const char * name, int32_t * addr) { short ret = -1; (void)name; (void)addr; return ret; }
int32_t no_inet_connect(int32_t addr, int32_t port, int32_t tout_sec) { int fh = -1; (void)addr; (void)port; (void)tout_sec; return fh; }
int32_t no_inet_send(int32_t fh, const char * buf, size_t len, my_ssl_context *ssl) {	long ret = -1; (void)fh; (void)buf; (void)len; (void)ssl; return ret; }
int32_t no_inet_recv(int32_t fh, char * buf, size_t len, my_ssl_context *ssl) { long ret = -1; (void)fh; (void)buf; (void)len; (void)ssl; return ret; }
void no_inet_close(int32_t fh, my_ssl_context *ssl) { (void)fh; (void)ssl; }
int32_t no_inet_instat(int32_t fh) { long ret = -1; (void)fh; return ret; }
int32_t no_inet_select(int32_t timeout, int32_t * rfds, int32_t * wfds) { long ret = 0; (void)timeout; (void)rfds; (void)wfds; return ret; }
const char * no_inet_info() { return NULL; }
const int16_t no_inet_type() { return 0; }
void no_inet_mbedtls_set(LDG_MBEDTLS_FTAB *ftab) { (void)ftab; }

