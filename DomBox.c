#include <string.h>
#include <stdio.h> /* debug */

#include "defs.h"


/*============================================================================*/
DOMBOX *
dombox_ctor (DOMBOX * This, DOMBOX * parent)
{
	memset (This, 0, sizeof (struct s_dombox));
	
	if (This == parent) {
		puts ("dombox_ctor(): This and parent are equal!");
		return This;
	}
	
	if (parent) {
		if (parent->ChildEnd) parent->ChildEnd->Sibling = This;
		else                  parent->ChildBeg          = This;
		parent->ChildEnd = This;
		This->Parent     = parent;
	}
	This->Backgnd = -1;
	
	return This;
}

/*============================================================================*/
DOMBOX *
dombox_dtor (DOMBOX * This)
{
	if (This->HtmlType < 0) {
		puts ("dombox_ctor(): This already destroyed!");
	}
	if (This->ChildBeg) {
		puts ("dombox_ctor(): This has still children!");
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
		This->HtmlType = -1;
	}
	return This;
}
