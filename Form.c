#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "token.h"
#include <gemx.h>

#ifdef __PUREC__
#define          FORM    FORM
typedef struct s_form  * FORM;
#define          INPUT   INPUT
typedef struct s_input * INPUT;
#endif

#include "global.h"
#include "fontbase.h"
#include "scanner.h"
#include "Loader.h"
#include "parser.h"
#include "Form.h"


struct s_form {
	FRAME       Frame;
	FORM        Next;
	char      * Target;
	char      * Action;
	INPUT       InputList, Last;
	FORM_METHOD Method;
};

typedef enum {
	IT_HIDDN = 1,
	IT_GROUP,  /* node for a radio button group */
	IT_RADIO,
	IT_CHECK,
	IT_BUTTN,
	IT_TEXT
} INP_TYPE;

struct s_input {
	INPUT    Next;
	union {
		void * Void;
		INPUT  Group; /* for radio buttons this points to the group node */
	}        u;
	WORDITEM Word;
	PARAGRPH Paragraph;
	FORM     Form;
	char   * Value;
	INP_TYPE Type;
	char     SubType; /* [S]ubmit, [R]eset, \0 */
	BOOL     checked;
	BOOL     disabled;
	UWORD    TextMax;
	char     Name[1];
};


/*============================================================================*/
void *
new_form (FRAME frame, char * target, char * action, const char * method)
{
	FORM form = malloc (sizeof (struct s_form));
	
	form->Frame     = frame;
	form->Next      = frame->FormList;
	frame->FormList = form;
	
	form->Target = target;
	form->Action = action;
	form->Method = (method && stricmp (method, "post") == 0
	                ? METH_POST : METH_GET);
	form->InputList = NULL;
	form->Last      = NULL;
	
	return form;
}


/*----------------------------------------------------------------------------*/
static INPUT
_alloc (INP_TYPE type, TEXTBUFF current, const char * name)
{
	FORM   form  = current->form;
	size_t n_len = (name && *name ? strlen (name) : 0);
	INPUT  input = malloc (sizeof (struct s_input) + n_len);
	input->Next      = NULL;
	input->u.Void    = NULL;
	input->Paragraph = current->paragraph;
	input->Form      = form;
	input->Value     = NULL;
	input->Type      = type;
	input->SubType   = '\0';
	input->disabled  = FALSE;
	input->TextMax   = 0;
	
	if (type <= IT_GROUP) {
		input->Word    = NULL;
		input->checked = (type == IT_HIDDN);
	} else {
		input->Word    = current->word;
		input->checked = (type >= IT_TEXT);
		current->word->input = input;
	}
	
	if (n_len) {
		memcpy (input->Name, name, n_len);
	}
	input->Name[n_len] = '\0';
	
	if (type != IT_RADIO) {
		if (form->Last) {
			form->Last->Next = input;
		} else {
			form->InputList  = input;
		}
		form->Last          = input;
	}
	return input;
}

/*----------------------------------------------------------------------------*/
static void
set_word (TEXTBUFF current, WORD asc, WORD dsc, WORD wid)
{
	WORDITEM word = current->word;
	
	if (current->text == current->buffer) {
		*(current->text++) = font_Nobrk(current->word->font);
	}
	new_word (current, TRUE);
	if (wid <= 0) {
		wid = word->word_width - wid;
	}
	word->word_width     = wid +4;
	word->word_height    = asc +2;
	word->word_tail_drop = dsc +2;
}

/*============================================================================*/
INPUT
form_check (TEXTBUFF current, const char * name, char * value, BOOL checked)
{
	WORD  asc = current->word->word_height - (current->word->word_tail_drop +2);
	INPUT input = _alloc (IT_CHECK, current, name);
	
	input->Value   = value;
	input->checked = checked;
	if (asc < 2) asc = 2;
	set_word (current, asc, -1, asc -1);
	
	return input;
}

/*============================================================================*/
INPUT
form_radio (TEXTBUFF current,
            const char * name, const char * value, BOOL checked)
{
	WORD  asc   = current->word->word_height - current->word->word_tail_drop;
	FORM  form  = current->form;
	INPUT input = _alloc (IT_RADIO, current, value);
	INPUT group = form->InputList;
	
	while (group) {
		if (group->Type == IT_GROUP && strcmp (group->Name, name) == 0) break;
		group = group->Next;
	}
	if (!group) {
		group = _alloc (IT_GROUP, current, name);
	}
	if (checked && !group->checked) {
		group->Value   = input->Name;
		group->checked = TRUE;
		input->checked = TRUE;
	}
	input->u.Group = group;
	input->Next    = group->u.Group;
	group->u.Group = input;
	
	if (asc < 1) asc =  1;
	else         asc |= 1;
	set_word (current, asc, 0, asc);
	
	return input;
}

