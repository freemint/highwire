#ifndef __FORM_H__
#define __FORM_H__


void * new_form (FRAME, char * target, char * action, const char * method);

INPUT new_input (PARSER);

BOOL input_isEdit (INPUT);
void input_draw   (INPUT, WORD x, WORD y);

void input_disable  (INPUT, BOOL onNoff);
WORD input_handle   (INPUT, GRECT *);
BOOL input_activate (INPUT);

INPUT form_check (TEXTBUFF, const char * name, char * value, BOOL checkd);
INPUT form_radio (TEXTBUFF, const char * name, const char * value, BOOL checkd);
INPUT form_buttn (TEXTBUFF, const char * name, const char * value,
                  ENCODING encoding, char sub_type);
INPUT form_text  (TEXTBUFF, const char * name, char * value, UWORD maxlen,
                  ENCODING encoding, UWORD cols);


#endif /* __FORM_H__ */
