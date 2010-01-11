#
# Makefile for highwire
#
TARGET = highwire.app

# compiler settings
CC = gcc -g #-DDEBUG
AS = $(CC) -c
LD = $(CC)
CP = cp
RM = rm -f

#CPU = 68000
#CPU = 68030
CPU = 68040
#CPU = 68020-60

DEFS = -DUSE_OVL -DUSE_INET

OPTS = $(CPU:%=-m%) -funsigned-char \
       -fomit-frame-pointer -O2 -fstrength-reduce

WARN = \
	-Wall \
	-Wmissing-prototypes \
	-Wshadow \
	-Wpointer-arith \
	-Wcast-qual -Werror

INCLUDE = -I/usr/GEM/include

CFLAGS = $(INCLUDE) $(WARN) $(OPTS) $(DEFS)
ASFLAGS = $(OPTS)
LDFLAGS =
LIBS = -L/usr/GEM/lib -lgem -lcflib -liio -lungif -ljpeg -lpng -lz -lm #-lsocket

OBJDIR = obj$(CPU:68%=.%)


#
# C source files
#
$(OBJDIR)/%.o: %.c
	@echo "$(CC) $(CFLAGS) -c $< -o $@"; \
	$(CC) -Wp,-MMD,.deps/$(<:.c=.P_) $(CFLAGS) -c $< -o $@
	@cat .deps/$(<:.c=.P_) \
	    | sed "s,^\(.*\)\.o:,$(OBJDIR)/\1.o:," > .deps/$(<:.c=.P)
	@rm -f .deps/$(<:.c=.P_)

#
# files
#
CFILES = \
	Logging.c \
	schedule.c \
	mime.c \
	ovl_sys.c \
	inet.c \
	http.c \
	cache.c \
	Location.c \
	cookie.c \
	DomBox.c \
	O_Struct.c \
	fontbase.c \
	W_Struct.c \
	raster.c \
	image.c \
	img-dcdr.c \
	Paragrph.c \
	list.c \
	Form.c \
	Table.c \
	Frame.c \
	color.c \
	encoding.c \
	scanner.c \
	parser.c \
	p_about.c \
	p_dir.c \
	render.c \
	Containr.c \
	Loader.c \
	Redraws.c \
	\
	Window.c \
	formwind.c \
	fntsetup.c \
	dl_mngr.c \
	Widget.c \
	hwWind.c \
	av_prot.c \
	dragdrop.c \
	olga.c \
	config.c \
	bookmark.c \
	Variable.c \
	Nice_VDI.c \
	keyinput.c \
	Mouse_R.c \
	AEI.c \
	HighWire.c \
	strtools.c \
#	mem-diag.c

HDR = hwWind.h Loader.h Containr.h Table.h Location.h Logging.h Form.h

SFILES =

OBJS = $(SFILES:%.s=$(OBJDIR)/%.o) $(CFILES:%.c=$(OBJDIR)/%.o)
OBJS_MAGIC := $(shell mkdir ./$(OBJDIR) > /dev/null 2>&1 || :)

DEPENDENCIES = $(addprefix ./.deps/, $(patsubst %.c,%.P,$(CFILES)))


$(TARGET): $(OBJS)
	$(LD) -o $@ $(CFLAGS) $(OBJS) $(LIBS)
	stack --fix=128k $@

000: ; $(MAKE) CPU=68000
030: ; $(MAKE) CPU=68030
040: ; $(MAKE) CPU=68040

clean:
	rm -Rf *.bak */*.bak */*/*.bak *[%~] */*[%~] */*/*[%~]
	rm -Rf obj.* */obj.* */*/obj.* .deps */.deps */*/.deps *.o */*/*.o
	rm -Rf *.app *.[gt]tp *.prg *.ovl */*.ovl

distclean: clean
	rm -Rf .cvsignore */.cvsignore */*/.cvsignore CVS */CVS */*/CVS


#
# adjust file names
#
$(CFILES) $(HDR) Makefile: ; mv `echo $@ | tr A-Z a-z` $@
case: Makefile $(HDR) $(CFILES)


#
# dependencies
#
DEPS_MAGIC := $(shell mkdir ./.deps > /dev/null 2>&1 || :)

-include $(DEPENDENCIES)
