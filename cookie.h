typedef struct s_cookie {
	size_t PathLen;
	char * PathStr;
	size_t ValueMax;
	size_t ValueLen;
	char * ValueStr;
	size_t NameLen;
	char   NameStr[1];
} * COOKIE;

typedef struct s_cookie_set {
	COOKIE Cookie[20];
} COOKIESET;

void cookie_set (LOCATION, const char *, long len, long srvr_date);
WORD cookie_Jar (LOCATION, COOKIESET *);
