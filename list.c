#include <stdlib.h>

#include "global.h"
#include "token.h"
#include "scanner.h"
#include "fontbase.h"


/*============================================================================*/
void
list_start (TEXTBUFF current, BULLET bullet, short counter)
{
	LSTSTACK list = malloc (sizeof (struct list_stack_item));
	PARAGRPH par  = current->paragraph;
	
	list->FontStk = fontstack_push (current, -1);
	list->Hanging = current->word->font->SpaceWidth *5;
	
	if (!current->lst_stack) {
		par = add_paragraph (current, 2);
		list->Indent = par->Indent;
	
	} else if (par->paragraph_code != PAR_LI ||
	           current->lst_stack->Spacer->next_word != current->prev_wrd) {
		par = add_paragraph (current, 0);
		list->Indent = par->Indent - par->Hanging;
	
	} else { /* reuse actual (empty) <li> line */
		list->Indent = par->Indent - par->Hanging;
	}
	
	par->Box.BoxClass = BC_MIXED;
	par->Box.HtmlCode = TAG_LI;
	par->alignment    = ALN_LEFT;
	par->Hanging     -= list->Hanging;

	list->next_stack_item = current->lst_stack;
	current->lst_stack    = list;
	
	list->BulletStyle = bullet;
	list->Counter     = counter;
	list->Spacer      = current->word;
	
	if (bullet > LT_NONE) {
		*(current->text++) = font_Nobrk (current->word->font);
		new_word (current, TRUE);
		list->Spacer->word_width = list->Hanging;
	}
}

/*============================================================================*/
void
list_finish (TEXTBUFF current)
{
	PARAGRPH par   = current->paragraph;
	LSTSTACK list  = current->lst_stack;
	current->lst_stack = list->next_stack_item;
	
	if (list->FontStk !=  current->font) {
		fontstack_pop (current);
	}
	fontstack_pop (current);
	
	if (!current->lst_stack) {
		if (list->Spacer == current->prev_wrd && par->paragraph_code != PAR_LI) {
			current->prev_wrd->next_word = NULL;
			destroy_word_list (par->item, NULL);
			par->item         = current->word;
			current->prev_wrd = NULL;
		} else {
			par = add_paragraph (current, 2);
		}
	} else {
		if (list->Spacer == current->prev_wrd && par->paragraph_code != PAR_LI) {
			current->lst_stack->Spacer = list->Spacer;
			list->Indent -= -par->Hanging;
		} else {
			par = add_paragraph (current, 0);
		}
/* This one seems to be wrong:
		par->Hanging = -current->lst_stack->Hanging;
*/
	}
	par->Indent = list->Indent;
	
	free (list);
}


/*------------------------------------------------------------------------------
 * Recursively converts a positive long to a decimal number wide string.
 * Rainer Seitel, 2002-03-11
*/
static void long2decimal (char **p_ws, long l)
{
	if (l >= 10)
		long2decimal(p_ws, l / 10);

	*((*p_ws)++) = (char)((l % 10) + '0');
}


/*------------------------------------------------------------------------------
 * Recursively converts a positive long to an alphabet number wide string.
 * Rainer Seitel, 2002-03-11
*/
static void long2alpha (char **p_ws, long l, BOOL upper)
{
	if (l > 26)
		long2alpha(p_ws, l / 26, upper);

	*((*p_ws)++) = (char)(((l - 1) % 26) + (upper? 'A' : 'a'));
}