/*============================================================================*/
INPUT
form_buttn (TEXTBUFF current, const char * name, const char * value,
            ENCODING encoding, char sub_type)
{
	WORDITEM word  = current->word;
	TEXTATTR attr  = word->attr;
	INPUT    input = _alloc (IT_BUTTN, current, name);
	
	input->SubType = sub_type;
	input->Value   = strdup (value);
	
	font_byType (-1, -1, -1, word);
	scan_string_to_16bit (value, encoding, &current->text,
	                      word->font->Base->Mapping);
	if (current->text == current->buffer) {
		*(current->text++) = font_Nobrk (word->font);
	}
	set_word (current, word->word_height, word->word_tail_drop +1, -4);
	current->word->attr = attr;
	
	return input;
}

/*============================================================================*/
INPUT
form_text (TEXTBUFF current, const char * name, char * value,
          UWORD maxlen, ENCODING encoding, UWORD cols)
{
	WORDITEM word  = current->word;
	TEXTATTR attr  = word->attr;
	WCHAR  * wmark = current->buffer + cols;
	INPUT    input = _alloc (IT_TEXT, current, name);
	
	input->Value   = value;
	input->TextMax = maxlen;
	
	font_byType (pre_font, -1, -1, word);
	scan_string_to_16bit (value, encoding, &current->text,
	                      word->font->Base->Mapping);
	if (current->text >= wmark) {
		current->text = wmark;
	} else {
		WCHAR nobrk = font_Nobrk (word->font);
		do {
			*(current->text++) = nobrk;
		} while (current->text < wmark);
	}
	set_word (current, word->word_height, word->word_tail_drop, -2);
	current->word->attr = attr;
	
	return input;
}

/*============================================================================*/
INPUT
new_input (PARSER parser)
{
	INPUT    input   = NULL;
	FRAME    frame   = parser->Frame;
	TEXTBUFF current = &parser->Current;
	char output[100], name[100];
	const char * val;
	
	if (!current->form) {
		current->form = new_form (frame, NULL, NULL, NULL);
	}
	
	get_value (parser, KEY_NAME, name, sizeof(name));
	
	if (!get_value (parser, KEY_TYPE, output, sizeof(output))
	    || stricmp (output, val = "TEXT")     == 0
	    || stricmp (output, val = "FILE")     == 0
	    || stricmp (output, val = "PASSWORD") == 0) {
		char * value;
		UWORD  mlen = get_value_unum (parser, KEY_MAXLENGTH, 0);
		UWORD  cols = get_value_unum (parser, KEY_SIZE, 0);
		if (!cols) cols = 20;
		if (!mlen) mlen = cols;
		get_value (parser, KEY_VALUE, value = malloc (mlen +1), mlen +1);
		input = form_text (current, name, value, mlen, frame->Encoding, cols);
		
	} else if (stricmp (output, "HIDDEN") == 0) {
		input = _alloc (IT_HIDDN, current, name);
		input->Value = get_value_str (parser, KEY_VALUE);
		
	} else if (stricmp (output, "RADIO") == 0) {
		get_value (parser, KEY_VALUE, output, sizeof(output));
		input = form_radio (current, name, output,
		                    get_value (parser, KEY_CHECKED, NULL,0));
		
	} else if (stricmp (output, "CHECKBOX") == 0) {
		input = form_check (current, name, get_value_str (parser, KEY_VALUE),
		                    get_value (parser, KEY_CHECKED, NULL,0));
		
	} else if (stricmp (output, val = "Submit") == 0 ||
	           stricmp (output, val = "Reset")  == 0 ||
	           stricmp (output, val = "Button") == 0) {
		char sub_type = (*val == 'B' ? '\0' : *val);
		if (get_value (parser, KEY_VALUE, output, sizeof(output))) {
			char * p = output;
			while (isspace (*p)) p++;
			if (*p) {
				p = strchr (val = p, '\0');
				while (isspace (*(--p))) *p = '\0';
			}
		}
		input = form_buttn (current, name, val, frame->Encoding, sub_type);
		
	} else if (stricmp (output, "debug") == 0) {
		FORM  form = current->form;
		INPUT inp  = form->InputList;
		printf ("%s: %s?", (form->Method == METH_POST ? "POST" : "GET"),
		        (form->Action ? form->Action : "<->"));
		while (inp) {
			if (inp->checked) {
				printf ("%s=%s&", inp->Name, (inp->Value ? inp->Value : ""));
			}
			inp = inp->Next;
		}
		printf ("\n");
	}
	
	if (input && get_value (parser, KEY_DISABLED, NULL,0)) {
		input->disabled = TRUE;
	}
	
	return input;
}


