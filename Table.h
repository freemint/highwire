/* @(#)highwire/Table.h
 */
#ifndef __TABLE_H__
#define __TABLE_H__


typedef struct s_table_row * TAB_ROW;
typedef struct s_table_cell* TAB_CELL;


struct s_table_stack {
	TABLE    Table;
	TAB_ROW  PrevRow;  /* previous row, to be copied from for rowspan */
	TAB_ROW  WorkRow;  /* actual row to work on */
	TAB_CELL WorkCell; /* actual cell to work on */
	unsigned NumCells; /* number of used cells in the actual row */
	H_ALIGN  AlignH;
	V_ALIGN  AlignV;
	PARSESUB SavedCurr;

	char _debug;
};

struct s_table {
	DOMBOX   Box;
	short    Spacing;
	short    Padding;
	TAB_ROW  Rows;
	unsigned NumCols;
	unsigned FixCols;
	unsigned NumRows;
	long   * Minimum;
	long   * Maximum;
	long   * Percent;
	long   * ColWidth;
};

struct s_table_row {
	short    Color;
	TAB_CELL Cells;
	TAB_ROW  NextRow;
	long     MinHeight;
	long     Height;
};

struct s_table_cell {
	DOMBOX    Box;
	V_ALIGN   AlignV;
	TAB_CELL  DummyFor;
	TAB_CELL  RightCell;
	TAB_CELL  BelowCell;
	short     ColSpan;
	short     RowSpan;

	char _debug;
};

void delete_table (TABLE*);

void table_start  (PARSER,   WORD color, H_ALIGN floating, WORD height,
                             WORD wdith, WORD spacng, WORD paddng, WORD border);
void table_row    (TEXTBUFF, WORD color, H_ALIGN, V_ALIGN, WORD height,
                             BOOL beginNend);
void table_cell   (PARSER,   WORD color, H_ALIGN, V_ALIGN, WORD height,
                             WORD wdith, UWORD rowspan, UWORD colspan);
void table_finish (PARSER);


#endif /* __TABLE_H__ */
