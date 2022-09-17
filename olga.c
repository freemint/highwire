#include <stdio.h>
#include <string.h>

#include "olga.h"
#include "global.h"
#include "Location.h"
#include "cache.h"
#include "hwWind.h"

static void send_olga_init(void);
static void send_olga_exit(void);


static int olga_id;
static int olga_active;
static int olga_group = 0;

/* All GLOBAL memory is merged to one block and allocated in main(). */
char *olga_memory; /* HW_PATH_MAX byte */

/*============================================================================*/

static void send_olga_init(void)
{
	short msg[8];

	msg[0] = OLE_INIT;
	msg[1] = gl_apid;
	msg[2] = 0;
	msg[3] = OL_CLIENT;
	msg[4] = msg[5] = msg[6] = msg[7] = 0;

	appl_write (olga_id, 16, msg);
}

/*============================================================================*/

static void send_olga_exit(void)
{
	short msg[8];

	msg[0] = OLE_EXIT;
	msg[1] = gl_apid;
	msg[2] = msg[3] = msg[4] = msg[5] = msg[6] = msg[7] = 0;

	appl_write (olga_id, 16, msg);
}

/*============================================================================*/

void send_olga_link ( const char *s )
{
	short msg[8];

	if ( olga_active )
	{
		olga_group++;
		
		strcpy ( olga_memory, s );
		
		msg[0] = OLGA_LINK;
		msg[1] = gl_apid;
		msg[2] = 0;
		*(char**)(msg +3) = olga_memory;
		msg[5] = olga_group;
		msg[6] = msg[7] = 0;

		appl_write (olga_id, 16, msg);
	}
}

/*============================================================================*/

void handle_olga( short msg[8])
{
	switch (msg[0])
	{
		case OLE_NEW:
			olga_id = msg[1];
			send_olga_init ();
		break;
		case OLE_EXIT:
			olga_active = FALSE;
		break;
		case OLGA_INIT:
			olga_active = TRUE;
		break;
		case OLGA_UPDATED:
		{
			char buf[2 * HW_PATH_MAX];
			FRAME  frame;
			LOCATION loc;
			HwWIND wind = hwWind_Top;
			
			strcpy ( olga_memory, *(char**)(msg +3) );
			while (wind) {
				frame = hwWind_ActiveFrame (wind);
		 		loc = (frame ? frame->Location : NULL);
				if (PROTO_isRemote (loc->Proto)) {
					CACHED cached = cache_lookup (loc, 0, NULL);
					if (cached) {
						union { CACHED c; LOCATION l; } u;
						u.c = cache_bound (cached, NULL);
						loc = u.l;
					}
				}
				location_FullName (loc, buf, sizeof(buf));
				if ( strcmp ( olga_memory , buf ) == 0 ) {
					hwWind_history (wind, wind->HistMenu, TRUE);
					break;
				}
				wind = hwWind_Next (wind);
			}
		}
		break;
	}
}

/*============================================================================*/

void Init_OLGA(void)
{
	if (!olga_active)
	{
		olga_id = appl_find("OLGA    ");
		if (olga_id > 0)
			send_olga_init();
	}
}

/*============================================================================*/

void Exit_OLGA(void)
{
	if (olga_active)
		send_olga_exit();
}