/*============================================================================*/
void
input_draw (INPUT input, WORD x, WORD y)
{
	WORDITEM word = input->Word;
	short c_lu, c_rd;
	PXY p[5];
	p[2].p_x = (p[0].p_x = x) + word->word_width -1;
	p[1].p_y = y - word->word_height;
	p[3].p_y = y + word->word_tail_drop -1;
	
	if (input->Type >= IT_TEXT) {
		vsf_color (vdi_handle, G_WHITE);
		vsl_color (vdi_handle, G_LBLACK);
		c_lu = G_BLACK;
		c_rd = G_LWHITE;
	} else if (input->checked) {
		vsf_color (vdi_handle, G_LBLACK);
		vsl_color (vdi_handle, G_BLACK);
		c_lu = G_BLACK;
		c_rd = G_WHITE;
	} else {
		vsf_color (vdi_handle, G_LWHITE);
		vsl_color (vdi_handle, G_BLACK);
		c_lu = G_WHITE;
		c_rd = G_LBLACK;
	}
	if (input->Type == IT_RADIO) {
		p[1].p_x = p[3].p_x = (p[0].p_x + p[2].p_x) /2;
		p[0].p_y = p[2].p_y = (p[1].p_y + p[3].p_y) /2;
	} else {
		p[0].p_y = p[3].p_y;
		p[1].p_x = p[0].p_x;
		p[2].p_y = p[1].p_y;
		p[3].p_x = p[2].p_x;
	}
	p[4]     = p[0];
	v_fillarea (vdi_handle, 4, (short*)p);
	v_pline    (vdi_handle, 5, (short*)p);
	if (input->Type == IT_RADIO) {
		vsl_color (vdi_handle, c_lu);
		p[0].p_x++;
		p[1].p_y++; 
		v_pline (vdi_handle, 2, (short*)p);
		p[0].p_x++;
		p[1].p_y++; 
		v_pline (vdi_handle, 2, (short*)p);
		vsl_color (vdi_handle, c_rd);
		p[1].p_x++;   p[2].p_x -= 2;   p[4].p_x = p[0].p_x +1;
		p[1].p_y++;   p[3].p_y -= 2;   p[4].p_y = p[0].p_y +1;
		v_pline (vdi_handle, 4, (short*)(p +1));
		p[2].p_x++;
		p[3].p_y++; 
		v_pline (vdi_handle, 2, (short*)(p +2));
	
	} else {
		vsl_color (vdi_handle, c_lu);
		p[1].p_x = ++p[0].p_x;   p[2].p_x -= 2;
		p[1].p_y = ++p[2].p_y;   p[0].p_y -= 2;
		v_pline (vdi_handle, 3, (short*)p);
		if (input->Type == IT_BUTTN) {
			p[1].p_x = ++p[0].p_x;  --p[2].p_x;
			p[1].p_y = ++p[2].p_y;  --p[0].p_y;
			v_pline (vdi_handle, 3, (short*)p);
			vsl_color (vdi_handle, c_rd);
			p[0].p_x++;   p[1].p_x = ++p[2].p_x;
			p[2].p_y++;   p[1].p_y = ++p[0].p_y;
			v_pline (vdi_handle, 3, (short*)p);
			p[0].p_x--;
			p[2].p_y--;
		} else {
			vsl_color (vdi_handle, c_rd);
			p[0].p_x++;
			p[2].p_y++;
		}
		p[1].p_x = ++p[2].p_x;
		p[1].p_y = ++p[0].p_y;
		v_pline (vdi_handle, 3, (short*)p);
	}
	if (input->Type >= IT_BUTTN) {
		font_switch (word->font, NULL);
		v_ftext16 (vdi_handle,
		           x + (input->Type == IT_BUTTN ? 4 : 3), y, word->item);
	}
	if (input->disabled) {
		p[1].p_x = (p[0].p_x = x) + word->word_width -1;
		p[0].p_y = y - word->word_height;
		p[1].p_y = y + word->word_tail_drop -1;
		vsf_interior (vdi_handle, FIS_PATTERN);
		vsf_style    (vdi_handle, 4);
		vsf_color    (vdi_handle, G_LWHITE);
		v_bar (vdi_handle, (short*)p);
		vsf_interior (vdi_handle, FIS_SOLID);
	}
}


