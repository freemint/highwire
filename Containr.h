/*
 * Containr.h -- Tree hierarchy to hold frames.
 *
 * A container is a polymorph struct which can either hold one or more children
 * containers or exactly one frame (or is empty).  This way a tree structure of
 * containers  represents the hierarchy as given by <FRAMESET> and <FRAME> tags
 * and  allowes  every  node (= container)  in the tree to have it's content be
 * replaced at any time.
 *
 * AltF4 - Feb. 17, 2002
 *
 */
#ifndef __CONTAINR_H__
#define __CONTAINR_H__


#ifdef __PUREC__
# ifdef CONTAINR
#  undef CONTAINR
#  undef HISTORY
# endif
typedef struct s_containr * CONTAINR;
typedef struct s_history  * HISTORY;
#endif


/*--- Events propagated by the library to registered handlers ---*/

typedef enum {
	HW_PageCleared,  /* A part of or the pane itsels lost it contents.  Any   *
	                  * pointer into the tree structure should be invalidated *
	                  * immediately                                           */
	HW_PageStarted,  /* A part of or the pane itsels starts loading new *
	                  * content.  'gen_ptr' points to a char* with the  *
	                  * location of the new contents.                   */
	HW_PageFinished, /* A part of or the pane itself is finished after loading *
	                  * and parsing.  'gen_ptr' points to the screen extents   *
	                  * or is NULL if no content exists.                       */
	HW_ImgBegLoad,   /* An image starts to be loaded.  'gen_ptr' points to a   *
	                  * char* of the image location.                           */
	HW_ImgEndLoad,   /* An image ended loading.  On success 'gen_ptr' points   *
	                  * to a rectangle of changed contents else it is NULL.    */
	HW_SetTitle,
	HW_SetInfo
} HW_EVENT;

typedef void (*CNTR_HDLR)(HW_EVENT, long arg, CONTAINR, const void * gen_ptr);
				/* Declaration of handlers for recieving events from the library
				 * which can be registered with a container.  The parameter 'arg' is
				 * the value that was given along with the handler function by the
				 * caller.  The container is this one where the event appeared and
				 * needn't to be the same that was initially created.  The gen_ptr
				 * argument holds some additional information depending of the type
				 * of event.
				*/


/*--- Declaration ---*/

struct s_containr {
	char * Name;   /* named location as given from the <FRAME NAME=".."> tag   */
	GRECT  Area;   /* position and size in absolute screen coordinates         */
	CONTAINR Base,     /* the anchestor of all containers in this tree or      *
	                    * itself if this is the base itself                    */
	         Parent,   /* structure pointer, back reference to it's linking    *
	                    * parent or NULL for a base container                  */
	         Sibling;  /* next container of the same parent or NULL            */
	
	short ColSize, RowSize; /* extent of the container, both values are either *
	                         * positive absolute values or negative fractions  *
	                         * of -1024                                        */
	void      * HighLight;
	BOOL        Border; /* as given from the <.. BORDER=".." tag   */
	SCROLL_CODE Scroll; /* as given from the <.. SCROLLING=".."tag */
		
	CNTR_HDLR Handler;
	long      HdlrArg;
	
	enum { CNT_EMPTY = 0, /* initial state                  */
	       CNT_FRAME = 1, /* u.Frame may point to a frame   */
	       CNT_CLD_H = 2, /* u.Child may point to a list of children,
	                       * horizontal ordered   */
	       CNT_CLD_V = 3  /* same as above but vertical ordered */
	} Mode;
	union { CONTAINR Child;
	        FRAME    Frame;
	} u;
};


/*--- Constructor/Destructor ---*/

CONTAINR new_containr    (CONTAINR parent);
					/* Create a new container structure with an initial mode of
					 * CNT_EMPTY.
					*/
void     delete_containr (CONTAINR *);
					/* Delete a container and all of it's children and substructures.
					*/


/*--- Modification of the tree structure ---*/

void   containr_fillup    (CONTAINR, const char * text, BOOL colsNrows);
					/* Fill an empty container with a number of children according
					 * to the comma separated list of 'text'.  If 'colsNrows' is TRUE
					 * the container's mode will be set to CNT_CLD_H and the children
					 * ColSizes will be calculated from the parameters in 'list'.
					 * Else the mode will be set to CNT_CLD_V and RowSizes are
					 * calculated.
					*/
void   containr_setup     (CONTAINR, FRAME, const char * anchor);
					/* Attache a frame to an empty container and change the
					 * container's mode to CNT_FRAME.  If anchor is not NULL the
					 * content will be shifted to this location.
					*/
void   containr_clear     (CONTAINR);
					/* Destroy all children and substructures of the container and
					 * reset it to the state CNT_EMPTY.  Only the structure pointers
					 * and the attributes Area and Name are preserved.
					*/
void   containr_register  (CONTAINR, CNTR_HDLR, long arg);
					/* Registers a handler function with a container.  Parents aren't
					 * affected but every later created children will inherit this
					 * values.
					*/
