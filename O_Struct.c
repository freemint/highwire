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
struct font_step *
new_step (WORD size, WORD color)
{
	struct font_step * temp = malloc (sizeof (struct font_step));
	if (temp) {
		temp->step               = size;
		temp->color              = color;
		temp->previous_font_step = NULL;
	}
	return (temp);
}

/*============================================================================*/
struct font_step *
add_step (struct font_step *old)
{
	struct font_step * temp = new_step (old->step, old->color);
	if (temp) {
		temp->previous_font_step = old;
	} else {
		temp = old; /* not very clever here but better than a crash */
	}
	return (temp);
}

/*============================================================================*/
struct font_step *
destroy_step (struct font_step *current)
{
	if (current && current->previous_font_step) {
		struct font_step * temp = current->previous_font_step;
		free (current);

		return (temp);
	}
	
	return (current);
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