/*============================================================================*/
void
input_disable (INPUT input, BOOL onNoff)
{
	input->disabled = onNoff;
}


/*============================================================================*/
BOOL
input_isEdit (INPUT input)
{
	return (input->Type >= IT_TEXT);
}


/*============================================================================*/
WORD
input_handle (INPUT input, GRECT * radio)
{
	WORD rtn;
	
	if (input->disabled) {
		rtn = 0;
	
	} else switch (input->Type) {
		case IT_RADIO:
			if (input->checked) {
				rtn = 0;
			} else {
				INPUT group = input->u.Group;
				rtn = 1;				
				if (group->checked) {
					INPUT check = group->u.Group;
					do if (check->checked) {
						WORDITEM c_w = check->Word, i_w = input->Word;
						radio->g_x = c_w->h_offset
						           - i_w->h_offset;
						radio->g_y = (c_w->line->OffsetY - c_w->word_height)
						           - (i_w->line->OffsetY - i_w->word_height);
						radio->g_w = c_w->word_width;
						radio->g_h = c_w->word_height + c_w->word_tail_drop;
						if (check->Paragraph != input->Paragraph) {
							OFFSET * offset = &check->Paragraph->Offset;
							long     x      = offset->X + check->Paragraph->Indent;
							long     y      = offset->Y;
							while ((offset = offset->Origin) != NULL) {
								x += offset->X;
								y += offset->Y;
							}
							offset = &input->Paragraph->Offset;
							x     -= offset->X + input->Paragraph->Indent;
							y     -= offset->Y;
							while ((offset = offset->Origin) != NULL) {
								x -= offset->X;
								y -= offset->Y;
							}
							radio->g_x += x;
							radio->g_y += y;
						}
						check->checked = FALSE;
						rtn = 2;
						break;
					} while ((check = check->Next) != NULL);
				} else {
					group->checked = TRUE;
				}
				group->Value = input->Name;
				input->checked = TRUE;
			}
			break;
		case IT_CHECK:
			input->checked = !input->checked;
			rtn = 1;
			break;
		case IT_BUTTN:
			if (!input->checked) {
				input->checked = TRUE;
				rtn = 1;
				break;
			}
		default:
			rtn = 0;
	}
	return rtn;
}


/*============================================================================*/
BOOL
input_activate (INPUT input)
{
	FORM form = input->Form;
	
	if (input->Type != IT_BUTTN) {
		return FALSE;
	}
	
	if (input->SubType == 'S' && form->Action) {
		INPUT  elem = form->InputList;
		size_t size = 0, len;
		char * url;
		
		if (elem) {
			do if (elem->checked && *elem->Name) {
				size += 2 + strlen (elem->Name)
				      + (elem->Value ? strlen (elem->Value) : 0);
			} while ((elem = elem->Next) != NULL);
		}
		
		len = strlen (form->Action);
		url = strcpy (malloc (len + size +1), form->Action);
		if (size) {
			char * p = url + len;
			size += len;
			elem = form->InputList;
			*(p++) = '?';
			do if (elem->checked && *elem->Name) {
				len = strlen (elem->Name);
				memcpy (p, elem->Name, len);
				p += len;
				*(p++) = '=';
				if (elem->Value) {
					len = strlen (elem->Value);
					memcpy (p, elem->Value, len);
					p += len;
				}
				*(p++) = '&';
			} while ((elem = elem->Next) != NULL);
			len = size;
		}
		url[len] = '\0';
		new_loader_job (url, form->Frame->Location, form->Frame->Container);
		free (url);
	}
	
	input->checked = FALSE;
	
	return TRUE;
}
