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
	short    Backgnd;

	char _debug;
};

struct s_table {
	PARAGRPH Paragraph;
	short    Color;
	short    Border;
	short    Spacing;
	short    Padding;
	short    SetWidth;
	short    SetHeight;
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
	CONTENT   Content;
	V_ALIGN   AlignV;
	TAB_CELL  DummyFor;
	TAB_CELL  RightCell;
	TAB_CELL  BelowCell;
	short     ColSpan;
	short     RowSpan;
	OFFSET    Offset;

	char _debug;
};

void delete_table (TABLE*);

void table_start  (PARSER);
void table_row    (PARSER, BOOL beginNend);
void table_cell   (PARSER, BOOL is_head);
void table_finish (PARSER);

long      table_calc    (TABLE, long max_width);
long      table_draw    (TABLE, short x, long y,
                         const GRECT *, void * highlight);
CONTENT * table_content (TABLE, long x, long y, long area[4]);



#endif /* __TABLE_H__ */