void * containr_highlight (CONTAINR, void * highlight);


/*--- History stuff ---*/

struct s_history {
	UWORD Length;
	char  Text[80]; /* maxium length of a GEM string */
};
#undef  HISTORY
#define HISTORY HISTORY

HISTORY history_create  (CONTAINR, const char * name, HISTORY prev);
					/* Creates a history item that is a snapshot of the complete
					 * container tree and all of it contents.  If prev is NULL a
					 * title string will be created from the name argument, else it
					 * will be taken from prev.
					*/
void    history_destroy (HISTORY*);
					/* Deletes a history item and frees all shared resources.
					*/

typedef struct {
	CONTAINR Target;
	LOCATION Location;
	ENCODING Encoding;
} HISTENTR; 

UWORD   containr_process (CONTAINR, HISTORY hist, HISTORY prev,
                          HISTENTR entr[], UWORD max_num);
					/* Re-creates a container tree from a history item.  If prev is
					 * not NULL only changed contents will be processed.  The entr
					 * array will be filled with frames which needs to get reloaded,
					 * but not more than given by max_num.
					 * The return value is the number of entries to be post-processed
					 * in the array.
					*/


/*--- Queries of attributes/characteristics (const methods) ---*/

#define  containr_Frame(cont) ((cont)->Mode == CNT_FRAME ?(cont)->u.Frame :NULL)
					/* Returns the frame of a container or NULL if no frame was
					 * attached.
					*/
#define  containr_Base(frame) (((CONTAINR)(frame)->Container)->Base)
					/* Returns the base of the frame's container hierarchy.  That is
					 * the parent of all other members and has no parent itself.
					*/
CONTAINR containr_byName  (CONTAINR, const char * name);
					/* Returns the first member of the given container's hierarchy
					 * which name matches the given string case insensitive or NULL
					 * if no match was found.
					*/
CONTAINR containr_byCoord (CONTAINR, short x, short y);
					/* Returns the member of the given container's hierarchy which
					 * includes the absolute coordinate [x/y] or NULL if the
					 * coordinate is outside of all.
					*/
BOOL     containr_Anchor  (CONTAINR, const char * anchor, long * dx, long * dy);
					/* Calculates the distances between the current container
					 * content's origin and and the location of an anchor and stores
					 * the results in dx and dy.  If one or both results are nonzero
					 * a TRUE will be returned.
					*/

/*--- Finding an element at a screen coordinate ---*/

#define PE_NONE      0x0000
#define PE_BORDER    0x0010
#define PE_BORDER_UP 0x0011
#define PE_BORDER_DN 0x0012
#define PE_BORDER_LF 0x0014
#define PE_BORDER_RT 0x0018
#define PE_EMPTY     0x0020
#define PE_FRAME     0x0030
#define PE_VBAR      0x0040
#define PE_HBAR      0x0050
#define PE_PARAGRPH  0x0080
#define PE_TEXT      0x0100
#define PE_IMAGE     0x0101
#define PE_TLINK     0x1100
#define PE_ILINK     0x1101
#define PE_INPUT     0x1200
#define PE_EDITABLE  0x1201
	
#define PE_Type(t)     (t & 0xFFF0)
#define PE_SubType(t)  (t & 0x000F)
#define PE_isActive(t) (t & 0x1000)
#define PE_isLink(t)  ((t & PE_ILINK) >= PE_TLINK)

UWORD containr_Element (CONTAINR *, short x, short y,
                        GRECT * watch, GRECT * clip, void **);


/*--- Modification of screen representation ---*/

BOOL containr_relocate  (CONTAINR, short x, short y);
					/* Set the absolute screen coordinate of a base container and
					 * adjust all children and substructures positions.
					*/
BOOL containr_calculate (CONTAINR, const GRECT *);
					/* Set the absolute screen coordinate and size of a base
					 * container and recalculate all children and substructure
					 * geometries.
					*/
BOOL containr_shift     (CONTAINR, long * dx, long * dy);
					/* Shifts the contents of the container with the distances dx
					 * horizontal and dy vertical.  The differences are aligned to
					 * the real extent of the contents and the results are stored
					 * into dx and dy.  If any of these both are nonzero the related
					 * slider value(s) will be updated and a TRUE returned.
					*/


/*--- Graphical output (const methods) ---*/

void containr_redraw (CONTAINR, const GRECT *);
					/* Redraw a container and all of it's children and substructures.
					 * If the GRECT argument isn't NULL it will be used as a clipping
					 * rectangle.
					*/
void containr_scroll (CONTAINR, const GRECT *, long dx, long dy);
					/* Scrolls the screen at the content's location by the distances
					 * aligned by the given rectangle.
					*/


/*--- Library internal ---*/

BOOL containr_notify (CONTAINR, HW_EVENT, const void * gen_ptr);


#endif /*__CONTAINR_H__*/
