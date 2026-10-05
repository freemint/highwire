
int16_t CDECL inet_host_addr (const char * name, int32_t * addr);
int32_t CDECL inet_connect (int32_t addr, int32_t port, int32_t tout_sec);
int32_t CDECL inet_send (int32_t fh, const char * buf, size_t len, my_ssl_context *ssl_context);
int32_t CDECL inet_recv (int32_t fh, char * buf, size_t len, my_ssl_context *ssl_context);
void CDECL inet_close (int32_t fh, my_ssl_context *ssl_context);
int32_t CDECL inet_instat (int32_t fh);
int32_t CDECL inet_select (int32_t timeout, int32_t * rfds, int32_t * wfds);
const char * CDECL inet_info (void);
const int16_t CDECL inet_type (void);
void CDECL inet_mbedtls_set (LDG_MBEDTLS_FTAB *ftab);
