#include <string.h>
#include <stdio.h> /* debug */

#include "defs.h"


/*============================================================================*/
DOMBOX *
dombox_ctor (DOMBOX * This, DOMBOX * parent, BOXCLASS class)
{
	memset (This, 0, sizeof (struct s_dombox));
	
	if (This == parent) {
		puts ("dombox_ctor(): This and parent are equal!");
		return This;
	}
	
	if (parent) {
		if (!class) {
			puts ("dombox_ctor(): no class given.");
		}
		if (parent->ChildEnd) parent->ChildEnd->Sibling = This;
		else                  parent->ChildBeg          = This;
		parent->ChildEnd = This;
		This->Parent     = parent;
	} else {
		if (class) {
			puts ("dombox_ctor(): main got class.");
		}
	}
	This->BoxClass = class;
	This->Backgnd  = -1;
	
	return This;
}

/*============================================================================*/
DOMBOX *
dombox_dtor (DOMBOX * This)
{
	if ((int)This->BoxClass < 0) {
		puts ("dombox_ctor(): already destroyed!");
	}
	if (This->ChildBeg) {
		puts ("dombox_ctor(): has still children!");
	}
	if (This->Parent) {
		DOMBOX ** ptr = &This->Parent->ChildBeg;
		while (*ptr) {
			if (*ptr == This) {
				*ptr = This->Sibling;
				break;
			}
			ptr = &(*ptr)->Sibling;
		}
		This->Parent = NULL;
	}
	This->BoxClass = -1;
	
	return This;
}
