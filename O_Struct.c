#include <stdlib.h>
#include <string.h>

#include "global.h"


/*============================================================================*/
ANCHOR
new_named_location (const char * address, OFFSET * origin)
{
	ANCHOR temp = malloc (sizeof (struct named_location));
	if (temp) {
		temp->offset.Origin = origin;
		temp->offset.X      = 0;
		temp->address       = address;
		temp->next_location = NULL;
	}
	return (temp);
}

/*============================================================================*/
void
destroy_named_location_structure (ANCHOR current_location)
{
	while (current_location) {
		ANCHOR temp = current_location->next_location;
		free (current_location);
		current_location = temp;
	}
}


/*============================================================================*/
struct url_link *
new_url_link (WORDITEM start, char * address, BOOL is_href, char * target)
{
	struct url_link * link = malloc (sizeof (struct url_link));
	if (link) {
		link->isHref   = is_href;
		link->address  = address;
		link->start    = start;
		link->u.target = target;
		link->encoding = ENCODING_WINDOWS1252;
	}
	return (link);
}
