#ifndef __FORM_H__
#define __FORM_H__


void * new_form (FRAME, char * target, char * action, const char * method);
void form_finish (TEXTBUFF);

INPUT new_input (PARSER);

BOOL input_isEdit (INPUT);
void input_draw   (INPUT, WORD x, WORD y);

void     input_disable  (INPUT, BOOL onNoff);
WORD     input_handle   (INPUT, PXY, GRECT *, char ***);
WORDITEM input_activate (INPUT, WORD slct);
WORDITEM input_keybrd   (INPUT, WORD key, UWORD state, GRECT *, INPUT * next);

INPUT form_check (TEXTBUFF, const char * name, char * value, BOOL checkd);
INPUT form_radio (TEXTBUFF, const char * name, const char * value, BOOL checkd);
INPUT form_buttn (TEXTBUFF, const char * name, const char * value,
                  ENCODING, char sub_type);
INPUT form_text  (TEXTBUFF, const char * name, char * value, UWORD maxlen,
                  ENCODING, UWORD cols, BOOL readonly);

INPUT form_selct   (TEXTBUFF, const char * name, UWORD size, BOOL disabled);
INPUT selct_option (TEXTBUFF, const char * text, BOOL disabled,
                    ENCODING, char * value, BOOL selected);
void  selct_finish (TEXTBUFF);


#endif /* __FORM_H__ */
