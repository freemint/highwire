
#include <stddef.h>

#include <ldg.h>

#include "../../src/hw-types.h"
#include "../../src/mbedTLS.h"
#include "network.h"

LDG_MBEDTLS_FTAB *inet_ldg_mbedtls_ftab;

WORD sockets_free = 0;

#define ldg_mbedtls_ssl_read(a,b,c) (*inet_ldg_mbedtls_ftab->ldg_mbedtls_ssl_read)(a,b,c)
#define ldg_mbedtls_ssl_write(a,b,c) (*inet_ldg_mbedtls_ftab->ldg_mbedtls_ssl_write)(a,b,c)
#define ldg_mbedtls_ssl_close_notify(a) (*inet_ldg_mbedtls_ftab->ldg_mbedtls_ssl_close_notify)(a)

#if defined(USE_MINTNET)
#include "mintnet.c"
#elif defined(USE_MAGICNET)
#include "magxnet.c"
#elif defined(USE_STING) // TODO: try to use libcmini to shrink file
#include "sting.c"
#elif defined(USE_ICONNECT) // TODO: fix at least compilation for iconnect
#include "iconnect/socklib.c"
#include "iconnect/inet.c"
#include "iconnect/host.c"
#include "iconnect/getent.c"
#include "iconnect/url_aid.c"
#include "iconnect/stream_i.c"
#include "iconnect/set_flag.c"
#include "iconnect.c"
#endif

PROC LibFunc[] =
{
  {"inet_host_addr", "int16_t host_addr(const char * host, int32_t * addr);\n", inet_host_addr},
  {"inet_connect", "int32_t connect(int32_t addr, int32_t port, int32_t tout_sec);\n", inet_connect},
  
  {"inet_send", "long send(int32_t fh, const char * buf, size_t len, my_ssl_context *ssl_context);\n", inet_send},
  {"inet_recv", "long recv(int32_t fh, char       * buf, size_t len, my_ssl_context *ssl_context);\n", inet_recv},
  {"inet_close", "void close(int32_t fh, my_ssl_context *ssl_context);\n", inet_close},

  {"inet_instat", "int32_t instat(int32_t fh);\n", inet_instat},
  {"inet_select", "int32_t select (int32_t timeout, int32_t * rfds, int32_t * wfds);;\n", inet_select},
 
  {"inet_info", "const char * info(int32_t fh);\n", inet_info},
  {"inet_type", "const int16_t type(void);\n", inet_type},
  {"inet_mbedtls_set", "void mbedtls_set(LDG_MBEDTLS_FTAB *ftab);\n", inet_mbedtls_set},
};

#if defined(USE_MINTNET)
LDGLIB LibLdg[] = { { 0x0001,  10, LibFunc,  "MiNTnet overlay for HighWire, by AltF4@freemint.de", 1} };
#elif defined(USE_MAGICNET)
LDGLIB LibLdg[] = { { 0x0001,  10, LibFunc,  "MagxNet overlay for HighWire, by gerhard_stoll@gmx.de", 1} };
#elif defined(USE_ICONNECT)
LDGLIB LibLdg[] = { { 0x0001,  10, LibFunc,  "IConnect overlay for HighWire, by AltF4@freemint.de", 1} };
#elif defined(USE_STING)
LDGLIB LibLdg[] = { { 0x0001,  10, LibFunc,  "STinG/STiK2 overlay for HighWire, by AltF4@freemint.de", 1} };
#endif

int main(void)
{
  ldg_init(LibLdg);
  return 0;
}
