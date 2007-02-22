#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "global.h"


/*============================================================================*/
ANCHOR
new_named_location (const char * address, DOMBOX * origin)
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


/*============================================================================*/
IMAGEMAP
create_imagemap (IMAGEMAP * list, const char * name, BOOL empty)
{
	size_t   len;
	IMAGEMAP map = *list;
	while (map) {
		if (stricmp (name, map->Name) == 0) {
			if (empty && map->Areas) {
				break; /* we need an empty map */
			} else {
				return map; /* found it */
			}
		}
		map = map->Next;
	}
	if ((len = strlen (name)) > 0 &&
	    (map = malloc (sizeof(struct s_img_map) + len)) != NULL) {
		map->Link.isHref   = TRUE;
		map->Link.address  = NULL;
		map->Link.start    = NULL;
		map->Link.u.target = NULL;
		map->Link.encoding = ENCODING_WINDOWS1252;
		memcpy (map->Name, name, len +1);
		map->Areas = NULL;
		map->Next  = *list;
		*list      = map;
	}
	return map;
}

/*============================================================================*/
void
destroy_imagemap (IMAGEMAP * list, BOOL all)
{
	IMAGEMAP map = *list;
	if (map && !all) {
		*list = map->Next;
		list  = &map->Next;
		map->Next = NULL;
	}
	while (map) {
		*list = map->Next;
		while (map->Areas) {
			MAPAREA area = map->Areas;
			map->Areas   = area->Next;
			if (area->Address) free (area->Address);
			if (area->Target)  free (area->Target);
			if (area->AltText) free (area->AltText);
			free (area);
		}
		free (map);
		map = *list;
	}
}

/*----------------------------------------------------------------------------*/
static size_t
read_coords (const char * buff, WORD * coord, size_t maxnum)
{
	size_t n = 0;
	
	while (isspace (*buff)) buff++;
	
	while (*buff && n < maxnum) {
		char * b = NULL;
		coord[n] = strtol (buff, &b, 10);
		if (!b || b == buff) break;
		buff = b;
		n   += 1;
		while (*buff == ',' || isspace (*buff)) buff++;
	}
	
	return n;
}

/*============================================================================*/
MAPAREA
new_maparea (const char * shape, const char * coords, char * href,
             char * target, char * text)
{
	MAPAREA area = NULL;
	
	if (stricmp (shape, "rect") == 0) {
		WORD n[4]; /* x1,y1, x2,y2 */
		if (read_coords (coords, n, numberof(n)) == 4
		    && n[0]/*x1*/ <= n[2]/*x2*/ && n[1]/*y1*/ <= n[3]/*y2*/
		    && (area = malloc (sizeof(struct s_map_area))) != NULL) {
			area->Type = 'R';
			area->u.Extent.g_w = n[2] - (area->u.Extent.g_x = n[0]) +1;
			area->u.Extent.g_h = n[3] - (area->u.Extent.g_y = n[1]) +1;
		}
	
	} else if (stricmp (shape, "circle") == 0) {
		WORD n[3]; /* xc, yc, r; */
		if (read_coords (coords, n, numberof(n)) == 3 && n[2]/*r*/ >= 1
		    && (area = malloc (sizeof(struct s_map_area))) != NULL) {
			area->Type = 'C';
			area->u.Extent.g_x = (area->u.Circ.Centre.p_x = n[0]) - n[2];
			area->u.Extent.g_y = (area->u.Circ.Centre.p_y = n[1]) - n[2];
			area->u.Extent.g_w =
			area->u.Extent.g_h = (area->u.Circ.Radius = n[2]) * 2 +1;
		}
	
	} else if (stricmp (shape, "poly") == 0) {
		WORD         n = 0;
		const char * p = coords;
		while (isdigit(*(p++))) {
			if (*p == ',') {
				p++;
				n++;
			} else if (!*p) {
				n++;
			}
		}
		if ((n /= 2) >= 3) {
			area = malloc (sizeof(struct s_map_area) + sizeof(PXY) * n);
		}
		if (area) {
			PXY *c = area->u.Poly.P;
			p = coords;
			area->Type = 'P';
			area->u.Poly.Count = 0;
			area->u.Extent.g_x = area->u.Extent.g_y = 0x7FFF;
			area->u.Extent.g_w = area->u.Extent.g_h = 0;
			while (n--) {
				int l;
				sscanf (p, "%hu,%hu,%n", &c->p_x, &c->p_y, &l);
				if (area->u.Extent.g_x > c->p_x) area->u.Extent.g_x = c->p_x;
				if (area->u.Extent.g_w < c->p_x) area->u.Extent.g_w = c->p_x;
				if (area->u.Extent.g_y > c->p_y) area->u.Extent.g_y = c->p_y;
				if (area->u.Extent.g_h < c->p_y) area->u.Extent.g_h = c->p_y;
				area->u.Poly.Count++;
				p += l;
				c++;
			}
			if (*(long*)(c -1) != *(long*)area->u.Poly.P) {
				*c = *area->u.Poly.P;
				area->u.Poly.Count++;
			}
			area->u.Extent.g_w -= area->u.Extent.g_x -1;
			area->u.Extent.g_h -= area->u.Extent.g_y -1;
		}
	}
	
	if (area) {
		area->Address = href;
		area->Target  = target;
		area->AltText = text;
		area->Next    = NULL;
	}
	
	return area;
}