/*------------------------------------------------------------------------------
 * Sources:
 * Friedlein, Gottfried: Die Zahlzeichen und das elementare Rechnen
 * der Griechen und R”mer und des christlichen Abendlandes vom 7.
 * bis 13. Jahrhundert. - unver„nderter Neudruck der Ausgabe von
 * 1869. - Wiesbaden : Dr. Martin S„ndig oHG, 1968.
 * Ifroh, Georges: Universalgeschichte der Zahlen. - Frankfurt am
 * Main ; New York : Campus, 1986. - ISBN 3-593-33666-9. - Kapitel
 * 9. (Histoire Universelle des Chiffres. - Paris : Seghers, 1981.)
 * Rainer Seitel, 2002-03-11
*/
static void short2roman (char **p_ws, short count, BOOL upper)
{
	struct  { char M,  D,  C,  L,  X,  V,  I; }
		u_case = { 'M','D','C','L','X','V','I' },
		l_case = { 'm','d','c','l','x','v','i' },
		* letter = (upper ? &u_case : &l_case);
	char * text = *p_ws;
	
	if (count >= 10000) {
		*(text++) = '(';
		*(text++) = '(';
		*(text++) = '|';
		*(text++) = ')';
		*(text++) = ')';
	}
	switch ((count % 10000) / 1000 * 1000) {
	case 9000:
		*(text++) = '|';
		*(text++) = ')';
		*(text++) = ')';
		*(text++) = letter->M;
		*(text++) = letter->M;
		*(text++) = letter->M;
		*(text++) = letter->M;
		break;
	case 8000:
		*(text++) = '|';
		*(text++) = ')';
		*(text++) = ')';
		*(text++) = letter->M;
		*(text++) = letter->M;
		*(text++) = letter->M;
		break;
	case 7000:
		*(text++) = '|';
		*(text++) = ')';
		*(text++) = ')';
		*(text++) = letter->M;
		*(text++) = letter->M;
		break;
	case 6000:
		*(text++) = '|';
		*(text++) = ')';
		*(text++) = ')';
		*(text++) = letter->M;
		break;
	case 5000:
		*(text++) = letter->M;
	case 4000:
		*(text++) = letter->M;
	case 3000:
		*(text++) = letter->M;
	case 2000:
		*(text++) = letter->M;
	case 1000:
		*(text++) = letter->M;
		break;
	}
	switch ((count % 1000) / 100 * 100) {
	case 300:
		*(text++) = letter->C;
	case 200:
		*(text++) = letter->C;
	case 100:
		*(text++) = letter->C;
		break;
	case 400:
		*(text++) = letter->C;
	case 500:
		*(text++) = letter->D;
		break;
	case 600:
		*(text++) = letter->D;
		*(text++) = letter->C;
		break;
	case 700:
		*(text++) = letter->D;
		*(text++) = letter->C;
		*(text++) = letter->C;
		break;
	case 800:
		*(text++) = letter->D;
		*(text++) = letter->C;
		*(text++) = letter->C;
		*(text++) = letter->C;
		break;
	case 900:
		*(text++) = letter->C;
		*(text++) = letter->M;
		break;
	}
	switch ((count % 100) / 10 * 10) {
	case 30:
		*(text++) = letter->X;
	case 20:
		*(text++) = letter->X;
	case 10:
		*(text++) = letter->X;
		break;
	case 40:
		*(text++) = letter->X;
	case 50:
		*(text++) = letter->L;
		break;
	case 60:
		*(text++) = letter->L;
		*(text++) = letter->X;
		break;
	case 70:
		*(text++) = letter->L;
		*(text++) = letter->X;
		*(text++) = letter->X;
		break;
	case 80:
		*(text++) = letter->L;
		*(text++) = letter->X;
		*(text++) = letter->X;
		*(text++) = letter->X;
		break;
	case 90:
		*(text++) = letter->X;
		*(text++) = letter->C;
		break;
	}
	switch (count % 10) {
	case 3:
		*(text++) = letter->I;
	case 2:
		*(text++) = letter->I;
	case 1:
		*(text++) = letter->I;
		break;
	case 4:
		*(text++) = letter->I;
	case 5:
		*(text++) = letter->V;
		break;
	case 6:
		*(text++) = letter->V;
		*(text++) = letter->I;
		break;
	case 7:
		*(text++) = letter->V;
		*(text++) = letter->I;
		*(text++) = letter->I;
		break;
	case 8:
		*(text++) = letter->V;
		*(text++) = letter->I;
		*(text++) = letter->I;
		*(text++) = letter->I;
		break;
	case 9:
		*(text++) = letter->I;
		*(text++) = letter->X;
		break;
	}
	
	*p_ws = text;
}


