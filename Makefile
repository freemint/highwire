#
# Makefile for highwire
#
TARGET = highwire.app

# compiler settings

CROSS = m68k-atari-mint-
CC = $(CROSS)gcc -g #-DDEBUG
AS = $(CC) -c
LD = $(CC) 
CP = cp
RM = rm -f

#CPU = 68000
#CPU = 68030
CPU = 68040
#CPU = 68020-60
#CPU = 5475

DEFS = -DUSE_OVL -DUSE_INET -DLIBPNG -DLIBGIF -DLIBJPG
OPTFLAGS = -funsigned-char \
       -fomit-frame-pointer -O2 -fstrength-reduce 

ifeq ($(CPU),5475)
	OPTS = $(CPU:%=-mcpu=%) $(OPTFLAGS)
else
	OPTS = $(CPU:%=-m%) $(OPTFLAGS)
endif

DISABLED_WARNINGS = -Wno-deprecated-declarations
WARN = \
	-Wall \
	-Wmissing-prototypes \
	-Wshadow \
	-Wpointer-arith \
	-Wcast-qual \
	$(DISABLED_WARNINGS) \
	-Werror 


INCLUDE = 

hash = \#
CHECKGIF := $(shell if echo -e "$(hash)include <gif_lib.h> \\nconst char *version = GIF_LIB_VERSION" | $(CC) -E - | grep GIF_LIB_VERSION >/dev/null; then echo -lgif; else echo -lungif; fi)

CFLAGS = $(INCLUDE) $(WARN) $(OPTS) $(DEFS)
ASFLAGS = $(OPTS)
LDFLAGS = -s
LIBS = -lgem -lcflib -liio $(CHECKGIF) -ljpeg -lpng -lz -lm \
       #-lsocket

ifeq ($(CPU),5475)
        OBJDIR = obj.$(CPU)
else
	OBJDIR = obj$(CPU:68%=.%)
endif

all: $(TARGET)

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
	clipbrd.c \
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
	$(LD) -o $@ -Wl,-stack,128k -Wl,--mprg-flags=0x17 $(CFLAGS) $(LDFLAGS) $(OBJS) $(LIBS)

000: ; $(MAKE) CPU=68000
030: ; $(MAKE) CPU=68030
040: ; $(MAKE) CPU=68040
060: ; $(MAKE) CPU=68020-60
v4e: ; $(MAKE) CPU=5475

clean:
	rm -Rf *.bak */*.bak */*/*.bak *[%~] */*[%~] */*/*[%~]
	rm -Rf obj.* */obj.* */*/obj.* .deps */.deps */*/.deps *.o */*/*.o
	rm -Rf *.app *.[gt]tp *.prg modules/mintnet.ovl

distclean: clean


#
# distribution/snapshot archive
#
DISTDIR=dist
dist::
	$(MAKE) clean
	$(MAKE) CPU=68000
	mkdir -p $(DISTDIR)
	mv highwire.app $(DISTDIR)/highwire.000
	$(MAKE) CPU=68030
	mv highwire.app $(DISTDIR)/highwire.030
	$(MAKE) CPU=68040
	mv highwire.app $(DISTDIR)/highwire.040
	$(MAKE) CPU=68020-60
	mv highwire.app $(DISTDIR)/highwire.060
	$(MAKE) CPU=5475
	mv highwire.app $(DISTDIR)/highwire.v4e
	cp -a deskicon.rsc highwire.rsc $(DISTDIR)
	mkdir -p $(DISTDIR)/doc
	cp -a doc/HIGHWIRE.DOC doc/hotkeys.txt $(DISTDIR)/doc
	cp -a Change.Log $(DISTDIR)
	mkdir -p $(DISTDIR)/html
	cp -pvr html/. $(DISTDIR)/html
	mkdir -p $(DISTDIR)/modules
	$(MAKE) -C modules/network.src clean
	$(MAKE) -C modules/network.src CPU=5475
	cp -a modules/mintnet.ovl $(DISTDIR)/modules/mintnet.v4e
	$(MAKE) -C modules/network.src CPU=68000
	cp -a modules/mintnet.ovl $(DISTDIR)/modules
	cp -a modules/README.TXT modules/iconnect.ovl modules/magicnet.ovl modules/stik2.ovl modules/sting.ovl $(DISTDIR)/modules
	mkdir -p $(DISTDIR)/example.cfg
	cp -a example.cfg/highwire.cfg $(DISTDIR)/example.cfg
	-unix2dos $(DISTDIR)/doc/HIGHWIRE.DOC $(DISTDIR)/doc/hotkeys.txt $(DISTDIR)/modules/README.TXT $(DISTDIR)/Change.Log $(DISTDIR)/example.cfg
	(cwd=`pwd`; cd $(DISTDIR); zip -r "$$cwd"/hw`date +%y%m%d`.zip .)

#
# dependencies
#
DEPS_MAGIC := $(shell mkdir ./.deps > /dev/null 2>&1 || :)

-include $(DEPENDENCIES)
