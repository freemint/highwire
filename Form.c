/*
 ** Form handler
 **
 ** Changes
 ** Author         Date           Desription
 ** P Slegg        14-Aug-2009    Utilise NKCC from cflib to handle the various control keys in text fields.
 **
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <cflib.h>

#ifndef __PUREC__
#  include <osbind.h>
#else
#  include <tos.h>
#endif

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
#include "Containr.h"
#include "Loader.h"
#include "Location.h"
#include "parser.h"
#include "Form.h"
#include "hwWind.h"

typedef struct s_select   * SELECT;
typedef struct s_slctitem * SLCTITEM;

struct s_form {
	FRAME       Frame;
	FORM        Next;
	char      * Target;
	char      * Action;
	char      * Enctype;
	INPUT       TextActive;
	WORD        TextCursrX;
	WORD        TextCursrY;
	WORD        TextShiftX;
	WORD        TextShiftY;
	INPUT       InputList, Last;
	FORM_METHOD Method;
};

typedef enum {
	IT_HIDDN = 1,
	IT_GROUP,  /* node for a radio button group */
	IT_RADIO,
	IT_CHECK,
	IT_BUTTN,
	IT_SELECT,
	IT_TEXT,
	IT_TAREA,
	IT_FILE
} INP_TYPE;

struct s_input {
	INPUT	Next;
	union {
		void * Void;
		INPUT  Group;  /* radio button: points to the group node             */
		INPUT  FileEd; /* file upload button: points to its text input field */
		SELECT Select; /* selection menue                                    */
	}        u;
	WORDITEM Word;
	PARAGRPH Paragraph;
	FORM     Form;
	char   * Value;
	INP_TYPE Type;
	char     SubType;  /* [S]ubmit, [R]eset, [F]ile, \0 */
	BOOL     checked;
	BOOL     disabled;
	BOOL     readonly;
	UWORD    VisibleX;
	UWORD    VisibleY;
	short    CursorH;
	UWORD    TextMax;  /* either 0 for infinite long or restricted to n char */
	UWORD    TextRows;
	WCHAR ** TextArray;
	char     Name[1];
};

struct s_select {
	SLCTITEM ItemList;
	WCHAR  * SaveWchr;
	WORD     NumItems;
	char   * Array[1];
};
struct s_slctitem {
	SLCTITEM Next;
	char   * Value;
	WORD     Width;
	WORD     Length;
	WCHAR    Text[80];
	char     Strng[82];
};


static void finish_selct (INPUT);

static WCHAR * edit_init (INPUT, TEXTBUFF, UWORD cols, UWORD rows, size_t size);
static BOOL    edit_zero (INPUT);
static void    edit_feed (INPUT, ENCODING, const char * beg, const char * end);
static BOOL    edit_crlf (INPUT, WORD col, WORD row);
static BOOL    edit_char (INPUT, WORD col, WORD row, WORD chr);
static BOOL    edit_delc (INPUT, WORD col, WORD row);
static void form_activate_multipart (FORM form);
#define        __edit_len(inp,a,b)   ((inp)->TextArray[b]-(inp)->TextArray[a]-1)
#define        edit_rowln(inp,row)   __edit_len (inp, row, row +1)
#define        __edit_r_0(inp)       ((inp)->TextArray[0])
#define        __edit_r_r(inp)       ((inp)->TextArray[(inp)->TextRows])
#define        edit_space(inp)       ((WCHAR*)&__edit_r_0(inp)-__edit_r_r(inp))

/*------------------------------------------------------------------------------
 * Memo: UTF-8 ranges
00-7F : 1 byte
	( 0bbbaaaa = 7 bits )
80-1FFF   : 2 bytes
	( 10dccccb 0bbbaaaa = 13 bits)
2000-3FFFF : 3 bytes
	( 110eeddd 10dccccb 0bbbaaaa = 18 bits)
40000-7FFFFF : 4 bytes
	( 1110fffe 10eeeddd 10dccccb 0bbbaaaa = 23 bits)
800000-FFFFFFF : 5 bytes
	( 11110ggg 10gffffe 10eeeddd 10dccccb 0bbbaaaa = 28 bits)
10000000-1FFFFFFFF : 6 bytes
	( 111110ih 10hhhggg 10gffffe 10eeeddd 10dccccb 0bbbaaaa = 33 bits)
*/
/* here's a little helper for multipart POST
 only supporting unicode values on 16bits, thus 3 bytes utf8 sequence */
static char *
unicode_to_utf8 (WCHAR *src)
{
	int len = 0;
	char *ret;
	char *outptr;
	char out;
	WCHAR *inptr = src;
	WCHAR in;
	if (src == NULL)
	{
		return NULL;
	}
	while((in = *inptr++) != 0)
	{
		if (in < 128)	{ len++; }
		else if (in < 0x1fff) { len += 2; }
		else { len += 3; }
	}
	ret = (char*) malloc(len+1);
	if (ret != NULL)
	{
		outptr = ret;
		inptr = src;
		while((in = *inptr++) != 0)
		{
			if (in < 128)
			{
				*outptr++ = (char)in;
			}
			else if (in < 0x1fff)
			{
				out = (char)((in >> 7) & 0x3f) | 0x80;
				*outptr++ = out;
				out = (char)(in & 0x7f);
				*outptr++ = out;
			}
			else
			{
				out = (char)((in >> 13) & 0x07) | 0xc0;
				*outptr++ = out;
				out = (char)((in >> 7) & 0x3f) | 0x80;
				*outptr++ = out;
				out = (char)(in & 0x7f);
				*outptr++ = out;
			}
		}
		*outptr = 0;
	}
	return ret;
}


/*============================================================================*/
void *
new_form (FRAME frame, char * target, char * action, const char * method, char *enctype)
{
	char *ptr;
	FORM form = malloc (sizeof (struct s_form));

	if (!action) {
		const char * q = strchr (frame->BaseHref->FullName, '?');
		size_t       n = (q ? q - frame->BaseHref->FullName
		                    : strlen (frame->BaseHref->FullName));
		if ((action = malloc (n +1)) != NULL) {
			strncpy (action, frame->BaseHref->FullName, n);
			action[n] = '\0';
		}
	}

	form->Frame     = frame;
	form->Next      = frame->FormList;
	frame->FormList = form;

	/* clean up action */
	if (action != NULL)
	{
		ptr = strstr(action,"&amp;");
		while(ptr != NULL)
		{
			ptr++;
			strcpy(ptr,ptr+4);
			ptr = strstr(ptr,"&amp;");
		}
	}

	form->Target = target;
	form->Action = action;
	form->Method = (method && strcmp  (method, "AUTH") == 0 ? METH_AUTH :
	                method && stricmp (method, "post") == 0 ? METH_POST :
	                                                          METH_GET);
	if (enctype != NULL)
	{
		form->Enctype = enctype;
	}
	else
	{
		form->Enctype = strdup("application/x-www-form-urlencoded");
	}
	form->TextActive = NULL;
	form->TextCursrX = 0;
	form->TextCursrY = 0;
	form->TextShiftX = 0;
	form->TextShiftY = 0;
	form->InputList = NULL;
	form->Last      = NULL;

	return form;
}

/*============================================================================*/
void
form_finish (TEXTBUFF current)
{
	INPUT input = (current->form ? ((FORM)current->form)->InputList : NULL);
	while (input) {
		if (input->Type == IT_SELECT) {
			finish_selct (input);
		}
		input = input->Next;
	}
	current->form = NULL;
}