/*============================================================================*/
void
list_marker (TEXTBUFF current, BULLET bullet, short counter)
{
	char buffer[80] = "", * text = buffer;
	WORD mapping    = current->word->font->Base->Mapping;
	LSTSTACK list   = current->lst_stack;
	PARAGRPH par    = current->paragraph;
	WORD     width  = list->Hanging;
	WORD     spc_wd = (current->word->font->SpaceWidth +1) /2;
	
	if (!(list->Spacer == current->prev_wrd && /* item is empty and...*/
	      (par->paragraph_code != PAR_LI ||    /* ...first item of the list  */
	       list->Spacer != par->item))) {      /* ...no nesting bullet befor */
		par = add_paragraph (current, 0);
		par->Box.BoxClass = BC_MIXED;
		par->Box.HtmlCode = TAG_LI;
		par->Indent  =  list->Indent;
		par->Hanging = -list->Hanging;
		
		list->Spacer = current->word;
		*(current->text++) = font_Nobrk (current->word->font);
		new_word (current, TRUE);
	}
	list->Spacer->word_width = current->word->font->SpaceWidth;
	par->paragraph_code = PAR_LI;
	
	switch (bullet)
	{
		case LT_DISC:
			current->text = unicode_to_wchar (0x2022,   /* BULLET */
			                                  current->text, mapping);
			break;
		case LT_SQUARE:
			current->text = unicode_to_wchar (0x25AA,   /* BLACK SMALL SQUARE */
			                                  current->text, mapping);
			break;
		case LT_CIRCLE:
			current->text = unicode_to_wchar (0x25E6,  /* WHITE BULLET */
			                                  current->text, mapping);
			break;
		
		case LT_DECIMAL:
			list->Counter = counter +1;
			if (counter < 0) {
				current->text = unicode_to_wchar (0x2212,  /* MINUS SIGN */
				                                  current->text, mapping);
				counter = -counter;
			}
			long2decimal (&text, counter);
			break;
		
		case LT_L_ALPHA:
			if (counter != 0) {
				list->Counter = counter +1;
				if (counter < 0) {
					current->text = unicode_to_wchar (0x2212,  /* MINUS SIGN */
					                                  current->text, mapping);
					counter = -counter;
				}
				long2alpha (&text, counter, FALSE);
				break;
			}
			/* else use @ as small alphabet zero */
		
		case LT_U_ALPHA:
			list->Counter = counter +1;
			if (counter < 0) {
				current->text = unicode_to_wchar (0x2212,  /* MINUS SIGN */
				                                  current->text, mapping);
				counter = -counter;
			}
			long2alpha (&text, counter, TRUE);
			break;
		
		case LT_L_ROMAN:
			list->Counter = counter +1;
			if (counter > 0 && counter < 3000) {
				short2roman (&text, counter, FALSE);
			} else {  /* BUG: small Roman numerals >= 3000 not implemented! */
				long2decimal (&text, counter);
			}
			break;
		
		case LT_U_ROMAN:
			list->Counter = counter +1;
			if (counter > 0 && counter < 20000) {
				short2roman (&text, counter, TRUE);
			} else {  /* BUG: Roman numerals >= 20000 not implemented! */
				long2decimal (&text, counter);
			}
			break;
		
		case LT_NONE:
			*(current->text++) = font_Nobrk (current->word->font);
			if (!list->BulletStyle) {
				spc_wd = 0;
				width  = current->prev_wrd->word_width *2;
			}
	}
	
	if (text > buffer) {
		*(text++) = '.';
		*(text)   = '\0';
		scan_string_to_16bit (buffer, ENCODING_ATARIST, &current->text, mapping);
	}
	new_word (current, TRUE);
	width -= (current->prev_wrd->word_width += spc_wd);
	list->Spacer->word_width = (spc_wd > width ? spc_wd : width);
}
