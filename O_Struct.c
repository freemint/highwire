#include <stdlib.h>
#include <string.h>

#include "global.h"


void
destroy_named_location_structure (struct named_location *current_location)
{
	struct named_location *temp;

	while (current_location != 0)
	{
		temp = current_location->next_location;
		free (current_location);
		current_location = temp;
	}
}

struct named_location *
new_named_location (const char * address, OFFSET * origin)
{
	struct named_location *temp = malloc (sizeof (struct named_location));

	temp->offset.Origin = origin;
	temp->offset.X      = 0;
	temp->address       = address;
	temp->next_location = NULL;
	
	return (temp);
}

/* ****************************** */

struct font_step *
new_step (WORD size, WORD color)
{
	struct font_step * temp = malloc (sizeof (struct font_step));
	temp->step               = size;
	temp->colour             = color;
	temp->previous_font_step = NULL;
	
	return (temp);
}

struct font_step *
add_step (struct font_step *old)
{
	struct font_step * temp = new_step (old->step, old->colour);
	temp->previous_font_step = old;
	
	return (temp);
}

struct font_step *
destroy_step (struct font_step *current)
{
	struct font_step *temp;

	if (current->previous_font_step)
	{
		temp = current->previous_font_step;
		free (current);

		return (temp);
	}
	else
		return (current);
}

/* ******************************** */

struct url_link *
new_url_link (WORDITEM start, char * address, BOOL is_href, char * target)
{
	struct url_link * link = malloc (sizeof (struct url_link));

	link->isHref   = is_href;
	link->address  = address;
	link->start    = start;
	link->u.target = target;
	link->encoding = ENCODING_WINDOWS1252;

	return (link);
}