/*============================================================================*/
void
destroy_form (FORM form, BOOL all)
{
	while (form) {
		FORM  next  = form->Next;
		INPUT input = form->InputList;
		while (input) {
			INPUT inxt = input->Next;
			if (input->Type == IT_SELECT) {
				SELECT sel = input->u.Select;
				if (sel) {
					while (sel->ItemList) {
						SLCTITEM item = sel->ItemList;
						sel->ItemList = item->Next;
						if (item->Value && item->Value != item->Strng +1) {
							free (item->Value);
						}
						free (item);
					}
					input->Value      = NULL;
					input->Word->item = sel->SaveWchr;
					free (sel);
				}
			}
			if (input->Type != IT_GROUP && input->Value) {
				free (input->Value);
			}
			if (input->Word) {
				input->Word->input = NULL;
			}
			free (input);
			input = inxt;
		}
		if (form->Enctype) {
			free (form->Enctype);
		}
		if (form->Target) {
			free (form->Target);
		}
		if (form->Action) {
			free (form->Action);
		}
		free (form);
		if (!all) break;
		else      form = next;
	}
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
	input->readonly  = TRUE;
	input->VisibleX  = 0;
	input->VisibleY  = 0;
	input->TextMax   = 0;
	input->TextRows  = 0;
	input->TextArray = NULL;

	if (type <= IT_GROUP) {
		input->Word    = NULL;
		input->checked = (type == IT_HIDDN);
	} else {
		input->Word    = current->word;
		input->checked = (type >= IT_SELECT/*IT_TEXT*/);
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
	word->space_width    = 0;
	TA_Color(word->attr) = G_BLACK;
}

/*============================================================================*/
INPUT
form_check (TEXTBUFF current, const char * name, char * value, BOOL checked)
{
	WORD  asc = current->word->word_height - (current->word->word_tail_drop +2);
	INPUT input = _alloc (IT_CHECK, current, name);

	if (value) {
	input->Value   = value;
	} else {
		char *val = malloc (3);
		if (val) memcpy (val, "on", 3);
		input->Value = val;
	}
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

	if (sub_type == 'I') {
		FRAME frame          = ((FORM)current->form)->Frame;
		short word_height    = current->word->word_height;
		short word_tail_drop = current->word->word_tail_drop;
		short word_v_align   = current->word->vertical_align;

		input->SubType = 'S';

		new_image (frame, current, value, frame->BaseHref, 0,0, 0,0,FALSE);
		font_switch (current->word->font, NULL);

		new_word (current, TRUE);
		current->word->word_height    = word_height;
		current->word->word_tail_drop = word_tail_drop;
		current->word->vertical_align = word_v_align;

	} else {
		input->SubType = sub_type;
		input->Value   = strdup (value);

		font_byType (-1, -1, -1, word);
		scan_string_to_16bit (value, encoding, &current->text,
		                      word->font->Base->Mapping);
		if (current->text == current->buffer) {
			*(current->text++) = font_Nobrk (word->font);
		}
		set_word (current, word->word_height, word->word_tail_drop +1, -4);
	}
	current->word->attr = attr;

	return input;
}

/*============================================================================*/
INPUT
form_text (TEXTBUFF current, const char * name, char * value, UWORD maxlen,
          ENCODING encoding, UWORD cols, BOOL readonly, BOOL is_pwd)
{
	INPUT  input = _alloc (IT_TEXT, current, name);
	size_t v_len = (value  ? strlen (value) : 0);
	size_t size  = (maxlen ? maxlen : v_len);

	input->TextMax  = maxlen;
	input->readonly = readonly;

	if (edit_init (input, current, cols, 1, size)) {
		if (value && v_len < maxlen) {
			char * mem = malloc (maxlen +1);
			if (mem) memcpy (mem, value, v_len +1);
			free (value);
			value = mem;
		} else if (!value && is_pwd) {
			value = malloc (maxlen +1);
		}
		if (is_pwd) { /* == "PASSWORD" */
			input->Value = value;
		}
		if (value) {
			edit_feed (input, encoding, value, value + v_len);
			if (value != input->Value) {
				free (value);
			}
		}
	} else {
		input->Value = value;
	}

	return input;
}

/*============================================================================*/
INPUT
new_input (PARSER parser, WORD width)
{
	INPUT    input   = NULL;
	FRAME    frame   = parser->Frame;
	TEXTBUFF current = &parser->Current;
	const char * val = "T"; /* default type is TEXT */
	char output[100], name[100];

	if (!current->form) {
		current->form = new_form (frame, NULL, NULL, NULL, NULL);
	}

	get_value (parser, KEY_NAME, name, sizeof(name));

	if (!get_value (parser, KEY_TYPE, output, sizeof(output))
	    || stricmp (output, val = "TEXT")     == 0
	    || stricmp (output, val = "FILE")     == 0
	    || stricmp (output, val = "PASSWORD") == 0) {
		UWORD  mlen = get_value_unum (parser, KEY_MAXLENGTH, 0);
		UWORD  cols = get_value_unum (parser, KEY_SIZE, 0);
		if (!mlen) {
			if (cols > 500)  cols = 500;
		} else {
			if (mlen > 500)  mlen = 500;
			if (cols > mlen) cols = mlen;
		}

		if (*val == 'F') {
			INPUT bttn = form_buttn (current, name, "...", frame->Encoding, 'F');

			input = form_text (current, name,
					   	get_value_exists (parser, KEY_READONLY) ? get_value_str (parser, KEY_VALUE) : strdup(""),
		                   mlen, frame->Encoding, (cols ? cols : 20),
		                   1, (*val == 'P'));

			/* Add the browse button */
			bttn->u.FileEd = input;
			input->Type = IT_FILE;
			if (width > 0) {
				width = (width > bttn->Word->word_width
				         ? width - bttn->Word->word_width : 1);
			}
		} else {
			input = form_text (current, name, get_value_str (parser, KEY_VALUE),
		                   mlen, frame->Encoding, (cols ? cols : 20),
		                   get_value_exists (parser, KEY_READONLY),
		                   (*val == 'P'));
		}

		if (width > 0) {
			WORD cw = (input->Word->word_width -1 -4) / input->VisibleX;
			input->Word->word_width = max (width, input->Word->word_height *2);
			input->VisibleX = (input->Word->word_width -1 -4) / cw;
		}
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
	           stricmp (output, val = "Button") == 0 ||
	           stricmp (output, val = "Image")  == 0) {
		char sub_type = (*val == 'B' ? '\0' : *val);

		if (sub_type == 'I') {
			if (get_value (parser, KEY_SRC, output, sizeof(output))) {
				val = output;
			} else {
				sub_type = 'S';
			}
		} else if (get_value (parser, KEY_VALUE, output, sizeof(output))) {
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
		printf ("%s: %s%s", (form->Method == METH_POST ? "POST" : "GET"),
		        (form->Action ? form->Action : "<->"),
		        (strchr (form->Action, '?') ? "&" : "?"));
		while (inp) {
			if (inp->checked) {
				printf ("%s=%s&", inp->Name, (inp->Value ? inp->Value : ""));
			}
			inp = inp->Next;
		}
		printf ("\n");
	}

	if (input && get_value (parser, KEY_DISABLED, NULL,0)) {
		input_disable (input, TRUE);
	}

	return input;
}

/*============================================================================*/
INPUT
new_tarea (PARSER parser, const char * beg, const char * end, UWORD nlines)
{
	INPUT    input   = NULL;
	FRAME    frame   = parser->Frame;
	TEXTBUFF current = &parser->Current;

	size_t size = (end - beg) + nlines * (sizeof(WCHAR*) / sizeof(WCHAR));
	UWORD  rows = get_value_unum (parser, KEY_ROWS, 0);
	UWORD  cols = get_value_unum (parser, KEY_COLS, 0);
	char   name[100];

	if (!current->form) {
		current->form = new_form (frame, NULL, NULL, NULL, NULL);
	}
	get_value (parser, KEY_NAME, name, sizeof(name));
	input = _alloc (IT_TAREA, current, name);

	input->disabled = get_value_exists (parser, KEY_DISABLED);
	input->readonly = get_value_exists (parser, KEY_READONLY);

	if (!rows) rows = 5;
	if (!cols) cols = 40;

	if (edit_init (input, current, cols, rows, size)) {
		edit_feed (input, frame->Encoding, beg, end);
	}

	return input;
}


/*============================================================================*/
INPUT
form_selct (TEXTBUFF current, const char * name, UWORD size, BOOL disabled)
{
	WORDITEM word = current->word;
	WORD     asc  = word->word_height -1;
	WORD     dsc  = word->word_tail_drop;
	INPUT    input;

	(void)size;

	if (!current->form /*&&
		 (current->form = new_form (frame, NULL, NULL, NULL)) == NULL*/) {
		input = NULL;

	} else if ((input = _alloc (IT_SELECT, current, name)) != NULL) {
		SELECT sel = malloc (sizeof(struct s_select));
		if ((input->u.Select = sel) != NULL) {
			sel->ItemList = NULL;
			sel->NumItems = 0;
			sel->Array[0] = NULL;
		} else {
			disabled = TRUE;
		}
		input->disabled = disabled;
		set_word (current, asc, dsc, asc + dsc);
		sel->SaveWchr = input->Word->item;
	}

	return input;
}

/*============================================================================*/
INPUT
selct_option (TEXTBUFF current, const char * text,
              BOOL disabled, ENCODING encoding, char * value, BOOL selected)
{
	INPUT    input = (current->form ? ((FORM)current->form)->Last : NULL);
	SELECT   sel;
	SLCTITEM item;

	if (((!input || input->Type != IT_SELECT) &&
	     (input = form_selct (current, "", 1, FALSE)) == NULL) ||
	    (sel = input->u.Select) == NULL) {
		return NULL;
	}

	if (*text && (item = malloc (sizeof(struct s_slctitem))) != NULL) {
		size_t tlen = strlen (text);
		WORD  pts[8];
		if ((text[0] == '-' && (!text[1] || (text[1] == '-' && (!text[2] ||
		    (text[2] == '-' && (!text[3] || (text[3] == '-'))))))) ||
		    (tlen > 3 && strncmp (text + tlen -3, "---", 3) == 0)) {
			item->Strng[0] = '-';
			disabled = TRUE;
		} else if (disabled) {
			item->Strng[0] = '!';
		} else {
			item->Strng[0] = ' ';
		}
		if (tlen >= sizeof(item->Strng) -1) {
			tlen = sizeof(item->Strng) -2;
		}
		memcpy (item->Strng +1, text, tlen);
		item->Strng[tlen +1] = '\0';

		encoding = ENCODING_ATARIST;
		current->text = current->buffer;
		scan_string_to_16bit (text, encoding, &current->text,
		                      current->word->font->Base->Mapping);
		tlen = current->text - current->buffer;
		if (tlen >= numberof(item->Text)) {
			tlen = numberof(item->Text) -1;
		}
		current->text = current->buffer;
		memcpy (item->Text, current->buffer, tlen *2);
		item->Text[tlen] = '\0';
		item->Length = tlen;
		vqt_f_extent16n (vdi_handle, item->Text, item->Length, pts);
		item->Width = pts[2] - pts[0];

		item->Value = (disabled ? NULL : value ? value : item->Strng +1);

		if (!sel->ItemList || selected) {
			input->Word->item   = item->Text;
			input->Word->length = item->Length;
			input->Value        = item->Value;
		}
		item->Next    = sel->ItemList;
		sel->ItemList = item;
		sel->NumItems--;
	}

	return input;
}

/*============================================================================*/
void
selct_finish (TEXTBUFF current)
{
	INPUT input = (current->form ? ((FORM)current->form)->Last : NULL);
	if (input && input->Type == IT_SELECT) {
		finish_selct (input);
	}
}

/*----------------------------------------------------------------------------*/
static void
finish_selct (INPUT input)
{
	SELECT sel = input->u.Select;
	WORD   num;

	if (!sel || !sel->NumItems) {
		input->disabled = TRUE;

	} else if ((num = -sel->NumItems) > 0) {
		SELECT rdy = malloc (sizeof (struct s_select) + num * sizeof(char*));
		if (rdy) {
			SLCTITEM item = sel->ItemList;
			short    wdth = 0;
			rdy->ItemList = sel->ItemList;
			rdy->SaveWchr = sel->SaveWchr;
			rdy->NumItems = num;
			rdy->Array[0] = rdy->Array[num] = NULL;
			do {
				if (wdth < item->Width) {
					wdth = item->Width;
				}
				rdy->Array[--num] = item->Strng;
			} while ((item = item->Next) != NULL && num);
			input->Word->word_width += wdth +2;
			free (sel);
			input->u.Select = rdy;
		}
	}
}


/*============================================================================*/
void
input_draw (INPUT input, WORD x, WORD y)
{
	WORDITEM word = input->Word;
	short c_lu, c_rd;
	PXY p[6];
	p[2].p_x = (p[0].p_x = x) + word->word_width -1;
	p[1].p_y = y - word->word_height;
	p[3].p_y = y + word->word_tail_drop -1;

	if (input->Type >= IT_TEXT) {
		vsf_color (vdi_handle, G_WHITE);
		vsl_color (vdi_handle, G_LBLACK);
		c_lu = G_BLACK;
		c_rd = G_LWHITE;
	} else if (input->checked && input->Type != IT_SELECT) {
		vsf_color (vdi_handle, G_LBLACK);
		vsl_color (vdi_handle, G_BLACK);
		c_lu = G_BLACK;
		c_rd = G_WHITE;
	} else {
		vsf_color (vdi_handle, (ignore_colours ? G_WHITE : G_LWHITE));
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
		} else if (input->Type == IT_SELECT) {
			short w;
			p[1].p_x = p[2].p_x; /* save this */
			p[3].p_y = p[4].p_y = p[2].p_y +3;
			p[2].p_y            = p[0].p_y -2;
			w = (p[2].p_y - p[3].p_y +1) & ~1;
			p[4].p_x = p[2].p_x -2;
			p[3].p_x = p[4].p_x - w;
			p[2].p_x = p[3].p_x + w /2;
			p[5]     = p[2];
			if (ignore_colours) {
				vsl_color (vdi_handle, c_rd);
				v_pline (vdi_handle, 4, (short*)(p +2));
			} else {
				v_pline (vdi_handle, 3, (short*)(p +2));
				vsl_color (vdi_handle, c_rd);
				v_pline (vdi_handle, 2, (short*)(p +4));
			}
			p[2] = p[1];
			p[0].p_x++;
			p[2].p_y++;
		} else {
			vsl_color (vdi_handle, c_rd);
			p[0].p_x++;
			p[2].p_y++;
		}
		p[1].p_x = ++p[2].p_x;
		p[1].p_y = ++p[0].p_y;
		v_pline (vdi_handle, 3, (short*)p);
	}
	if (input->Type >= IT_TEXT) {
		BOOL     fmap = (word->font->Base->Mapping != MAP_UNICODE);
		FORM     form = input->Form;
		WCHAR ** wptr = input->TextArray;
		short    rows = min (input->TextRows, input->VisibleY);
		WORD     shft;
		PXY      pos;
		pos.p_x = x + 3;
		pos.p_y = y - input->CursorH * input->VisibleY;
		if (input == form->TextActive) {
			shft =  form->TextShiftX;
			wptr += form->TextShiftY;
		} else {
			shft = 0;
		}
		if (fmap) {
			vst_map_mode (vdi_handle, MAP_UNICODE);
		}
		while (rows--) {
			WCHAR * ptr = wptr[0] + shft;
			WORD    len = min (wptr[1] -1 - ptr, input->VisibleX);
			pos.p_y += input->CursorH;
			if (len > 0) v_ftext16n (vdi_handle, pos, ptr, len);
			wptr++;
		}
		if (input == form->TextActive) {
			WCHAR * ptr = input->TextArray[form->TextShiftY]   + shft;
			vqt_f_extent16n (vdi_handle, ptr, form->TextCursrX - shft, (short*)p);
			p[0].p_x = x +2 + p[1].p_x - p[0].p_x;
			p[1].p_x = p[0].p_x +1;
			p[0].p_y = y - word->word_height +2
			         + (form->TextCursrY - form->TextShiftY) * input->CursorH;
			p[1].p_y = p[0].p_y                              + input->CursorH;
			vsf_color (vdi_handle, G_WHITE);
			vswr_mode (vdi_handle, MD_XOR);
			v_bar     (vdi_handle, (short*)p);
			vswr_mode (vdi_handle, MD_TRANS);
		}
		if (fmap) {
			vst_map_mode (vdi_handle, word->font->Base->Mapping);
		}
	} else if (input->Type >= IT_BUTTN) {
		v_ftext16 (vdi_handle,
		           x + (input->Type == IT_BUTTN ? 4 : 3), y, word->item);
	}
	if (input->disabled) {
		p[1].p_x = (p[0].p_x = x) + word->word_width -1;
		p[0].p_y = y - word->word_height;
		p[1].p_y = y + word->word_tail_drop -1;
		vsf_interior (vdi_handle, FIS_PATTERN);
		if (ignore_colours) {
			vsf_style (vdi_handle, 3);
			vsf_color (vdi_handle, G_WHITE);
		} else {
			vsf_style (vdi_handle, 4);
			vsf_color (vdi_handle, G_LWHITE);
		}
		v_bar (vdi_handle, (short*)p);
		vsf_interior (vdi_handle, FIS_SOLID);
	}
}


/*============================================================================*/
void
input_disable (INPUT input, BOOL onNoff)
{
	input->disabled = onNoff;
	/* check for file uploade element to set its correspondind part also */
	if (input->Type == IT_BUTTN) {
		INPUT field = (input->SubType == 'F' ? input->u.FileEd : NULL);
		if (field && field->Next == input) {
			field->disabled = onNoff;
		}
	} else if (input->Type == IT_TEXT) {
		INPUT buttn = (input->Next->Type == IT_BUTTN ? input->Next : NULL);
		if (buttn->SubType == 'F' && buttn->u.FileEd == input) {
			buttn->disabled = onNoff;
		}
	}
}


/*============================================================================*/
BOOL
input_isEdit (INPUT input)
{
	return (input->Type >= IT_TEXT);
}


/*----------------------------------------------------------------------------*/
static void
coord_diff (INPUT check, INPUT input,  GRECT * rect)
{
	WORDITEM c_w = check->Word, i_w = input->Word;
	rect->g_x = c_w->h_offset
	           - i_w->h_offset;
	rect->g_y = (c_w->line->OffsetY - c_w->word_height)
	           - (i_w->line->OffsetY - i_w->word_height);
	rect->g_w = c_w->word_width;
	rect->g_h = c_w->word_height + c_w->word_tail_drop;
	if (check->Paragraph != input->Paragraph) {
		long c_x, c_y, i_x, i_y;
		dombox_Offset (&check->Paragraph->Box, &c_x, &c_y);
		dombox_Offset (&input->Paragraph->Box, &i_x, &i_y);
		rect->g_x += c_x - i_x;
		rect->g_y += c_y - i_y;
	}
}

/*============================================================================*/
WORD
input_handle (INPUT input, PXY mxy, GRECT * radio, char *** popup)
{
	WORD rtn = 0;

	if (!input->disabled) switch (input->Type) {

		case IT_RADIO:
			if (!input->checked) {
				INPUT group = input->u.Group;
				rtn = 1;
				if (group->checked) {
					INPUT check = group->u.Group;
					do if (check->checked) {
						coord_diff (check, input, radio);
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

		case IT_FILE:
		case IT_TAREA:
		case IT_TEXT: {
			FORM form = input->Form;
			if (form->TextActive && form->TextActive != input) {
				coord_diff (form->TextActive, input, radio);
				rtn = 2;
			} else {
				rtn = 1;
			}
			form->TextActive = input;
			form->TextShiftX = 0;
			form->TextShiftY = 0;
			if (mxy.p_y > 0 && input->TextRows > 1) {
				form->TextCursrY = mxy.p_y / input->CursorH;
				if (form->TextCursrY > input->VisibleY) {
					 form->TextCursrY = input->VisibleY;
				}
				if (form->TextCursrY > input->TextRows -1) {
					 form->TextCursrY = input->TextRows -1;
				}
			} else {
				form->TextCursrY = 0;
			}
			if (mxy.p_x > 0 && edit_rowln (input, form->TextCursrY) > 0) {
				form->TextCursrX = mxy.p_x / (input->Word->font->SpaceWidth -1);
				if (form->TextCursrX > input->VisibleX) {
					 form->TextCursrX = input->VisibleX;
				}
				if (form->TextCursrX > edit_rowln (input, form->TextCursrY)) {
					 form->TextCursrX = edit_rowln (input, form->TextCursrY);
				}
			} else {
				form->TextCursrX = 0;
			}
		}	break;

		case IT_CHECK:
			input->checked = !input->checked;
			rtn = 1;
			break;

		case IT_SELECT: {
			SELECT sel = input->u.Select;
			if (sel && sel->NumItems > 0) {
				WORDITEM word = input->Word;
				long   * x    = &((long*)radio)[0];
				long   * y    = &((long*)radio)[1];
				dombox_Offset (&input->Paragraph->Box, x, y);
				*x += word->h_offset;
				*y += word->line->OffsetY + word->word_tail_drop -1;
				*popup = sel->Array;
				rtn = 1;
			}
		}	break;

		case IT_BUTTN:
			if (!input->checked) {
				input->checked = TRUE;
				rtn = 1;
			}
			break;

		default: ;
	}
	return rtn;
}


/*----------------------------------------------------------------------------*/
static char *
base64enc (const char * src, long len)
{
	char * mem = (src && len > 0 ? malloc (((len +2) /3) *4 +1) : NULL);
	if (mem) {
		const char trans[] =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		char * dst = mem;
		do {
			if (len >= 3) {
				long val = ((((long)src[0] <<8) | src[1]) <<8) | src[2];
				dst[3] = trans[val & 0x3F]; val >>= 6;
				dst[2] = trans[val & 0x3F]; val >>= 6;
				dst[1] = trans[val & 0x3F]; val >>= 6;
				dst[0] = trans[val & 0x3F];
			} else if (len == 2) {
				long val = (((long)src[0] <<8) | src[1]) <<2;
				dst[3] = '=';
				dst[2] = trans[val & 0x3F]; val >>= 6;
				dst[1] = trans[val & 0x3F]; val >>= 6;
				dst[0] = trans[val & 0x3F];
			} else /* len == 1 */ {
				long val = (long)src[0] <<4;
				dst[3] = '=';
				dst[2] = '=';
				dst[1] = trans[val & 0x3F]; val >>= 6;
				dst[0] = trans[val & 0x3F];
			}
			src += 3;
			dst += 4;
		} while ((len -= 3) > 0);
		dst[0] = '\0';
	}
	return mem;
}

/*----------------------------------------------------------------------------*/
static void
form_activate (FORM form)
{
	FRAME    frame = form->Frame;
	INPUT    elem  = form->InputList;
	LOCATION loc   = frame->Location;
	LOADER   ldr   = NULL;
	size_t	size  = 0;
	size_t	len;
	char 	*data;
	char	*url;

	if (form->Method == METH_AUTH) { /* special case, internal created *
	                                  * for HTTP Authentication        */
		const char * realm = (frame->AuthRealm && *frame->AuthRealm
		                      ? frame->AuthRealm : NULL);
		char buf[100], * p = buf;
		ldr = start_page_load (frame->Container, NULL,loc, TRUE, NULL);
		if (!ldr) {
			realm = NULL;
		}
		if (realm) {
			WCHAR * beg, * end;
			if (elem && elem->TextArray) {
				beg = elem->TextArray[0];
				end = elem->TextArray[1] -1;
			} else {
				beg = end = NULL;
			}
			if (beg < end) {
				do {
					*(p++)= *(beg++);
				} while (beg < end);
				*(p++) = ':';
				*(p)   = '\0';
				elem = elem->Next;
			} else {
				realm = NULL;
			}
		}
		if (realm) {
			if (elem && elem->Value) {
				strcpy (p, elem->Value);
				elem = elem->Next;
			} else {
				realm = NULL;
			}
		}

		if (realm && elem && !elem->Next
		          && elem->Type == IT_BUTTN && elem->SubType == 'S') {
			ldr->AuthRealm = strdup (realm);
			ldr->AuthBasic = base64enc (buf, strlen(buf));
		}

		return;
	}

	if (elem)
	{
		int nbvar = 0;
		int nbfile = 0;
		/* pre-check list of inputs for a type IT_FILE */
		do
		{
			if (*elem->Name && elem->checked)
			{
				nbvar++;
				if (elem->Type == IT_FILE)
				{
					nbfile++;
				}
			}
			elem = elem->Next;
		} while(elem != NULL);
		if (nbfile > 0)
		{
			/* PUT only possible if single file and single item */
			if (form->Method == METH_PUT && ((nbvar > 1) || (nbfile > 1)))
			{
				form->Method = METH_POST;
			}
			if (form->Method == METH_POST)
			{
				/* if file input used, switch to "multipart/form-data" enc */
				form_activate_multipart(form);
				return;
			}
		}
		if (form->Enctype && stricmp(form->Enctype, "multipart/form-data")==0)
		{
			form->Method = METH_POST;
			form_activate_multipart(form);
			return;
		}
		/* go back to 1st input */
		elem = form->InputList;

		do if (elem->checked && *elem->Name) {
			size += 2 + strlen (elem->Name);
			if (elem->Value) {
				char * v = elem->Value, c;
				while ((c = *(v++)) != '\0') {
					size += (c == ' ' || isalnum (c) ? 1 : 3);
				}
			} else if (elem->TextArray) {
				WCHAR * beg = elem->TextArray[0];
				WCHAR * end = elem->TextArray[elem->TextRows] -1;
				while (beg < end) {
					char c = *(beg++);
					size += (c == ' ' || isalnum (c) ? 1 : 3);
				}
			}
		} while ((elem = elem->Next) != NULL);
	}

	if (form->Method == METH_POST) {
		len  = 0;
		url  = form->Action;
		data = malloc (size +1);
		if (size) size--;
	} else {
		len  = strlen (form->Action);
		url  = strcpy (malloc (len + size +1), form->Action);
		data = url;
		data[len] = (strchr (url, '?') ? '&' : '?');
	}
	if (size) {
		char * p = data + (len > 0 ? len +1 : 0);
		size += len;
		elem = form->InputList;
		do if (elem->checked && *elem->Name) {
			char * q;
			len = strlen (elem->Name);
			*p = '\0';
			if ((q = strstr (data, elem->Name)) != NULL) {
				char * f = (q == data || q[-1] == '?' || q[-1] == '&'
				            ? q + len : NULL);
				do if (f && (!*f || *f == '&' || *f == '=')) {
					if (*f == '=') while (*(++f) && *f != '&');
					if (*f == '&') f++;
					while ((*(q++) = *(f++)) != '\0');
					size -= f - q;
					p    -= f - q;
					break;
				} else if ((q = strstr (q +1, elem->Name)) != NULL) {
					f = (q[-1] == '&' ? q + len : NULL);
				} while (q);
			}
			memcpy (p, elem->Name, len);
			p += len;
			*(p++) = '=';
			if (elem->Value) {
				char * v = elem->Value, c;
				while ((c = *(v++)) != '\0') {
					if (c == ' ') {
						*(p++) = '+';
					} else if (isalnum (c)) {
						*(p++) = c;
					} else {
						p += sprintf (p, "%%%02X", (int)c);
					}
				}
			} else if (elem->TextArray) {
				WCHAR * beg = elem->TextArray[0];
				WCHAR * end = elem->TextArray[elem->TextRows] -1;
				while (beg < end) {
					char c = *(beg++);
					if (c == ' ') {
						*(p++) = '+';
					} else if (isalnum (c)) {
						*(p++) = c;
					} else {
						p += sprintf (p, "%%%02X", (int)c);
					}
				}
			}
			*(p++) = '&';
		} while ((elem = elem->Next) != NULL);
		len = size;
	}
	data[len] = '\0';

	if (form->Method != METH_POST) {
		CONTAINR target = NULL;
		CONTAINR cont = NULL;

		if (form->Target && stricmp(form->Target, "_hw_top") == 0) {
			HwWIND this = hwWind_byType (0);

			if (this != NULL) {
				hwWind_raise (this, TRUE);

				cont = this->Pane;
				target = containr_byName (cont, "_top");
			} else {
				target = NULL;
			}
		} else {
			target = (form->Target &&
		                   stricmp (form->Target, "_blank") != 0
		                   ? containr_byName (frame->Container, form->Target) : NULL);
		}

		if (target) {
			cont = target;
		} else {
			cont = frame->Container;
		}

		ldr = start_page_load (cont, url,loc, TRUE, NULL);
		free (url);
	} else {
		POSTDATA post = new_post(data, strlen(data), strdup("application/x-www-form-urlencoded"));
		if (post)
		{
			ldr = start_page_load (frame->Container, url,loc, TRUE, post);
			if (!ldr) delete_post (post);
		}
		/* TODO: add an else with an error message for user */
	}
	if (ldr) {
		if (location_equalHost (loc, ldr->Location)
	       && frame->AuthRealm && frame->AuthBasic) {
			ldr->AuthRealm = strdup (frame->AuthRealm);
			ldr->AuthBasic = strdup (frame->AuthBasic);
		}
		ldr->Referer = location_share (loc);
	}
}

/*============================================================================*/
static void
form_activate_multipart (FORM form)
{
	FRAME  frame = form->Frame;
	INPUT  elem  = form->InputList;
	LOADER   ldr   = NULL;
	POSTDATA post  = NULL;
	FILE *current = NULL;
	size_t size  = 0;
	size_t len;
	size_t boundlen;
	size_t flen;
	char *data;
	char *url;
	char boundary[39];
	char minihex[16] = "0123456789ABCDEF";
	ULONG randomized;
	WORD i;
	char *ptr;
	char *cnvstr;
	char *atari;
	WCHAR *wptr;
	char *type;

	/* build our own multipart boundary for this submitting */
	randomized = (ULONG)Random();	/* Xbios(17) -> 24bit random value */
	memset(boundary, '-', 38);
	boundary[38] = 0;
	for (i=0; i<6; i++)
	{
		boundary[32+i] = minihex[randomized & 0x0F];
		randomized >>= 4;
	}
	boundary[30] = 'H';
	boundary[31] = 'W';
	boundlen = 38;

	/* multipart posting is MIME-like formatted */
	/* it's a list of bodies containing the value of each variable */
	/* contents are not encoded, and charset of texts should be same as page */
	i = 0;
	do
	{
		if (elem->checked && *elem->Name)
		{
			if ((elem->Type == IT_FILE) && (elem->TextArray[0]))
			{
				cnvstr = unicode_to_utf8(elem->TextArray[0]);
				if (cnvstr)
				{
					if (elem->Value) free(elem->Value);
					elem->Value = cnvstr;
				}
				/* quick hack to get filename */
				len = 0;
				wptr = elem->TextArray[0];
				while(*wptr++) len++;
				atari = malloc(len+1);
				len = 0;
				wptr = elem->TextArray[0];
				while(*wptr) { atari[len] = (char)(*wptr++); len++; }
				atari[len] = 0;
				/* "--" + boundary + CRLF */
				size += 2 + boundlen + 2;
				/* "Content-Disposition: form-data; name=\"" + Name + "\"" */
				size += 38 + strlen(elem->Name) + 1;
				/* "; filename=\"" + value + "\"" + CRLF */
				size += 12 + strlen(elem->Value) + 1 + 2;
				/* guess content-type, use "application/octet-stream" (24) now... */
				/* then add line "Content-Type: " (14) + strlen(cnttype) + CRLF + CRLF */
				size += 14 + 24 + 2 + 2;
				/* then add file content (raw) + CRLF */
				current = fopen(atari, "rb");
				flen = 0;
				if (current)
				{
					fseek (current, 0, SEEK_END);
					flen = ftell(current);
					fclose(current);
				}
				size += flen + 2;
				free(atari);
			}
			else
			{
				/* "--" + boundary + CRLF */
				size += 2 + boundlen + 2;
				/* "Content-Disposition: form-data; name=\"" + Name + "\"" */
				size += 38 + strlen(elem->Name) + 1;
				/* CRLF + CRLF + value + CRLF */
				size += 4;
				/* body is the value if not a file */
				if (elem->Value)
				{
					size += strlen(elem->Value);
				}
				else if (elem->TextArray)
				{
					WCHAR * beg = elem->TextArray[0];
					WCHAR * end = elem->TextArray[elem->TextRows] -1;
					size += (end-beg);	/* the - gives a number of entry, not byte */
					/*while (beg < end) {
						char c = *(beg++);
						size += (c == ' ' || isalnum (c) ? 1 : 3);
					}*/
				}
				size += 2;
			}
		}
		elem = elem->Next;
	}
	while (elem != NULL);
	/* "--" + boundary + "--" */
	size += 2 + boundlen + 2;
	/* end of post data size computing */

	/* now malloc the buffer */
	len  = 0;
	url  = form->Action;
	data = malloc (size +1);
	if (data == 0)
	{
		/* bad luck, cannot post because not enough ram */
		return;
	}

	/* now fill that post buffer */
	elem = form->InputList;
	ptr = data;
	do
	{
		if (elem->checked && *elem->Name)
		{
			/* "--" + boundary + CRLF */
			sprintf(ptr, "--%s\r\n", boundary);
			ptr += 2 + boundlen + 2;
			/* "Content-Disposition: form-data; name=\"" + Name + "\"" */
			sprintf(ptr, "Content-Disposition: form-data; name=\"%s\"", elem->Name);
			ptr += 38 + strlen(elem->Name) + 1;
			if ((elem->Type == IT_FILE) && (elem->Value))
			{
				/* "; filename=\"" + value + "\"" + CRLF */
				sprintf(ptr, "; filename=\"%s\"\r\n", elem->Value);
				ptr += 12 + strlen(elem->Value) + 1 + 2;
				/* guess content-type, use "application/octet-stream" (24) now... */
				/* then add line "Content-Type: " (14) + strlen(cnttype) + CRLF + CRLF */
				sprintf(ptr, "Content-Type: application/octet-stream\r\n\r\n");
				ptr += 14 + 24 + 2 + 2;
				/* then add file content (raw) + CRLF */
				current = fopen(elem->Value, "rb");
				/* quick hack to get filename */
				len = 0;
				wptr = elem->TextArray[0];
				while(*wptr++) len++;
				atari = malloc(len+1);
				len = 0;
				wptr = elem->TextArray[0];
				while(*wptr) { atari[len] = (char)(*wptr++); len++; }
				atari[len] = 0;
				if (current)
				{
					fseek (current, 0, SEEK_END);
					flen = ftell(current);
					fseek (current, 0, SEEK_SET);
					fread(ptr, 1, flen, current);
					fclose(current);
					ptr += flen;
				}
				*ptr++ = '\r';
				*ptr++ = '\n';
			}
			else
			{
				/* CRLF + CRLF + value + CRLF */
				sprintf(ptr,"\r\n\r\n");
				ptr += 4;
				/* body is the value if not a file */
				if (elem->Value)
				{
					strcpy(ptr, elem->Value);
					ptr += strlen(elem->Value);
				}
				else if (elem->TextArray)
				{
					WCHAR * beg = elem->TextArray[0];
					WCHAR * end = elem->TextArray[elem->TextRows] -1;
					char c;
					while (beg < end) {
						c = (char)*beg++;
						*ptr++ = c;
					}
				}
				*ptr++ = '\r';
				*ptr++ = '\n';
			}
		}
		elem = elem->Next;
	}
	while (elem != NULL);
	/* "--" + boundary + "--" */
	sprintf(ptr, "--%s--", boundary);
	ptr += 4 + boundlen;
	size = ptr - data;
	/* new send request with Content-Length: %ld\r\nContent-Type: multipart/form-data; boundary=%s\r\n */
	form->Method = METH_POST; /* with file, nothing else possible */

	type = malloc(50+strlen(boundary));
	if (type)
	{
		sprintf(type, "multipart/form-data; boundary=%s", boundary);
	}
	post = new_post(data, size, type);
	ldr = start_page_load (frame->Container, url, frame->Location, TRUE, post);
	if (!ldr)
	{
		/* post already deleted in start_page_load */
	}
	return;
}
/*============================================================================*/
/* A routine that could be broken into a wrapper for 2 or 3 small util routines */
static void
input_file_handler (INPUT input) {
	char fsel_file[HW_PATH_MAX] = "";
	FORM   form = input->Form;
	FRAME frame = form->Frame;
	HwWIND wind = hwWind_byContainr(frame->Container);

	if (file_selector ("HighWire: Select File to Upload", NULL,
	                   fsel_file, fsel_file,sizeof(fsel_file))) {
		INPUT field = input->u.FileEd;

		form->TextActive = field;

		edit_feed (field, frame->Encoding, fsel_file, strchr(fsel_file, '\0'));

		/* to be overworked, probably the same scheme as for radio buttons */
		{
			WORDITEM word = field->Word;
			GRECT clip;
			long  x, y;
			clip.g_x = 0;
			clip.g_w = word->word_width;
			clip.g_y = -word->word_height;
			clip.g_h = word->word_height + word->word_tail_drop;

			dombox_Offset (&word->line->Paragraph->Box, &x, &y);
			x += (long)clip.g_x + word->h_offset
			     + frame->clip.g_x - frame->h_bar.scroll;
			y += (long)clip.g_y + word->line->OffsetY
			     + frame->clip.g_y - frame->v_bar.scroll;

			clip.g_x = x;
			clip.g_y = y;
			hwWind_redraw (wind, &clip);
		}
	}
}

/*============================================================================*/
WORDITEM
input_activate (INPUT input, WORD slct)
{
	FORM form = input->Form;

	if (input->Type >= IT_TEXT) {
		WORDITEM word;
		if (slct >= 0) {
			word = NULL;
		} else {
			word = input->Word;
			if (input == form->TextActive) form->TextActive = NULL;
		}
		return word;
	}

	if (input->Type == IT_SELECT) {
		if (slct >= 0) {
			SELECT   sel  = input->u.Select;
			SLCTITEM item = sel->ItemList;
			while (++slct < sel->NumItems && item->Next) {
				item = item->Next;
			}
			input->Word->item   = item->Text;
			input->Word->length = item->Length;
			input->Value        = item->Value;
		}
		return input->Word;
	}

	if (input->Type != IT_BUTTN) {
		return NULL;
	}

	if (input->SubType == 'F') {
		input_file_handler (input);
	}

	if (input->SubType == 'S' && form->Action) {
		form_activate (form);
	}

	input->checked = FALSE;

	return input->Word;
}


/*============================================================================*/
WORDITEM
input_keybrd (INPUT input, WORD key, UWORD state, GRECT * rect, INPUT * next)
{
	FORM     form = input->Form;
	WORDITEM word = input->Word;
	WCHAR ** text = input->TextArray;
	WORD     ascii_code;
	WORD     scrl = 0;
	WORD     lift = 0;
	WORD     line = 0;
	unsigned short nkey;
/**	WORD     start;
	WORD     end;
	CHAR     strChunk[];
**/
	BOOL     shift, ctrl, alt;

  /* Convert the GEM key code to the "standard" */
  nkey = gem_to_norm ((short)state, (short)key);

  /* Remove the unwanted flags */
  nkey &= ~(NKF_RESVD|NKF_SHIFT|NKF_CTRL|NKF_CAPS);

	ascii_code =  nkey & 0x00FF;

	shift = (state & (K_RSHIFT|K_LSHIFT)) != 0;
	ctrl  = (state & K_CTRL) != 0;
	alt   = (state & K_ALT) != 0;

	if (input != (*next = form->TextActive))
	{
		return NULL;   /* shouldn't happen but who knows... */
	}

  if (!(nkey & NKF_FUNC))
	{
		if (!input->readonly
		    && edit_char (input, form->TextCursrX, form->TextCursrY, ascii_code))
		{
			scrl = +1;
			line = 1;
		}
		else
		{
			word = NULL;
		}
	}
	else
	{
    nkey &= ~NKF_FUNC;

	switch (nkey)
	{
    case NK_UP:
     	if (form->TextCursrY)
			{
				lift = -1;
				line = +2;
			}
			else
			{
				word = NULL;
			}
			break;

		case NK_DOWN:
			if (form->TextCursrY < input->TextRows -1)
			{
				lift = +1;
				line = -2;
			}
			else
			{
				word = NULL;
			}
			break;

		case NK_LEFT:  /* cursor-left */
		  if (shift)  /* cursor-left shifted: 52 */
		  {
			scrl = -form->TextCursrX;
			if (scrl >= 0)
			{
				word = NULL;
			}
			else
			{
				line = 1;
			}
		  }
#if 0
		  else if (ctrl)
		  {
        /* move cursor one word to the left */
        start = edit_rowln (input, form->TextCursrY-1) + 1;
        end = edit_rowln (input, form->TextCursrY) - form->TextCursrX;
        strChunk = strncpy(input, start, end-start);
        position = strrchr(strChunk, ' ');
        if (position == NULL)
          scrl = 0;
        else
          scrl = position  - thisLine + 1;
		  }
#endif
		  else  /* cursor-left unshifted */
		  {
		    if (form->TextCursrX)
			  {
				  scrl = -1;
				  line = 1;
			  }
			  else if (form->TextCursrY)
			  {
				  scrl = +edit_rowln (input, form->TextCursrY -1);
				  lift = -1;
				  line = +2;
			  }
			  else
			  {
				  word = NULL;
			  }
		  }
		  break;

		case NK_RIGHT:  /*cursor-right */
		  if (shift)  /* cursor-right shifted: 54 */
		  {
			  scrl = +edit_rowln (input, form->TextCursrY) - form->TextCursrX;
			  if (scrl <= 0)
			  {
				  word = NULL;
			  }
			  else
			  {
				  line = 1;
			  }
		  }
#if 0
      else if (ctrl)  /* cursor-right control */
      {
			  /* move cursor one word to the right */
			  start = edit_rowln(input, form->TextCursrY) - form->TextCursrX;
			  end = edit_rowln (input, form->TextCursrY);

        strChunk = strncpy(input, start, end-start);
        position = strrchr(strChunk, ' ');
			  if (position == NULL)
				  scrl = 0;
			  else
				  scrl = position  - thisLine + 1;
		  }

#endif
		  else  /* cursor-right unshifted */
		  {
		  	if (form->TextCursrX < edit_rowln (input, form->TextCursrY))
			  {
				  scrl = +1;
				  line = 1;
			  }
			  else if (form->TextCursrY < input->TextRows -1)
			  {
				  scrl = -form->TextCursrX;
				  lift = +1;
				  line = -2;
			  }
			  else
			  {
				  word = NULL;
			  }
		  }
		  break;

		case NK_ESC:  /* Escape:  27 */
		{
			WCHAR  * last = text[input->TextRows];
			if (!input->readonly && text[0] < last -1 && edit_zero (input))
			{
				form->TextCursrX = 0;
				form->TextCursrY = 0;
				form->TextShiftX = 0;
				form->TextShiftY = 0;
			}
			else
			{
				word = NULL;
			}
		}	break;

		case NK_RET:  /* Enter/Return: 13 */
		case (NK_ENTER|NKF_NUM):
			if (input->Type == IT_TAREA)
			{
				if (edit_crlf (input, form->TextCursrX, form->TextCursrY))
				{
					scrl = -form->TextCursrX;
					lift = +1;
				}
				else
				{
					word = NULL;
				}
			}
			else if (edit_rowln (input, form->TextCursrY) && (key & 0xFF00))
			{
				form_activate (form);
				form->TextActive = NULL;
			}
			else
			{
				word = NULL;
			}
			break;

		case NK_CLRHOME:  /* Clr/Home: 55 */
		  if (shift)  /* Clr-Home shifted */
		  {
			scrl = edit_rowln (input, input->TextRows -1) - form->TextCursrX;
			lift =                    input->TextRows -1  - form->TextCursrY;
			if (!scrl && !lift)
			{
				word = NULL;
			}
		  }
		  else  /* Clr-Home unshifted */
		  {
		  	if (form->TextCursrX || form->TextCursrY)
			  {
				  scrl = -form->TextCursrX;
				  lift = -form->TextCursrY;
			  }
			  else
			  {
				  word = NULL;
			  }
		  }
		  break;

		case NK_M_END:  /* Mac END key  */
		  scrl = edit_rowln (input, input->TextRows -1) - form->TextCursrX;
		  lift =                    input->TextRows -1  - form->TextCursrY;
		  if (!scrl && !lift)
		  {
			word = NULL;
		  }
		  break;


		case NK_TAB:  /* Tab: 9 */
			if (ctrl)  /* Tabe control */
			{
				form->TextActive = NULL;
				*next            = NULL;
			}
			else if (shift)
			{
				INPUT srch = form->InputList;
				INPUT last = NULL;

				if (srch)
				{
					while (1)
					{
						while ((srch->disabled) && (srch = srch->Next) != NULL) ;

						if (srch->Next == input)
						{
							if (srch->Type == IT_TEXT)
							{
								last = srch;
							}
							break;
						}
						else
						{
							if (srch->Type == IT_TEXT)
							{
								last = srch;
							}
							srch = srch->Next;
						}
					}
				}

				if (!last)
				{
					srch = input->Next;

					while ((srch = srch->Next) != NULL)
					{
						if (!srch->disabled && srch->Type == IT_TEXT)
						{
							last = srch;
						}
					}
				}

				/* on google it would move out of the form */
				if (!last) last = input;

				*next = last;
			}
			else
			{
				INPUT srch = input->Next;
				if (srch)
				{
					while ((srch->disabled || srch->Type < IT_TEXT)
					       && (srch = srch->Next) != NULL);
				}
				if (!srch)
				{
					srch = form->InputList;
					while ((srch->disabled || srch->Type < IT_TEXT)
					       && (srch = srch->Next) != NULL);
				}

				*next = srch;
			}
			break;

		case NK_BS:  /* Backspace: 8 */
			if (input->readonly)
			{
				word = NULL;
			}
			else if (form->TextCursrX)
			{
				edit_delc (input, form->TextCursrX -1, form->TextCursrY);
				scrl = -1;
				line = 1;
			}
			else if (form->TextCursrY)
			{
				WORD col = edit_rowln (input, form->TextCursrY -1);
				edit_delc (input, col, form->TextCursrY -1);
				scrl = col;
				lift = -1;
			}
			else
			{
				word = NULL;
			}
			break;

		case NK_DEL:  /* Delete: 127 */
			if (input->readonly)
			{
				word = NULL;
			}
			else
			{
				if (shift)  /* Delete shifted */
				{
				  printf ("X=%d edit_rowln=%ld\n\r",form->TextCursrX, edit_rowln(input,form->TextCursrY));
					if (form->TextCursrX < edit_rowln (input, form->TextCursrY))
					{
					  printf ("delete character to right of cursor\n\r");
						edit_delc (input, form->TextCursrX, form->TextCursrY);
						line = 1;
					}
					else if (!edit_delc (input, form->TextCursrX, form->TextCursrY))
					{
					  printf ("end of line reached so merge lines\n\r");
						word = NULL;
					}
				}
				else
				{
					if (form->TextCursrX < edit_rowln (input, form->TextCursrY))
					{
						edit_delc (input, form->TextCursrX, form->TextCursrY);
						line = 1;
					}
					else if (!edit_delc (input, form->TextCursrX, form->TextCursrY))
					{
						word = NULL;
					}
				}
			}
			break;

		default:
			word = NULL;

	}  /* switch */
	}  /* if  */


	if (word)
	{
		form->TextCursrY += lift;
		if (!scrl && edit_rowln (input, form->TextCursrY) < form->TextCursrX)
		{
			scrl = edit_rowln (input, form->TextCursrY) - form->TextCursrX;
		}
		if (lift > 0)
		{
			if (form->TextShiftY < form->TextCursrY - (WORD)(input->VisibleY -1))
			{
				form->TextShiftY = form->TextCursrY - (WORD)(input->VisibleY -1);
				line = 0;
			}
		}
		else
		{
			if (form->TextShiftY > form->TextCursrY)
			{
				form->TextShiftY = form->TextCursrY;
				line = 0;
			}
			else if (form->TextShiftY &&
			           input->TextRows < form->TextShiftY + input->VisibleY)
			{
				form->TextShiftY = input->TextRows - (WORD)input->VisibleY;
				line = 0;
			}
		}
		form->TextCursrX += scrl;
		if (scrl > 0)
		{
			if (form->TextShiftX < form->TextCursrX - (WORD)input->VisibleX)
			{
				form->TextShiftX = form->TextCursrX - (WORD)input->VisibleX;
				line = 0;
			}
		}
		else
		{
			if (form->TextShiftX > form->TextCursrX)
			{
				form->TextShiftX = form->TextCursrX;
				line = 0;
			}
			else if (form->TextShiftX)
			{
				WORD n = edit_rowln (input, form->TextCursrY);
				if (n < form->TextShiftX + input->VisibleX)
				{
					form->TextShiftX = n - (WORD)input->VisibleX;
					line = 0;
				}
			}
		}
/*printf ("%i %i %i\n", form->TextShiftX, form->TextCursrX, input->VisibleX);*/
		rect->g_x = 2;
		rect->g_y = 2 - word->word_height;
		rect->g_w = word->word_width -4;
		if (!line)
		{
			rect->g_h = word->word_height + word->word_tail_drop -4;
		}
		else
		{
			WORD row = form->TextCursrY - form->TextShiftY;
			if (line < 0)
			{
				row -= 1;
				line = 2;
			}
			rect->g_y += input->CursorH * row;
			rect->g_h =  input->CursorH * line +1;
		}
	}
	return word;
}


/*******************************************************************************
 *
 * Edit field functions
 *
 *   ||W0|W1|W2|...|\0||...(free)...|| L0 | L1 | L2 |...|Ln-1| Ln ||
*/

/*----------------------------------------------------------------------------*/
static BOOL
edit_grow (INPUT input)
{
	size_t  o_num = (WCHAR*)&input->TextArray[0] - (WCHAR*)input->Word->item;
	size_t  n_num = o_num +500;
	WORD    rows  = input->TextRows +1;
	size_t  size  = (n_num * sizeof(WCHAR) + rows * sizeof(WCHAR*) +3) & ~3uL;
	WCHAR * buff;

	if (input->TextMax) {
		return FALSE;
	}
	if (input->Value) {
		char * mem = malloc (n_num +1);
		if (!mem) {
			return FALSE;
		}
		memcpy (mem, input->Value, o_num +1);
		free (input->Value);
		input->Value = mem;
	}
	if ((buff = malloc (size)) != NULL) {
		WCHAR ** arr = (WCHAR**)((char*)buff + size) - rows;
		WORD     n   = 0;
		while (n < rows) {
			arr[n] = &buff[input->TextArray[n] - input->Word->item];
			n++;
		}
		memcpy (buff, input->Word->item, o_num * sizeof(WCHAR));
		free (input->Word->item);
		input->Word->item = buff;
		input->TextArray  = arr;
	}
	return (buff != NULL);
}

/*----------------------------------------------------------------------------*/
static WCHAR *
edit_init (INPUT input, TEXTBUFF current, UWORD cols, UWORD rows, size_t size)
{
	WORDITEM word = current->word;
	TEXTATTR attr = word->attr;
	void   * buff;
	WORD     p[8], n;

	font_byType (pre_font, -1, -1, word);
	if (word->font->Base->Mapping != MAP_UNICODE) {
		vst_map_mode (vdi_handle, MAP_UNICODE);
	}
	for (n = 0; n < cols; current->text[n++] = NOBRK_UNI);
	vqt_f_extent16n (vdi_handle, word->item, cols, p);
	*(current->text++) = word->font->Base->SpaceCode;
	set_word (current, word->word_height, word->word_tail_drop, p[2] - p[0] +2);
	if (word->font->Base->Mapping != MAP_UNICODE) {
		vst_map_mode (vdi_handle, word->font->Base->Mapping);
	}
	current->word->attr = attr;

	size += 1;                 /* space for the trailing 0 */
	size *= sizeof(WCHAR);
	size =  (size +1) & ~1uL;  /* aligned to (WCHAR*) boundary */
	size += sizeof(WCHAR*) *2;
	if ((buff = malloc (size)) != NULL) {
		word->item       = buff;
		input->TextArray = (WCHAR**)((char*)buff + size) -1;

	} else { /* memory exhausted */
		input->TextMax  = 1;
		input->readonly = TRUE;
	}
	edit_zero (input);

	input->VisibleX = cols;
	input->VisibleY = rows;
	input->CursorH  = p[7] + p[1] -1;

	if (rows > 1) {
		word->word_height += (rows-1) * input->CursorH;
	}

	return buff;
}

/*----------------------------------------------------------------------------*/
static BOOL
edit_zero (INPUT input)
{
	BOOL ok;
	if (input->TextArray) {
		input->TextArray    += (WORD)input->TextRows -1;
		input->TextRows      = 1;
		input->TextArray[0]  = input->Word->item;
		input->TextArray[1]  = input->Word->item +1;
		input->Word->item[0] = '\0';
		ok = TRUE;
	} else {
		ok = FALSE;
	}
	if (input->Value) {
		input->Value[0] = '\0';
	}
	return ok;
}

/*----------------------------------------------------------------------------*/
static void
edit_feed (INPUT input, ENCODING encoding, const char * beg, const char * end)
{
	WCHAR ** line = input->TextArray +1;
	WCHAR *  ptr  = input->TextArray[0];
	BOOL     crlf = (input->Type == IT_TAREA);
	*line = ptr;
	while (beg < end) {
		if (ptr >= (WCHAR*)&input->TextArray[0] -1) {
			*line = ptr;
			if (!edit_grow (input)) {
				beg = end;
				break;
			}
			line = &input->TextArray[input->TextRows];
			ptr  = *line;
		}
		if (*beg == '\n' && crlf) {
			WORD col = ptr - *line;
			*line    = ptr;
			if (!edit_crlf (input, col, input->TextRows -1)) {
				beg = end;
				break;
			}
			line = &input->TextArray[input->TextRows];
			ptr  = *line;
			beg++;
		} else if (*beg < ' ') {
			beg++;
		} else if (*beg == '&') {
			WCHAR tmp[5];
			scan_namedchar (&beg, tmp, TRUE, MAP_UNICODE);
			*(ptr++) = *tmp;
		} else {
			*(ptr++) = *(beg++);
		}
	}
	*(ptr++) = '\0';
	*(line)  = ptr; /* behind the last line */

	if (input->Value) { /* password */
		char * val = input->Value;
		ptr = input->Word->item;
		while ((*(val++) = *(ptr)) != '\0') {
			*(ptr++) = '*';
		}
	}

	(void)encoding;
}

/*----------------------------------------------------------------------------*/
static BOOL
edit_crlf (INPUT input, WORD col, WORD row)
{
	WCHAR * dst, * end;
	WORD    n;

	if (edit_space (input) < (sizeof(WCHAR) + sizeof(WCHAR*)) / sizeof(WCHAR)
	    && !edit_grow (input)) {
		return FALSE;
	}

	input->TextArray--;
	input->TextRows++;
	for (n = 0; n <= row; n++) {
		input->TextArray[n] = input->TextArray[n+1];
	}
	input->TextArray[++row] += col;
	for (n = row; n <= input->TextRows; n++) {
		input->TextArray[n]++;
	}
	dst = input->TextArray[row] -1;
	end = input->TextArray[input->TextRows];
	while (--end > dst) {
		*(end) = *(end -1);
	}
	*(dst) = '\n';

	return TRUE;
}

/*----------------------------------------------------------------------------*/
static BOOL
edit_char (INPUT input, WORD col, WORD row, WORD chr)
{
	ENCODER_W encoder = encoder_word (ENCODING_ATARIST, MAP_UNICODE);
	const char  * ptr = &((char*)&chr)[1];
	BOOL ok;

	if (edit_space (input) > 0 || edit_grow (input)) {
		WCHAR ** line = input->TextArray;
		WORD  n;
		WCHAR uni[5];
		(*encoder)(&ptr, uni);

		if (input->Value) { /*password */
			UWORD  len = edit_rowln (input, row);
			char * end = input->Value + len;
			char * dst = input->Value + col;
			do {
				*(end +1) = *(end);
			} while (--end >= dst);
			*dst = *uni;
			input->Word->item[len]    = '*';
			input->Word->item[len +1] = '\0';

		} else {
			WCHAR * end = line[input->TextRows];
			WCHAR * dst = line[row] + col;
			while (--end >= dst) {
				end[1] = end[0];
			}
			*dst = *uni;
		}
		for (n = row +1; n <= input->TextRows; line[n++]++);
		ok = TRUE;

	} else { /* buffer full */
		ok = FALSE;
	}
	return ok;
}

/*----------------------------------------------------------------------------*/
static BOOL
edit_delc (INPUT input, WORD col, WORD row)
{
	WCHAR ** text = input->TextArray;
	WCHAR  * beg  = text[row] + col;
	WCHAR  * end  = text[input->TextRows];
	BOOL ok;

	if (beg < end -1) {
		WORD n;

		if (input->Value) { /*password */
			char * ptr = input->Value + col;
			do {
				*(ptr) = *(ptr +1);
			} while (*(ptr++));
			*(--end) = '\0';

		} else {
			if (beg >= text[row +1] -1) { /* at the end of this row, merge */
				n    =  row;               /* with the following one        */
				text += row +2;
				for (n = row; n >= 0; *(--text) = input->TextArray[n--]);
				input->TextArray++;
				input->TextRows--;
			}
			do {
				*(beg) = *(beg +1);
			} while (*(beg++));
		}
		for (n = row; n < input->TextRows; text[++n]--);
		ok = TRUE;

	} else {
		ok = FALSE;
	}
	return ok;
}