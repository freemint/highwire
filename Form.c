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
#include "Location.h"
#include "parser.h"
#include "Form.h"


typedef struct s_select   * SELECT;
typedef struct s_slctitem * SLCTITEM;

struct s_form {
	FRAME       Frame;
	FORM        Next;
	char      * Target;
	char      * Action;
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
	IT_TEXT
} INP_TYPE;

struct s_input {
	INPUT    Next;
	union {
		void * Void;
		INPUT  Group; /* for radio buttons this points to the group node */
		SELECT Select;
	}        u;
	WORDITEM Word;
	PARAGRPH Paragraph;
	FORM     Form;
	char   * Value;
	INP_TYPE Type;
	char     SubType; /* [S]ubmit, [R]eset, \0 */
	BOOL     checked;
	BOOL     disabled;
	BOOL     readonly;
	WCHAR    HideChar;
	UWORD    TextMax;
	UWORD    TextLen;
	UWORD    VisibleX;
	UWORD    VisibleY;
	short    CursorH;
	UWORD    TextRows;
	WCHAR ** TextArray;
	char     Name[1];
};

struct s_select {
	SLCTITEM ItemList;
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

static WCHAR * edit_init (INPUT, TEXTBUFF, UWORD cols, size_t size);
static BOOL    edit_zero (INPUT);
static void    edit_feed (INPUT, ENCODING, const char * beg, const char * end);
static BOOL    edit_char (INPUT, WORD chr, WORD col);
static BOOL    edit_delc (INPUT, WORD col);


/*============================================================================*/
void *
new_form (FRAME frame, char * target, char * action, const char * method)
{
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
	
	form->Target = target;
	form->Action = action;
	form->Method = (method && stricmp (method, "post") == 0
	                ? METH_POST : METH_GET);
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
	input->HideChar  = 0;
	input->TextMax   = 0;
	input->TextLen   = 0;
	input->VisibleX  = 0;
	input->VisibleY  = 0;
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
form_text (TEXTBUFF current, const char * name, char * value, UWORD maxlen,
          ENCODING encoding, UWORD cols, BOOL readonly, BOOL is_pwd)
{
	INPUT input = _alloc (IT_TEXT, current, name);
	
	input->TextMax  = maxlen;
	input->readonly = readonly;
	if (edit_init (input, current, cols, maxlen)) {
		if (is_pwd) { /* == "PASSWORD" */
			input->Value    = value;
			input->HideChar = '*';
		}
		edit_feed (input, encoding, value, strchr (value, '\0'));
		if (!input->Value && value) {
			free (value);
		}
	} else {
		input->Value = value;
	}
	input->TextLen = input->TextArray[input->TextRows] - input->TextArray[0] -1;
	
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
		if (!mlen || mlen > 500) mlen = 500;
		if      (cols > mlen) cols = mlen;
		else if (!cols)       cols = 20;
		get_value (parser, KEY_VALUE, value = malloc (mlen +1), mlen +1);
		input = form_text (current, name, value, mlen, frame->Encoding, cols,
		                   get_value_exists (parser, KEY_READONLY),
		                   (toupper (*output) == 'P'));
		
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
		input->disabled = TRUE;
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
		BOOL fmap = (word->font->Base->Mapping != MAP_UNICODE);
		FORM form = input->Form;
		WORD len  = min (input->TextLen, input->VisibleX);
		PXY  pos;
		pos.p_x = x +3;
		pos.p_y = y;
		if (fmap) {
			vst_map_mode (vdi_handle, MAP_UNICODE);
		}
		if (input != form->TextActive) {
			v_ftext16n (vdi_handle, pos, word->item, len);
		} else {
			v_ftext16n (vdi_handle, pos, word->item + form->TextShiftX, len);
			vqt_f_extent16n (vdi_handle, word->item + form->TextShiftX,
			                 form->TextCursrX - form->TextShiftX, (short*)p);
			p[0].p_x = x +2 + p[1].p_x - p[0].p_x;
			p[1].p_x = p[0].p_x +1;
			p[0].p_y = y - word->word_height    +2;
			p[1].p_y = y + word->word_tail_drop -2;
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
		DOMBOX * box = &check->Paragraph->Box;
		long     x   = box->Rect.X;
		long     y   = box->Rect.Y;
		while ((box = box->Parent) != NULL) {
			x += box->Rect.X;
			y += box->Rect.Y;
		}
		box = &input->Paragraph->Box;
		x  -= box->Rect.X;
		y  -= box->Rect.Y;
		while ((box = box->Parent) != NULL) {
			x -= box->Rect.X;
			y -= box->Rect.Y;
		}
		rect->g_x += x;
		rect->g_y += y;
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
		
		case IT_TEXT: {
			FORM form = input->Form;
			if (form->TextActive && form->TextActive != input) {
				coord_diff (form->TextActive, input, radio);
				rtn = 2;
			} else {
				rtn = 1;
			}
			form->TextActive = input;
			form->TextCursrY = 0;
			form->TextShiftX = 0;
			form->TextShiftY = 0;
			if (mxy.p_x > 0 && input->TextLen > 0) {
				form->TextCursrX = mxy.p_x / (input->Word->font->SpaceWidth -1);
				if (form->TextCursrX > input->VisibleX) {
					 form->TextCursrX = input->VisibleX;
				}
				if (form->TextCursrX > input->TextLen) {
					 form->TextCursrX = input->TextLen;
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
				WORDITEM word   = input->Word;
				DOMBOX * box = &input->Paragraph->Box;
				long     x   = word->h_offset;
				long     y   = word->line->OffsetY;
				do {
					x += box->Rect.X;
					y += box->Rect.Y;
				} while ((box = box->Parent) != NULL);
				((long*)radio)[0] = x;
				((long*)radio)[1] = y + word->word_tail_drop -1;
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
static void
form_activate (FORM form)
{
	FRAME  frame = form->Frame;
	INPUT  elem  = form->InputList;
	size_t size  = 0, len;
	char * data, * url;
	
	if (elem) {
		do if (elem->checked && *elem->Name) {
			size += 2 + strlen (elem->Name);
			if (elem->Value) {
				char * v = elem->Value, c;
				while ((c = *(v++)) != '\0') {
					size += (c == ' ' || isalnum (c) ? 1 : 3);
				}
			} else {
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
			} else {
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
		start_page_load (frame->Container, url, frame->Location, TRUE, NULL);
		free (url);
	} else {
		LOADER ldr = start_page_load (frame->Container, url, frame->Location,
		                              TRUE, data);
		if (!ldr) free (data);
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
	WCHAR  * last = text[input->TextRows];
	WORD     asc  = key & 0xFF;
	WORD     scrl = 0;
	
	if (input != (*next = form->TextActive)) {
		return NULL;   /* shouldn't happen but who knows... */
	}
	
/*printf ("%04X %04X\n", key, state);*/
	if (asc >= ' ' && asc != 127 &&
	    (asc < '0' || asc > '9' || !(state & (K_RSHIFT|K_LSHIFT)))) {
		if (!input->readonly && edit_char (input, asc, form->TextCursrX)) {
			input->TextLen++;
			scrl = +1;
		} else {
			word = NULL;
		}
		
	} else if (asc) switch (asc) {
			
		case 27: /* escape */
			if (!input->readonly && text[0] < last -1 && edit_zero (input)) {
				form->TextCursrX = 0;
				form->TextCursrY = 0;
				form->TextShiftX = 0;
				form->TextShiftY = 0;
			} else {
				word = NULL;
			}
			break;
		
		case 55: /* shift+home */
			if ((scrl = input->TextLen - form->TextCursrX) == 0) {
				word = NULL;
			}
			break;
		
		case 13: /* enter/return */
			if (input->TextLen) {
				form_activate (form);
				form->TextActive = NULL;
			} else {
				word = NULL;
			}
			break;
		
		case 9: /* tab */
			if (state & K_CTRL) {
				form->TextActive = NULL;
				*next            = NULL;
			} else {
				INPUT srch = input->Next;
				if (srch) {
					while ((srch->disabled || srch->Type < IT_TEXT)
					       && (srch = srch->Next) != NULL);
				}
				if (!srch) {
					srch = form->InputList;
					while ((srch->disabled || srch->Type < IT_TEXT)
					       && (srch = srch->Next) != NULL);
				}
				*next = srch; 
			}
			break;
		
		case 8: /* backspace */
			if (input->readonly) {
				word = NULL;
			} else if (form->TextCursrX && edit_delc (input, form->TextCursrX-1)) {
				input->TextLen--;
				scrl = -1;
			} else {
				word = NULL;
			}
			break;
		
		case 127: /* delete */
			if (input->readonly) {
				word = NULL;
			} else if (edit_delc (input, form->TextCursrX)) {
				input->TextLen--;
			} else {
				word = NULL;
			}
			break;
		
		default:
			word = NULL;
	
	} else switch (key >>8) {
			
		case 0x4B: /* left */
			if (form->TextCursrX) {
				scrl = -1;
			} else {
				word = NULL;
			}
			break;
		
		case 0x4D: /*right */
			if (form->TextCursrX < input->TextLen) {
				scrl = +1;
			} else {
				word = NULL;
			}
			break;
		
		case 0x47: /* home */
			if (form->TextCursrX) {
				scrl = -form->TextCursrX;
			} else {
				word = NULL;
			}
			break;
		
		case 0x4F: /* end */
			if ((scrl = input->TextLen - form->TextCursrX) == 0) {
				word = NULL;
			}
			break;
		
		default:
			word = NULL;
	}
	if (word) {
		if (scrl > 0) {
			form->TextCursrX += scrl;
			if (form->TextShiftX < form->TextCursrX - (WORD)input->VisibleX) {
				form->TextShiftX = form->TextCursrX - (WORD)input->VisibleX;
			}
		} else {
			form->TextCursrX += scrl;
			if (form->TextShiftX > form->TextCursrX) {
				form->TextShiftX = form->TextCursrX;
			} else if (input->TextLen < form->TextShiftX + input->VisibleX) {
				WORD n = input->TextLen - (WORD)input->VisibleX;
				form->TextShiftX = max (0, n);
			}
		}
/*printf ("%i %i %i\n", form->TextShiftX, form->TextCursrX, input->VisibleX);*/
		rect->g_x = 2;
		rect->g_y = 2 - word->word_height;
		rect->g_w = word->word_width                         -4;
		rect->g_h = word->word_height + word->word_tail_drop -4;
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
static WCHAR *
edit_init (INPUT input, TEXTBUFF current, UWORD cols, size_t size)
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
	size =  (size +3) & ~3uL;  /* aligned to (WCHAR*) boundary */
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
	input->VisibleY = 1;
	input->CursorH  = p[7] + p[1] -1;
	
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
	input->TextLen = 0;
	
	return ok;
}

/*----------------------------------------------------------------------------*/
static void
edit_feed (INPUT input, ENCODING encoding, const char * beg, const char * end)
{
	WCHAR ** line = input->TextArray +1;
	WCHAR *  ptr  = input->TextArray[0];
	while (beg < end) {
		if (*beg < ' ') {
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
	input->TextMax = (WCHAR*)&input->TextArray[0] - input->TextArray[0];
	
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
edit_char (INPUT input, WORD chr, WORD col)
{
	ENCODER_W encoder = encoder_word (ENCODING_ATARIST, MAP_UNICODE);
	const char  * ptr = &((char*)&chr)[1];
	BOOL ok;
	
	if ((WCHAR*)&input->TextArray[0] > input->TextArray[input->TextRows]) {
		WCHAR ** text = input->TextArray;
		WORD  n;
		WCHAR uni[5];
		(*encoder)(&ptr, uni);
		
		if (input->Value) { /*password */
			char * end = input->Value + input->TextLen;
			char * dst = input->Value + col;
			do {
				*(end +1) = *(end);
			} while (--end >= dst);
			*dst = *uni;
			input->Word->item[input->TextLen]    = '*';
			input->Word->item[input->TextLen +1] = '\0';
		
		} else {
			WCHAR * end = text[input->TextRows];
			WCHAR * dst = text[0] + col;
			do {
				*(end +1) = *(end);
			} while (--end >= dst);
			*dst = *uni;
		}
		for (n = 0; n < input->TextRows; text[++n]++);
		ok = TRUE;
	
	} else { /* buffer full */
		ok = FALSE;
	}
	return ok;
}

/*----------------------------------------------------------------------------*/
static BOOL
edit_delc (INPUT input, WORD col)
{
	WCHAR ** text = input->TextArray;
	WCHAR * w_beg = text[0];
	WCHAR * w_end = text[input->TextRows];
	BOOL ok;
	
	if (col < w_end - w_beg) {
		WORD n;
		
		if (input->Value) { /*password */
			char * ptr = input->Value + col;
			do {
				*(ptr) = *(ptr +1);
			} while (*(ptr++));
			*(--w_end) = '\0';
		
		} else {
			WCHAR * ptr = w_beg + col;
			do {
				*(ptr) = *(ptr +1);
			} while (*(ptr++));
		}
		for (n = 0; n < input->TextRows; text[++n]--);
		ok = TRUE;
	
	} else {
		ok = FALSE;
	}
	return ok;
}
