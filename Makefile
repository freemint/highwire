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
CPU = 68030
#CPU = 68040
#CPU = 68020-60

DEFS = -DLIBGIF -DUSE_INET

OPTS = $(CPU:%=-m%) -fomit-frame-pointer -funsigned-char \
       -O2 -fstrength-reduce

WARN = \
	-Wall \
	-Wmissing-prototypes \
	-Wshadow \
	-Wpointer-arith \
	-Wcast-qual

INCLUDE = -I/usr/GEM/include

CFLAGS = $(INCLUDE) $(WARN) $(OPTS) $(DEFS)
ASFLAGS = $(OPTS)
LDFLAGS = 
LIBS = -L/usr/GEM/lib -lgem -liio -lungif -lsocket

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
	HighWire.c \
	AEI.c \
	Mouse_R.c \
	keyinput.c \
	Nice_VDI.c \
	Variable.c \
	color.c \
	config.c \
	av_prot.c \
	\
	hwWind.c \
	Redraws.c \
	Loader.c \
	Containr.c \
	render.c \
	p_dir.c \
	p_about.c \
	parser.c \
	scanner.c \
	encoding.c \
	Frame.c \
	Table.c \
	Form.c \
	list.c \
	Paragrph.c \
	image.c \
	W_Struct.c \
	O_Struct.c \
	Location.c \
	cache.c \
	schedule.c \
	http.c inet.c \
	Logging.c \
	Widget.c \
	fontbase.c \
	ovl_sys.c

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
	$(RM) $(OBJS)

distclean:
	$(MAKE) clean;
	$(RM) *~


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
